#ifndef GSBHTTP_H
#define GSBHTTP_H

void http_server();
void http_stop();
std::string create_device_list();
std::string http_get_balance();
void receive_http(int client_sockfd);
std::string http_join();
std::string send_cmd_to_device(char *url);
std::string send_cmd_to_device(std::string url);
std::string send_cmd_to_onoff(std::string url);

#endif