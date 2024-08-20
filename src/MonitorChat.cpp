#include "MonitorChat.h"
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <cstring>

// Constructor de la clase MonitorChat
MonitorChat::MonitorChat(int puerto)
    : puerto(puerto), descriptorMonitor(-1) {}

// Método para iniciar el monitor
void MonitorChat::iniciar() {
    // Crear un socket para el monitor
    descriptorMonitor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptorMonitor == -1) {
        std::cerr << "Error al crear el socket del monitor.\n";
        return;
    }

    // Configurar la dirección del monitor
    sockaddr_in direccionMonitor;
    direccionMonitor.sin_family = AF_INET;
    direccionMonitor.sin_port = htons(puerto);
    direccionMonitor.sin_addr.s_addr = INADDR_ANY;

    // Asignar la dirección al socket
    if (bind(descriptorMonitor, (sockaddr*)&direccionMonitor, sizeof(direccionMonitor)) == -1) {
        std::cerr << "Error al hacer bind del socket del monitor.\n";
        return;
    }

    // Poner el monitor en modo escucha
    if (listen(descriptorMonitor, 10) == -1) {
        std::cerr << "Error al poner el monitor en modo escucha.\n";
        return;
    }

    std::cout << "Monitor iniciado en el puerto " << puerto << ". Esperando conexiones...\n";

    // Iniciar un hilo para leer la entrada de la consola
    hiloConsola = std::thread(&MonitorChat::leerEntradaConsola, this);

    // Bucle para aceptar conexiones de servidores
    while (true) {
        sockaddr_in direccionServidor;
        socklen_t tamanoDireccionServidor = sizeof(direccionServidor);
        int descriptorServidor = accept(descriptorMonitor, (sockaddr*)&direccionServidor, &tamanoDireccionServidor);

        if (descriptorServidor == -1) {
            std::cerr << "Error al aceptar la conexión de una clase.\n";
            continue;
        }

        // Agregar el descriptor del servidor a la lista de servidores conectados
        {
            std::lock_guard<std::mutex> lock(mutexServidores);
            servidoresConectados.push_back(descriptorServidor);
        }

        // Iniciar un hilo para manejar la comunicación con el servidor
        std::thread hiloServidor(&MonitorChat::manejarServidor, this, descriptorServidor);
        hiloServidor.detach();
    }
}

// Método para manejar la comunicación con un servidor conectado
void MonitorChat::manejarServidor(int descriptorServidor) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRecibidos = recv(descriptorServidor, buffer, 1024, 0);
        if (bytesRecibidos <= 0) {
            std::cerr << "Error o desconexión de la clase " << descriptorServidor << ".\n";
            
            // Eliminar el servidor de la lista si se desconecta
            std::lock_guard<std::mutex> lock(mutexServidores);
            servidoresConectados.erase(
                std::remove(servidoresConectados.begin(), servidoresConectados.end(), descriptorServidor),
                servidoresConectados.end()
            );
            close(descriptorServidor);
            break;
        }

        // Procesar y mostrar la respuesta del servidor
        {
            std::lock_guard<std::mutex> lock(mutexServidores);
            auto it = std::find(servidoresConectados.begin(), servidoresConectados.end(), descriptorServidor);
            int posicion = std::distance(servidoresConectados.begin(), it) + 1;

            std::cout << "Respuesta de la clase " << posicion << ": " << std::string(buffer, bytesRecibidos) << std::endl;
        }
    }
}

// Método para enviar un comando a todos los servidores conectados
void MonitorChat::enviarComando(const std::string& comando) {
    std::cout << "Comando: " << comando << std::endl;
    std::lock_guard<std::mutex> lock(mutexServidores); 
    for (int descriptor : servidoresConectados) {
        ssize_t bytesEnviados = send(descriptor, comando.c_str(), comando.size(), 0);
        if (bytesEnviados == -1) {
            std::cerr << "Error al enviar comando a la clase " << descriptor << ".\n";
        }
    }
}

// Método para leer la entrada de la consola y ejecutar comandos
void MonitorChat::leerEntradaConsola() {
    std::string comando;
    while (std::getline(std::cin, comando)) {
        if (comando == "@clientes_todo") {
            recibirYSumarClientes();
        } else if (comando == "@mensajes_todo") {
            recibirYsumarMensajes();
        } else {
            enviarComando(comando);
        }
    }
}

// Método para recibir y sumar el número total de clientes de todos los servidores
void MonitorChat::recibirYSumarClientes() {
    std::lock_guard<std::mutex> lock(mutexServidores);

    int totalClientes = 0;
    std::string comando = "@clientes_todo";  

    // Envía el comando a todos los servidores conectados
    for (int descriptor : servidoresConectados) {
        ssize_t bytesEnviados = send(descriptor, comando.c_str(), comando.size(), 0);
        if (bytesEnviados == -1) {
            std::cerr << "Error al enviar comando a la clase " << descriptor << ".\n";
            continue; 
        }
    }

    // Recibe la respuesta de cada servidor y suma el total de clientes
    for (int descriptor : servidoresConectados) {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        
        ssize_t bytesRecibidos = recv(descriptor, buffer, sizeof(buffer) - 1, 0); 
        if (bytesRecibidos > 0) {
            buffer[bytesRecibidos] = '\0'; 
            std::string respuesta(buffer);
            
            try {
                int clientes = std::stoi(respuesta);
                totalClientes += clientes;
            } catch (const std::exception& e) {
                std::cerr << "Error al procesar la respuesta de la clase " << descriptor << ": " << e.what() << std::endl;
            }
        } else if (bytesRecibidos == 0) {
            std::cerr << "La clase " << descriptor << " ha cerrado la conexión.\n";
        } else {
            std::cerr << "Error al recibir respuesta de la clase " << descriptor << ". Código de error: " << errno << std::endl;
        }
    }
    
    // Imprime el total de clientes de todos los servidores
    std::cout << "Total de estudiantes de todas las clases: " << totalClientes << std::endl;
}

// Método para recibir y sumar el número total de mensajes de todos los servidores
void MonitorChat::recibirYsumarMensajes() {
    std::lock_guard<std::mutex> lock(mutexServidores);

    int totalMensajes = 0;
    std::string comando = "@mensajes_todo";  // Comando para solicitar el total de mensajes

    // Enviar el comando a todos los servidores conectados
    for (int descriptor : servidoresConectados) {
        ssize_t bytesEnviados = send(descriptor, comando.c_str(), comando.size(), 0);
        if (bytesEnviados == -1) {
            std::cerr << "Error al enviar comando a la clase " << descriptor << ".\n";
            continue;
        }
    }

    // Recibir y sumar la respuesta de todos los servidores
    for (int descriptor : servidoresConectados) {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        
        ssize_t bytesRecibidos = recv(descriptor, buffer, sizeof(buffer) - 1, 0);
        if (bytesRecibidos > 0) {
            buffer[bytesRecibidos] = '\0';
            std::string respuesta(buffer, bytesRecibidos);

            try {
                int mensajes = std::stoi(respuesta);
                totalMensajes += mensajes;
            } catch (const std::exception& e) {
                std::cerr << "Error al procesar la respuesta de la clase " << descriptor << ": " << e.what() << std::endl;
            }
        } else if (bytesRecibidos == 0) {
            std::cerr << "La clase " << descriptor << " ha cerrado la conexión.\n";
        } else {
            std::cerr << "Error al recibir respuesta de la clase" << descriptor << ". Código de error: " << errno << std::endl;
        }
    }

    // Imprime el total de mensajes de todos los servidores
    std::cout << "El total de mensajes en todas las clases es de: " << totalMensajes << std::endl;
}
