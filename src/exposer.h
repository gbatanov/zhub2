#ifndef PROMSERVER_H
#define PROMSERVER_H

#include "app.h"

#ifndef MAX_TCP_SIZE
#define MAX_TCP_SIZE 4096
#endif

class Exposer
{
public:
    Exposer(std::string url, int port);
    ~Exposer();
    void start();
    int open_tcp_socket(int port);
void stop_exposer();
private:
    void receive_http(int clientSockfd);
    void send_answer(int clientSockfd, std::string answer);
    void send_error(int clientSockfd, int codeError, std::string response);
    std::string create_header(int status, std::string title, std::string extraHeader, off_t contentLength);
    std::string metrics();

    std::string url_{};
    int httpSockfd = -1;
    int httpServerPort = 8092;
    std::atomic<bool> flag;
};

#endif