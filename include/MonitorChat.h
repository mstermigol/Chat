#ifndef MONITORCHAT_H
#define MONITORCHAT_H

#include <string>
#include <vector>
#include <mutex>
#include <thread>

class MonitorChat {
public:
    MonitorChat(int puerto);  // Constructor que inicializa el monitor con un puerto específico
    void iniciar();  // Método para iniciar el monitor y esperar conexiones de servidores
    void enviarComando(const std::string& comando);  // Método para enviar un comando a todos los servidores conectados

private:
    void manejarServidor(int descriptorServidor);  // Maneja la comunicación con un servidor específico
    void leerEntradaConsola();  // Lee comandos desde la consola
    void recibirYSumarClientes();  // Recibe el número de clientes de todos los servidores y los suma
    void recibirYsumarMensajes();  // Recibe el número de mensajes de todos los servidores y los suma

    int puerto;  // Puerto en el que el monitor escucha conexiones
    int descriptorMonitor;  // Descriptor del socket del monitor
    std::vector<int> servidoresConectados;  // Lista de descriptores de servidores conectados
    std::mutex mutexServidores;  // Mutex para proteger el acceso a la lista de servidores conectados
    std::thread hiloConsola;  // Hilo para manejar la lectura de la entrada de consola
};

#endif // MONITORCHAT_H
