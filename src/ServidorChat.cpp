#include "ServidorChat.h"
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <cstring>

// Constructor que inicializa el puerto del servidor y la conexión con el monitor
ServidorChat::ServidorChat(int puerto, const std::string& ipMonitor, int puertoMonitor)
    : puerto(puerto), puertoMonitor(puertoMonitor), direccionIPMonitor(ipMonitor), descriptorServidor(-1), descriptorMonitor(-1) {}

// Método para iniciar el servidor
void ServidorChat::iniciar() {
    // Conectar con el monitor
    conectarConMonitor();

    // Crear el socket del servidor
    descriptorServidor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptorServidor == -1) {
        std::cerr << "Error al crear el socket del servidor.\n";
        return;
    }

    sockaddr_in direccionServidor;
    direccionServidor.sin_family = AF_INET;
    direccionServidor.sin_port = htons(puerto);
    direccionServidor.sin_addr.s_addr = INADDR_ANY;

    // Asociar el socket a la dirección y puerto
    if (bind(descriptorServidor, (sockaddr*)&direccionServidor, sizeof(direccionServidor)) == -1) {
        std::cerr << "Error al hacer bind del socket del servidor.\n";
        return;
    }

    // Poner el servidor en modo escucha
    if (listen(descriptorServidor, 10) == -1) {
        std::cerr << "Error al poner el servidor en modo escucha.\n";
        return;
    }

    std::cout << "Servidor iniciado en el puerto " << puerto << ". Esperando conexiones...\n";

    // Iniciar el hilo del monitor
    hiloMonitor = std::thread(&ServidorChat::manejarComandosMonitor, this);

    // Aceptar conexiones entrantes
    while (true) {
        sockaddr_in direccionCliente;
        socklen_t tamanoDireccionCliente = sizeof(direccionCliente);
        int descriptorCliente = accept(descriptorServidor, (sockaddr*)&direccionCliente, &tamanoDireccionCliente);

        if (descriptorCliente == -1) {
            std::cerr << "Error al aceptar la conexión de un cliente.\n";
            continue;
        }

        // Crear un hilo para manejar el cliente
        std::thread hiloCliente(&ServidorChat::manejarCliente, this, descriptorCliente);
        hiloCliente.detach();
    }
}

// Conectar con el monitor
void ServidorChat::conectarConMonitor() {
    descriptorMonitor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptorMonitor == -1) {
        std::cerr << "Error al crear el socket del monitor.\n";
        return;
    }

    sockaddr_in direccionMonitor;
    direccionMonitor.sin_family = AF_INET;
    direccionMonitor.sin_port = htons(puertoMonitor);
    inet_pton(AF_INET, direccionIPMonitor.c_str(), &direccionMonitor.sin_addr);

    if (connect(descriptorMonitor, (sockaddr*)&direccionMonitor, sizeof(direccionMonitor)) == -1) {
        std::cerr << "Error al conectar con el monitor.\n";
        return;
    }

    std::cout << "Conectado al monitor en " << direccionIPMonitor << ":" << puertoMonitor << ".\n";
}

// Manejar comandos del monitor
void ServidorChat::manejarComandosMonitor() {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRecibidos = recv(descriptorMonitor, buffer, 1024, 0);

        if (bytesRecibidos <= 0) {
            std::cerr << "Error o desconexión del monitor.\n";
            close(descriptorMonitor);
            return;
        }

        std::string comando(buffer, bytesRecibidos);

        if (comando == "num-clientes") {
            std::string respuesta = std::to_string(usuarios.size()) + "\n";
            send(descriptorMonitor, respuesta.c_str(), respuesta.size(), 0);
        } else {
            std::cerr << "Comando desconocido del monitor: " << comando << "\n";
        }
    }
}

