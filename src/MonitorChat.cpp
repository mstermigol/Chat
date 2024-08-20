#include "MonitorChat.h"
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <cstring>

MonitorChat::MonitorChat(int puerto)
    : puerto(puerto), descriptorMonitor(-1) {}

void MonitorChat::iniciar() {
    descriptorMonitor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptorMonitor == -1) {
        std::cerr << "Error al crear el socket del monitor.\n";
        return;
    }

    sockaddr_in direccionMonitor;
    direccionMonitor.sin_family = AF_INET;
    direccionMonitor.sin_port = htons(puerto);
    direccionMonitor.sin_addr.s_addr = INADDR_ANY;

    if (bind(descriptorMonitor, (sockaddr*)&direccionMonitor, sizeof(direccionMonitor)) == -1) {
        std::cerr << "Error al hacer bind del socket del monitor.\n";
        return;
    }

    if (listen(descriptorMonitor, 10) == -1) {
        std::cerr << "Error al poner el monitor en modo escucha.\n";
        return;
    }

    std::cout << "Monitor iniciado en el puerto " << puerto << ". Esperando conexiones...\n";

    hiloConsola = std::thread(&MonitorChat::leerEntradaConsola, this);

    while (true) {
        sockaddr_in direccionServidor;
        socklen_t tamanoDireccionServidor = sizeof(direccionServidor);
        int descriptorServidor = accept(descriptorMonitor, (sockaddr*)&direccionServidor, &tamanoDireccionServidor);

        if (descriptorServidor == -1) {
            std::cerr << "Error al aceptar la conexión de un servidor.\n";
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(mutexServidores);
            servidoresConectados.push_back(descriptorServidor);
        }

        std::thread hiloServidor(&MonitorChat::manejarServidor, this, descriptorServidor);
        hiloServidor.detach();
    }
}

void MonitorChat::manejarServidor(int descriptorServidor) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRecibidos = recv(descriptorServidor, buffer, 1024, 0);
        if (bytesRecibidos <= 0) {
            std::cerr << "Error o desconexión del servidor " << descriptorServidor << ".\n";
            
            std::lock_guard<std::mutex> lock(mutexServidores);
            // Remove the server descriptor from the list of connected servers
            servidoresConectados.erase(
                std::remove(servidoresConectados.begin(), servidoresConectados.end(), descriptorServidor),
                servidoresConectados.end()
            );
            close(descriptorServidor);
            break;
        }

        {
            std::lock_guard<std::mutex> lock(mutexServidores);
            // Get the position of the server in the list
            auto it = std::find(servidoresConectados.begin(), servidoresConectados.end(), descriptorServidor);
            int posicion = std::distance(servidoresConectados.begin(), it) + 1;

            std::cout << "Respuesta del servidor " << posicion << ": " << std::string(buffer, bytesRecibidos) << std::endl;
        }
    }
}

void MonitorChat::enviarComando(const std::string& comando) {
    std::cout << "Comando: " << comando << std::endl;
    std::lock_guard<std::mutex> lock(mutexServidores);  // Ensure thread safety
    for (int descriptor : servidoresConectados) {
        ssize_t bytesEnviados = send(descriptor, comando.c_str(), comando.size(), 0);
        if (bytesEnviados == -1) {
            std::cerr << "Error al enviar comando al servidor " << descriptor << ".\n";
        }
    }
}

void MonitorChat::leerEntradaConsola() {
    std::string comando;
    while (std::getline(std::cin, comando)) {
        if (comando == "@clientes_todo") {
            recibirYSumarClientes();
        } else {
            enviarComando(comando);
        }
    }
}

void MonitorChat::recibirYSumarClientes() {
    std::lock_guard<std::mutex> lock(mutexServidores);

    int totalClientes = 0;
    std::string comando = "@clientes_todo";  

    // Envía el comando a todos los servidores conectados
    for (int descriptor : servidoresConectados) {
        ssize_t bytesEnviados = send(descriptor, comando.c_str(), comando.size(), 0);
        if (bytesEnviados == -1) {
            std::cerr << "Error al enviar comando al servidor " << descriptor << ".\n";
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
                std::cerr << "Error al procesar la respuesta del servidor " << descriptor << ": " << e.what() << std::endl;
            }
        } else if (bytesRecibidos == 0) {
            std::cerr << "El servidor " << descriptor << " ha cerrado la conexión.\n";
        } else {
            std::cerr << "Error al recibir respuesta del servidor " << descriptor << ". Código de error: " << errno << std::endl;
        }
    }
    
    // Imprime solo el total de clientes de todos los servidores
    std::cout << "Total de clientes de todos los servidores: " << totalClientes << std::endl;
}
