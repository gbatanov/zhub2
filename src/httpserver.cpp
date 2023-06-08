// Сервер для АПИ для внешних запросов

#define INVALID_PARAMS "Invalid params\n"

#include "version.h"
#include "../gsb_utils/gsbutils.h"

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
#include <any>
#include <optional>

#include <termios.h>
#include "comport/unix.h"
#include "comport/serial.h"
#include "common.h"
#include "zigbee/zigbee.h"
#include "modem.h"
#include "http.h"
#include "httpserver.h"
#include "app.h"

extern App app;

// TODO: нужен мап с объектом Request и функцией-обработчиком
HttpServer::HttpServer()
{
}
HttpServer::~HttpServer() {}

bool HttpServer::start()
{
#ifdef WITH_HTTP
    // Попытка открыть TCP socket для HTTP-сервер
    http_sockfd = open_tcp_socket(http_server_port);
    if (http_sockfd < 0)
    {
        gsbutils::dprintf(1, (char *)"HTTPServer: HTTP ServerSocket is not opened \n");
        return -1;
    }
    gsbutils::dprintf(1, (char *)"HTTPServer: HTTP ServerSocket=%d\n", http_sockfd);

    // Стартуем цикл сервера
    while (app.Flag.load())
    {
        // needed for coming select
        fd_set read_fds;
        int max;

        struct timeval select_timeout;

        select_timeout.tv_sec = (long)1;
        select_timeout.tv_usec = (long)0;

        FD_ZERO(&read_fds); // clear the file handle set
        FD_SET(http_sockfd, &read_fds);
        max = http_sockfd;

        if (select(max + 1, &read_fds, NULL, NULL, (timeval *)&select_timeout) > 0 && app.Flag.load())
        {
            // http socket has data waiting
            if (FD_ISSET(http_sockfd, &read_fds))
            {
                int client_sockfd = accept(http_sockfd, NULL, NULL);
                if (client_sockfd < 0)
                {
                    continue;
                }
                receive_http(client_sockfd);
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } // while Flag

    if (http_sockfd >= 0)
        close(http_sockfd);
#endif
    return true;
}

/// @brief Open TCP socket for web-interface
/// @param port
/// @return
int HttpServer::open_tcp_socket(int port)
{
    int server_len = 0;
    struct sockaddr_in server_address = {0};
    int flags = 0;
    int retry = 0;
    int sock_fd = -1;

    gsbutils::dprintf(7, (char *)"HTTPServer: Opening TCP server socket...\n");
    // open TCP socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
        gsbutils::dprintf(1, (char *)"HTTPServer: Unable to open TCP socket: %s.\n", strerror(errno));
        return (-1);
    }
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // receive from anybody
    server_address.sin_port = htons(port);              // http server port
    server_len = sizeof(server_address);
    gsbutils::dprintf(7, (char *)"HTTPServer:  TCP server socket open on file descriptor %d\n", sock_fd);
    // this is done the way it is to make restarts of the program easier
    // in Linux, TCP sockets have a 2 minute wait period before closing
    while (app.Flag.load() && bind(sock_fd, (struct sockaddr *)&server_address, server_len) < 0)
    {
        gsbutils::dprintf(7, (char *)"HTTPServer:  Error binding TCP server socket: %s.  Retrying...\n", strerror(errno));
        sleep(5); // wait 5 seconds to see if it clears
        retry++;
        // more than 1 minute has elapsed, there must be something wrong
        if (retry > ((1 * 60) / 5) || !app.Flag.load())
            return (-1);
    }
    if (listen(sock_fd, 5) < 0)
    {
        gsbutils::dprintf(1, (char *)"HTTPServer: Error making TCP server socket listen: %s.\n", strerror(errno));
        return (-1);
    }
    flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, O_NONBLOCK | flags);
    // TCP socket is ready to go
    gsbutils::dprintf(7, (char *)"HTTPServer: TCP server socket listening on local port %d\n", port);

    return sock_fd;
}

void HttpServer::send_error(int client_sockfd, int status, char *text)
{
    char dstring[2048] = {0};

    char *errorTpl = (char *)"<p>error: %d, reason : %s </p>";

    char buf[1024] = {0};
    int len = snprintf(buf, 1024, errorTpl, status, text);
    buf[len] = '\0';

    create_header(dstring, status, (char *)"Error", NULL, (char *)"text/html", len, -1);

    strncat(dstring, buf, len); // len - ограничение на buf
    len = strlen(dstring);
    len = write(client_sockfd, dstring, len);
    gsbutils::dprintf(7, (char *)"HTTPServer: Sending %d bytes back to browser\n", len);
}

