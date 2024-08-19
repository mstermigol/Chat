#ifndef SERVIDORCHAT_H
#define SERVIDORCHAT_H

#include "Usuario.h"
#include <string>
#include <vector>
#include <mutex>
#include <thread>

class ServidorChat {
public:
    ServidorChat(int puerto, const std::string& ipMonitor, int puertoMonitor);
    void iniciar();

private:
    void manejarCliente(int descriptorCliente);
    void enviarMensajeATodos(const std::string& mensaje, int descriptorRemitente);
    void enviarListaUsuarios(int descriptorCliente);
    void enviarDetallesConexion(int descriptorCliente);
    void conectarConMonitor();
    void manejarComandosMonitor();

    int puerto;  // Puerto en el que escucha el servidor
    int descriptorServidor;  // Descriptor del socket del servidor
    int descriptorMonitor;  // Descriptor del socket del monitor
    std::string direccionIPMonitor;
    int puertoMonitor;
    std::vector<Usuario> usuarios;  // Lista de usuarios conectados
    std::mutex mutexUsuarios;  // Mutex para proteger el acceso a la lista de usuarios
    std::thread hiloMonitor;  // Hilo para manejar la comunicación con el monitor
};

#endif // SERVIDORCHAT_H
