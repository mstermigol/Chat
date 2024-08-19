#include <iostream>
#include "ClienteChat.h"
#include "ServidorChat.h"
#include "MonitorChat.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " <modo> <direccionIP> <puerto>\n";
        std::cerr << "Modos disponibles: servidor, cliente, monitor\n";
        return 1;
    }

    std::string modo = argv[1];

    if (modo == "servidor") {
        if (argc < 4) {
            std::cerr << "Uso: " << argv[0] << " servidor <puerto> <direccionIPMonitor> <puertoMonitor>\n";
            return 1;
        }
        int puerto = std::stoi(argv[2]);
        std::string direccionIPMonitor = argv[3];
        int puertoMonitor = std::stoi(argv[4]);

        ServidorChat servidor(puerto, direccionIPMonitor, puertoMonitor);  // Inicializa el servidor con el puerto y la conexión al monitor
        servidor.iniciar();  // Inicia el servidor

    } else if (modo == "cliente") {
        if (argc < 4) {
            std::cerr << "Uso: " << argv[0] << " cliente <direccionIP> <puerto>\n";
            return 1;
        }
        std::string direccionIP = argv[2];
        int puerto = std::stoi(argv[3]);

        ClienteChat cliente(direccionIP, puerto);  // Inicializa el cliente con la dirección IP y puerto proporcionados
        cliente.conectarAlServidor();  // Conecta al servidor

        std::string mensaje;
        while (std::getline(std::cin, mensaje)) {
            cliente.manejarComando(mensaje);  // Envía el mensaje al servidor
        }

        cliente.desconectar();  // Desconecta del servidor

    } else if (modo == "monitor") {
        if (argc < 3) {
            std::cerr << "Uso: " << argv[0] << " monitor <puerto>\n";
            return 1;
        }
        int puerto = std::stoi(argv[2]);

        MonitorChat monitor(puerto);  // Inicializa el monitor con el puerto proporcionado
        monitor.iniciar();

    } else {
        std::cerr << "Modo desconocido: " << modo << "\n";
        return 1;
    }

    return 0;
}