/// @brief Заголовок ответа
/// @param dstring
/// @param status
/// @param title
/// @param extra_header
/// @param mime_type
/// @param length
/// @param mod
/// @return
size_t HttpServer::create_header(char *dstring, int status, char *title, char *extra_header, char *mime_type, off_t length, time_t mod)
{
    time_t now;
    char timebuf[256];
    size_t header_size = 0; // return value

    char tmp_string[1024] = {0};
    int len = 0;

    now = time((time_t *)0);
    (void)strftime(timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));

    len = snprintf(tmp_string, 1024,
                   "%s %d %s\r\n"
                   "Server: %s\r\n"
                   "Date: %s\r\n",
                   "HTTP/1.0", status, title, "localhost", timebuf);
    strncat(dstring, tmp_string, len);
    strcat(dstring, (char *)"Content-Type:");
    if (mime_type)
    {
        strcat(dstring, mime_type);
    }
    else
    {
        strcat(dstring, (char *)"text/html");
    }
    strcat(dstring, (char *)"\r\n");
    if (length >= 0)
    {
        len = snprintf(tmp_string, 1024, "Content-Length: %lld\r\n", (long long int)length);
        tmp_string[len] = '\0';
        strncat(dstring, tmp_string, len);
    }
    if (mod != (time_t)-1)
    {
        (void)strftime(timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&mod));
        len = snprintf(tmp_string, 1024, "Last-Modified: %s\r\n", timebuf);
        tmp_string[len] = '\0';
        strncat(dstring, tmp_string, len);
    }
    strcat(dstring, (char *)"Connection: close\r\n\r\n");
    header_size = strlen(dstring);

    return header_size;
}

// send_TCP sends a fully created document back to the browser
// Тип контента по умолчанию - text/html
void HttpServer::send_answer(int client_sockfd, std::string http_buffer, size_t http_size, std::string contentType, bool autorefresh)
{
    if (contentType.size() < 4)
        contentType = "text/html";
    send_answer(client_sockfd, (char *)http_buffer.c_str(), http_size, (char *)contentType.c_str(), autorefresh);
}
void HttpServer::send_answer(int client_sockfd, char *http_buffer, size_t http_size, char *contentType, bool autorefresh)
{
    char http_header[1024] = {0};

    size_t tcp_size = (size_t)0;
    char body_response[8192];
    body_response[0] = 0;
    if (!contentType)
        contentType = (char *)"text/html";

    if (http_buffer && http_size)
    {
        std::string head = "";
        std::string response = "";
        if (strcmp(contentType, "text/html") == 0)
        {
            head = create_head(autorefresh);
            create_body(http_buffer, http_size, &body_response[0]);
            std::string body = std::string(body_response);
            response = create_page(head, body);
        }
        else
        {
            response = std::string(http_buffer);
        }

        size_t http_header_size = create_header(http_header, 200, (char *)"OK", NULL, (char *)contentType, response.size(), -1);

        response = std::string(http_header) + response;
        gsbutils::dprintf(7, "Response size: %ld\n", response.size());
        gsbutils::dprintf(7, "%s\n", response.c_str());
        ssize_t written = write(client_sockfd, response.c_str(), response.size());
        //        ssize_t written = send(client_sockfd, response.c_str(), response.size(),0);
        gsbutils::dprintf(7, "Response written size: %ld\n", written);
    }
}

void HttpServer::send_plain(int client_sockfd, std::string response)
{
    write(client_sockfd, response.c_str(), response.size());
}
// создание заголовка страницы.
std::string HttpServer::create_head(bool autorefresh)
{
    std::string top = "<html lang='ru'><head><meta charset='utf-8'>";

    if (autorefresh)
    {
#ifdef DEBUG
        top = top + "<meta http-equiv='refresh' content='20'>";
#else
        top = top + "<meta http-equiv='refresh' content='60'>";
#endif
    }

    top = top + "<link type='text/css' rel='stylesheet' href='/gsb_style.css'>";
    top = top + "</head>";
    return top;
}

// создание страницы
std::string HttpServer::create_page(std::string header, std::string body)
{
    if (header.size() == 0)
        return body;
    return header + body + "</html>";
}
// создание тела страницы. В ответе длина тела страницы
size_t HttpServer::create_body(char *http_buffer, size_t http_size, char *body)
{
    std::string top = "<body>";
    strncpy(body, top.c_str(), top.size() + 1);
    strncat(body, http_buffer, http_size);
    strncat(body, "</body>", strlen("</body>") + 1);
    return strlen(body);
}

// TODO: путь к стилям в настройки
std::string HttpServer::get_style_from_file(std::string stylefile)
{
    std::string response = "";
    std::string fullPath = cssPath + stylefile;
    char resp[2048]{};
    char *fstyle = (char *)fullPath.c_str();
    FILE *fp = fopen(fstyle, "r");
    if (fp == NULL)
    {
        ERRLOG("Failed to open css style file\n");
        return "";
    }

    size_t count = fread(resp, 1, 2048, fp);
    if (count < 1)
    {
        ERRLOG("Failed to read css style file\n");
        fclose(fp);
        return "";
    }
    resp[count] = 0;
    fclose(fp);
    response = std::string(resp);
    return response;
}
