// web-server для  prometheus
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <thread>
#include <atomic>
#include <cstring>
#include <vector>
#include <queue>
#include <ctime>
#include <mutex>
#include <map>
#include <array>
#include <set>
#include <memory>
#include <optional>
#include <any>
#include <sstream>
#include <termios.h>
#include <string>

#include "version.h"
#include "../gsb_utils/gsbutils.h"
#include "app.h"
#include "exposer.h"

extern std::shared_ptr<App> app;

Exposer::Exposer(std::string url, int port)
{
    url_ = std::move(url);
    if (port > 0)
        httpServerPort = port;
}
Exposer::~Exposer()
{
    flag.store(false);
}

void Exposer::stop_exposer()
{
    flag.store(false);
}
void Exposer::start()
{

    flag.store(true);
    // Попытка открыть TCP socket для HTTP-сервер
    httpSockfd = open_tcp_socket(httpServerPort);
    if (httpSockfd < 0)
    {
        gsbutils::dprintf(1, (char *)"Exposer: ServerSocket is not opened \n");
        return;
    }
    gsbutils::dprintf(1, (char *)"Exposer:  ServerSocket=%d\n", httpSockfd);

    // Стартуем цикл сервера
    while (flag.load())
    {
        // needed for coming select
        fd_set read_fds;
        int max;

        struct timeval select_timeout;

        select_timeout.tv_sec = (long)1;
        select_timeout.tv_usec = (long)0;

        FD_ZERO(&read_fds); // clear the file handle set
        FD_SET(httpSockfd, &read_fds);
        max = httpSockfd;

        if (select(max + 1, &read_fds, NULL, NULL, (timeval *)&select_timeout) > 0 && flag.load())
        {
            // http socket has data waiting
            if (FD_ISSET(httpSockfd, &read_fds))
            {
                int clientSockfd = accept(httpSockfd, NULL, NULL);
                if (clientSockfd < 0)
                {
                    continue;
                }
                receive_http(clientSockfd);
            }
        }
    } // while Flag

    if (httpSockfd >= 0)
        close(httpSockfd);
}

/// @brief Open TCP socket for web-interface
/// @param port
/// @return
int Exposer::open_tcp_socket(int port)
{
    int server_len = 0;
    struct sockaddr_in server_address = {0};
    int flags = 0;
    int retry = 0;
    int sock_fd = -1;

    // open TCP socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
        gsbutils::dprintf(1, (char *)"Exposer: Unable to open TCP socket: %s.\n", strerror(errno));
        return (-1);
    }
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // receive from anybody
    server_address.sin_port = htons(port);              // http server port
    server_len = sizeof(server_address);
    // this is done the way it is to make restarts of the program easier
    // in Linux, TCP sockets have a 2 minute wait period before closing
    while (app->Flag.load() && bind(sock_fd, (struct sockaddr *)&server_address, server_len) < 0)
    {
        sleep(5); // wait 5 seconds to see if it clears
        retry++;
        // more than 1 minute has elapsed, there must be something wrong
        if (retry > ((1 * 60) / 5) || !app->Flag.load())
            return (-1);
    }
    if (listen(sock_fd, 5) < 0)
    {
        gsbutils::dprintf(1, (char *)"Exposer: Error making TCP server socket listen: %s.\n", strerror(errno));
        return (-1);
    }
    flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, O_NONBLOCK | flags);
    // TCP socket is ready

    return sock_fd;
}
// Функция-обработчик входящих запросов
void Exposer::receive_http(int clientSockfd)
{
    int i = 0;
    int numread = 0;
    std::string response = "";

    char request[MAX_TCP_SIZE]{0};
    // url - полный заголовок ~554 байта
    numread = read(clientSockfd, request, MAX_TCP_SIZE);
    if (numread > 0)
    {
        // truncate if necessary
        if (numread > (MAX_TCP_SIZE - 1))
            numread = (MAX_TCP_SIZE - 1);
        request[numread] = '\0';

        // GET method from browser
        if (strncmp(request, "GET", 3) == 0)
        {
            for (i = 4; i < numread; i++)
            {
                // передвигаем указатель на начало GET-запроса
                if (request[i] == ' ')
                    break;
                request[i - 4] = tolower(request[i]); // переводим запрос в нижний регистр
            }
            request[i - 4] = '\0'; // terminate URL string

            // строка ответа
            response = "";
            std::string url_str = std::string(request);

            if (url_str.starts_with("/metrics"))
            {
                // отдать метрики для Prometheus
                response = metrics();
                send_answer(clientSockfd, response);
            }
            else
            {
                send_error(clientSockfd, 404, "Not found");
            }
        }
        else
        {
            send_error(clientSockfd, 501, "Not implemented");
        }
    }

    close(clientSockfd);
}

/// @brief Отправка ответа с метриками
/// @param clientSockfd
/// @param response
void Exposer::send_answer(int clientSockfd, std::string response)
{
    if (response.size() > 0)
    {
        std::string httpHeader = create_header(200, "OK", "", response.size());
        std::string answer = httpHeader + response;
        ssize_t written = write(clientSockfd, answer.c_str(), answer.size());
    }
}
/// @brief При ошибке отправляю только заголовок
/// @param clientSockfd
/// @param codeError
/// @param response
void Exposer::send_error(int clientSockfd, int codeError, std::string response)
{
    if (response.size() > 0)
    {
        std::string httpHeader = create_header(codeError, response, "", 0);
        ssize_t written = write(clientSockfd, httpHeader.c_str(), httpHeader.size());
    }
}

/// @brief Заголовок ответа
/// @param status
/// @param title
/// @param extra_header
/// @param length
/// @return
std::string Exposer::create_header(int status, std::string title, std::string extraHeader, off_t contentLength)
{

    std::string dstring = "HTTP/1.0 " + std::to_string(status) + " " + title + "\r\n";
    dstring = dstring + "Content-Type: text/plain\r\n";
    dstring = dstring + "Content-Length: " + std::to_string(contentLength) + "\r\n";
    dstring = dstring + "Connection: close\r\n\r\n";
    return dstring;
}
/// @brief Формирует строку с метриками для Prometheus
/// @return
std::string Exposer::metrics()
{
    std::string answer = "";
    // Получим давление
    std::shared_ptr<zigbee::EndDevice> di = app->zhub->get_device_by_mac((zigbee::IEEEAddress)0x00124b000b1bb401); // датчик климата в детской
    if (di)
        answer = answer + di->get_prom_pressure();

    for (auto &li : zigbee::EndDevice::PROM_MOTION_LIST)
    {
        std::shared_ptr<zigbee::EndDevice> di = app->zhub->get_device_by_mac((zigbee::IEEEAddress)li);
        if (di)
            answer = answer + di->get_prom_motion_string();
    }
    for (auto &li : zigbee::EndDevice::PROM_RELAY_LIST)
    {
        // для сдвоенного реле показываем по отдельности
        std::shared_ptr<zigbee::EndDevice> di = app->zhub->get_device_by_mac((zigbee::IEEEAddress)li);
        if (di)
            answer = answer + di->get_prom_relay_string();
    }
    for (auto &li : zigbee::EndDevice::PROM_DOOR_LIST)
    {
        std::shared_ptr<zigbee::EndDevice> di = app->zhub->get_device_by_mac((zigbee::IEEEAddress)li);
        if (di)
            answer = answer + di->get_prom_door_string();
    }
    return answer;
}