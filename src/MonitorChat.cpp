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
        std::cout << "Respuesta del servidor: " << std::string(buffer, bytesRecibidos) << std::endl;
    }
}

void MonitorChat::enviarComando(int descriptorServidor, const std::string& comando) {
    send(descriptorServidor, comando.c_str(), comando.size(), 0);
}
