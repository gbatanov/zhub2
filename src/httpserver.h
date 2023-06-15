#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <string>
#include <cstring>

class HttpServer;

typedef void (*input_handler)();
#ifndef MAX_TCP_SIZE
#define MAX_TCP_SIZE 4096
#endif

class HttpServer
{
public:
    HttpServer();
    ~HttpServer();
    bool start();
    void stop_http();

    int open_tcp_socket(int port);
    void send_error(int client_sockfd, int status, char *text);
    size_t create_header(char *dstring, int status, char *title, char *extra_header, char *mime_type, off_t length, time_t mod);
    void send_answer(int client_sockfd, char *http_buffer, size_t http_size, char *contentType = nullptr, bool autorefresh = false);
    void send_answer(int client_sockfd, std::string http_buffer, size_t http_size, std::string contentType = "", bool autorefresh = false);
    void send_plain(int client_sockfd, std::string response);

    size_t create_body(char *http_buffer, size_t http_size, char *body);
    std::string create_head(bool autorefresh = false);
    std::string create_page(std::string header, std::string body);
    std::string get_style_from_file(std::string stylefile);

    std::string cssPath = "/usr/local/etc/";

    int http_sockfd = -1;
    int http_server_port = 8054;
    int client_sockfd = -1;
    std::atomic<bool> flag;
    std::thread http_main_thread;
    std::shared_ptr <gsbutils::ThreadPool<int>> threadPoolHttp; // пул рабочих потоков веб-сервера
};

#endif // HTTPSERVER_H
