#ifndef MONITORCHAT_H
#define MONITORCHAT_H

#include <string>
#include <vector>
#include <mutex>
#include <thread>

class MonitorChat {
public:
    MonitorChat(int puerto);
    void iniciar();
    void enviarComando(const std::string& comando);

private:
    void manejarServidor(int descriptorServidor);
    void leerEntradaConsola(); 

    int puerto;
    int descriptorMonitor;
    std::vector<int> servidoresConectados;  // Lista de descriptores de servidores conectados
    std::mutex mutexServidores;  // Mutex para proteger el acceso a la lista de servidores conectados
    std::thread hiloConsola; 
};

#endif // MONITORCHAT_H
