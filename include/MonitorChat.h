#ifndef MONITORCHAT_H
#define MONITORCHAT_H

#include <string>
#include <vector>
#include <mutex>

class MonitorChat {
public:
    MonitorChat(int puerto);
    void iniciar();

private:
    void manejarServidor(int descriptorServidor);
    void enviarComando(int descriptorServidor, const std::string& comando);

    int puerto;
    int descriptorMonitor;
    std::vector<int> servidoresConectados;  // Lista de descriptores de servidores conectados
    std::mutex mutexServidores;  // Mutex para proteger el acceso a la lista de servidores conectados
};

#endif // MONITORCHAT_H