// Manejar la comunicación con un cliente
void ServidorChat::manejarCliente(int descriptorCliente) {
    char buffer[1024];
    std::string nombreUsuario;

    // Solicitar el nombre del usuario
    send(descriptorCliente, "Ingrese su nombre: ", 20, 0);
    ssize_t bytesRecibidos = recv(descriptorCliente, buffer, 1024, 0);
    if (bytesRecibidos <= 0) {
        close(descriptorCliente);
        return;
    }

    nombreUsuario = std::string(buffer, bytesRecibidos);
    nombreUsuario.erase(nombreUsuario.find_last_not_of(" \n\r\t") + 1); // Eliminar espacios en blanco

    {
        std::lock_guard<std::mutex> lock(mutexUsuarios);
        usuarios.emplace_back(nombreUsuario, descriptorCliente);
    }

    // Notificar a todos los usuarios que un nuevo usuario se ha conectado
    std::string mensajeBienvenida = nombreUsuario + " se ha conectado al chat.\n";
    enviarMensajeATodos(mensajeBienvenida, descriptorCliente);

    // Manejar los mensajes del cliente
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        bytesRecibidos = recv(descriptorCliente, buffer, 1024, 0);

        if (bytesRecibidos <= 0) {
            // El cliente se ha desconectado
            {
                std::lock_guard<std::mutex> lock(mutexUsuarios);
                usuarios.erase(
                    std::remove_if(usuarios.begin(), usuarios.end(),
                                    [descriptorCliente](const Usuario& usuario) {
                                        return usuario.obtenerDescriptorSocket() == descriptorCliente;
                                    }),
                    usuarios.end()
                );
            }

            std::string mensajeDesconexion = nombreUsuario + " se ha desconectado del chat.\n";
            enviarMensajeATodos(mensajeDesconexion, descriptorCliente);

            // Cerrar el socket del cliente
            close(descriptorCliente);
            break;
        }

        std::string mensaje = std::string(buffer, bytesRecibidos);

        // Procesar comandos del protocolo
        if (mensaje.substr(0, 9) == "@usuarios") {
            enviarListaUsuarios(descriptorCliente);
        } else if (mensaje.substr(0, 9) == "@conexion") {
            enviarDetallesConexion(descriptorCliente);
        } else if (mensaje.substr(0, 6) == "@salir") {
            // Manejar el comando @salir
            {
                std::lock_guard<std::mutex> lock(mutexUsuarios);
                usuarios.erase(
                    std::remove_if(usuarios.begin(), usuarios.end(),
                                    [descriptorCliente](const Usuario& usuario) {
                                        return usuario.obtenerDescriptorSocket() == descriptorCliente;
                                    }),
                    usuarios.end()
                );
            }

            std::string mensajeDesconexion = nombreUsuario + " se ha desconectado del chat.\n";
            enviarMensajeATodos(mensajeDesconexion, descriptorCliente);

            // Cerrar el socket del cliente
            close(descriptorCliente);
            return;
        } else if (mensaje.substr(0, 2) == "@h") {
            std::string ayuda = "Comandos disponibles:\n"
                                "@usuarios - Lista de usuarios conectados\n"
                                "@conexion - Muestra la conexión y el número de usuarios\n"
                                "@salir - Desconectar del chat\n";
            send(descriptorCliente, ayuda.c_str(), ayuda.size(), 0);
        } else {
            // Enviar el mensaje a todos los usuarios
            mensaje = nombreUsuario + ": " + mensaje;
            enviarMensajeATodos(mensaje, descriptorCliente);
        }
    }
}
// Enviar un mensaje a todos los usuarios conectados, excepto al remitente
void ServidorChat::enviarMensajeATodos(const std::string& mensaje, int descriptorRemitente) {
    std::lock_guard<std::mutex> lock(mutexUsuarios);
    for (const auto& usuario : usuarios) {
        if (usuario.obtenerDescriptorSocket() != descriptorRemitente) {
            send(usuario.obtenerDescriptorSocket(), mensaje.c_str(), mensaje.size(), 0);
        }
    }
}

// Enviar la lista de usuarios conectados al cliente especificado
void ServidorChat::enviarListaUsuarios(int descriptorCliente) {
    std::lock_guard<std::mutex> lock(mutexUsuarios);
    std::string listaUsuarios = "Usuarios conectados:\n";
    for (const auto& usuario : usuarios) {
        listaUsuarios += usuario.obtenerNombreUsuario() + "\n";
    }
    send(descriptorCliente, listaUsuarios.c_str(), listaUsuarios.size(), 0);
}

// Enviar los detalles de la conexión y el número de usuarios conectados
void ServidorChat::enviarDetallesConexion(int descriptorCliente) {
    std::lock_guard<std::mutex> lock(mutexUsuarios);
    std::string detalles = "Número de usuarios conectados: " + std::to_string(usuarios.size()) + "\n";
    send(descriptorCliente, detalles.c_str(), detalles.size(), 0);
}
