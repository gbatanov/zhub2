#include <atomic>
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

#include "version.h"
#include "comport/unix.h"
#include "comport/serial.h"
#include "../gsb_utils/gsbutils.h"
#include "common.h"
#include "zigbee/zigbee.h"

#ifdef WITH_SIM800
#include "modem.h"
#endif

#include "httpserver.h"
#include "http.h"

extern char *Program_Version;
extern std::unique_ptr<zigbee::Zhub> zhub;
extern std::string startTime;

#ifdef WITH_TELEGA
#include "../telebot32/src/tlg32.h"
extern std::unique_ptr<Tlg32> tlg32;
#endif

#ifdef WITH_SIM800
extern GsmModem *gsmmodem;
#endif

extern std::atomic<bool> Flag;
std::unique_ptr<HttpServer> http;

using gsb_utils = gsbutils::SString;

void http_server()
{
    http = std::make_unique<HttpServer>();
    http->start(); // поток приема http-запросов
}

// Функция-обработчик входящих запросов
void receive_http(int client_sockfd)
{
    int i = 0;
    int numread = 0;
    std::string response = "";

    char request[MAX_TCP_SIZE]{0};
    // url - полный заголовок ~554 байта
    numread = read(client_sockfd, request, MAX_TCP_SIZE);
    if (numread > 0)
    {
        // truncate if necessary
        if (numread > (MAX_TCP_SIZE - 1))
            numread = (MAX_TCP_SIZE - 1);
        request[numread] = '\0';
        gsbutils::dprintf(7, (char *)"HTTPServer: Client sockfd %d\n", client_sockfd);
        gsbutils::dprintf(7, (char *)"HTTPServer: %d bytes received\n", numread);
        gsbutils::dprintf(7, (char *)"HTTPServer: http request:\n%s", request);

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

            gsbutils::dprintf(7, (char *)"HTTPServer: Browser is asking to GET %s\n", request);

            // строка ответа
            response = "";
            std::string url_str = std::string(request);
            // стили, послать содержимое файла
            if (url_str.starts_with("/gsb_style.css"))
            {
                char http_header[1024] = {'\0'};

                response = http->get_style_from_file("zhub2/css/gsb_style.css");
                gsbutils::dprintf(7, "%s\n", response.c_str());
                size_t http_header_size = http->create_header(http_header, 200, (char *)"OK", NULL, (char *)"text/css", response.size(), -1);

                response = std::string(http_header) + response;
                gsbutils::dprintf(7, "gsb_style.css size: %d\n", response.size());
                gsbutils::dprintf(7, "%s\n", response.c_str());
                ssize_t written = write(client_sockfd, response.c_str(), response.size());
                gsbutils::dprintf(7, "gsb_style.css written: %ld\n", written);
            }
            else
            {
                // команды
                if (url_str.starts_with("/index") || url_str.size() == 1)
                {
                    // отдаем список подключенных устройств
                    response = create_device_list();
                    http->send_answer(client_sockfd, response, response.size(), "text/html", true);
                }
                else if (url_str.starts_with("/water"))
                {
                    // выполнить команду на устройствах охраны
                    response = send_cmd_to_device(url_str);
                    http->send_answer(client_sockfd, response, response.size(), "text/html", false);
                }
                else if (url_str.starts_with("/command"))
                {
                    // выполнить команду на произвольных реле (не охрана протечек)
                    // TODO: что в списке all?
                    response = send_cmd_to_onoff(url_str);
                    http->send_answer(client_sockfd, response, response.size(), "text/html", false);
                }
                else if (url_str.starts_with("/balance"))
                {
                    // запрос баланса сим-карты
                    response = http_get_balance();
                    http->send_answer(client_sockfd, response, response.size(), "text/html", false);
                }
                else if (url_str.starts_with("/join"))
                {
                    // выполнить команду
                    response = http_join();
                    http->send_answer(client_sockfd, response, response.size(), "text/html", false);
                }
                else if (url_str.starts_with("/join"))
                {
                    // выполнить команду
                    response = http_join();
                    http->send_answer(client_sockfd, response, response.size(), "text/html", false);
                }
                else
                {
                    http->send_error(client_sockfd, 404, (char *)"Not found");
                    gsbutils::dprintf(7, (char *)"HTTPServer: URL \"%s\" not found\n", url_str.c_str());
                }
            }
        }
        else
        {
            http->send_error(client_sockfd, 501, (char *)"Not implemented");
        }
    }
    else
    {
        gsbutils::dprintf(7, (char *)"HTTPServer: numread=%d\n", numread);
    }

    close(client_sockfd);
}

// Список устройств
// В шапке выдается температура материнской платы и данные по модему
// а также время последнего срабатывания любого датчика движения
std::string create_device_list()
{

    std::string result = "";

#if !defined __MACH__
    float board_temperature = zhub->get_board_temperature();
    if (board_temperature > -100.0)
    {
        char buff[128]{0};
        int len = snprintf(buff, 128, "%.1f", board_temperature);
        buff[len] = 0;
        result = result + "<p>" + "<b>Температура платы управления: </b>";
        result = result + std::string(buff) + "</p>";
    }
#endif
#ifdef WITH_SIM800
    result = result + "<p>" + zhub->show_sim800_battery() + "</p>";
#endif

    std::time_t lastMotionSensorAction = zhub->getLastMotionSensorActivity();
    result = result + "<p>Время последнего срабатывания датчиков движения: " + gsbutils::DDate::timestamp_to_string(lastMotionSensorAction) + "</p>";
    result = result + "<p>Старт программы: " + startTime + "</p>";

    std::string list = zhub->show_device_statuses(true);
    result = result + "<h3>Список устройств:</h3>";
    if (list.empty())
        result = result + "<p>(устройства отсутствуют)</p>";
    else
        result = result + list;

    result = result + "<p><a href=\"/command\">Список команд</a></p>";
    return result;
}

// command выводит список комманд
// каждая команда - ссылка
// в качестве адреса используем мак-адрес, он константный
std::string command_list()
{
    std::string result = "";
    result += "<h3>Список команд</h3>";
    result += "<p>--------- Протечка -----------------</p>";
    result += "<p>Краны и реле СМ&nbsp;<a href=\"/water?on=all\">Включить</a>&nbsp;<a href=\"/water?off=all\">Выключить</a></p>";
    result += "<p>Кран ХВ&nbsp;<a href=\"/water?on=0xa4c138373e89d731\">Включить</a>&nbsp;<a href=\"/water?off=0xa4c138373e89d731\">Выключить</a></p>";
    result += "<p>Кран ГВ&nbsp;<a href=\"/water?on=0xa4c138d9758e1dcd\">Включить</a>&nbsp;<a href=\"/water?off=0xa4c138d9758e1dcd\">Выключить</a></p>";
    result += "<p>Реле СМ&nbsp;<a href=\"/water?on=0x54ef441000193352\">Включить</a>&nbsp;<a href=\"/water?off=0x54ef441000193352\">Выключить</a></p>";
    result += "<p>----------- Умные розетки ---------------</p>";
//    result += "<p>Розетка 1&nbsp;<a href=\"/command?on=0x70b3d52b6001b4a4\">Включить</a>&nbsp;<a href=\"/command?off=0x70b3d52b6001b4a4\">Выключить</a></p>";
    result += "<p>Розетка 2&nbsp;<a href=\"/command?on=0x70b3d52b6001b5d9\">Включить</a>&nbsp;<a href=\"/command?off=0x70b3d52b6001b5d9\">Выключить</a></p>";
    result += "<p>Розетка 3&nbsp;<a href=\"/command?on=0x70b3d52b60022ac9\">Включить</a>&nbsp;<a href=\"/command?off=0x70b3d52b60022ac9\">Выключить</a></p>";
    result += "<p>Розетка 4&nbsp;<a href=\"/command?on=0x70b3d52b60022cfd\">Включить</a>&nbsp;<a href=\"/command?off=0x70b3d52b60022cfd\">Выключить</a></p>";
    result += "<p>----------- Двухканальные реле ---------------</p>";
    result += "<p>Реле 7&nbsp;<a href=\"/command?on=0x00158d0009414d7e&ep=1\">Включить 1</a>&nbsp;<a href=\"/command?off=0x00158d0009414d7e&ep=1\">Выключить1</a><a href=\"/command?on=0x00158d0009414d7e&ep=2\">Включить 2</a>&nbsp;<a href=\"/command?off=0x00158d0009414d7e&ep=2\">Выключить 2</a></p>";
    result += "<p></p>";
    result += "<p>-------------------------------</p>";
#ifdef WITH_SIM800
    result += "<p><a href=\"/balance\">Запросить баланс</a></p>";
    result += "<p>-------------------------------</p>";
#endif
    result += "<p><a href=\"/join\">Разрешить спаривание</a></p>";
    result += "<p>-------------------------------</p>";

    result = result + "<p><a href=\"/\">Список устройств</a></p>";
    return result;
}

// команды управления реле и умными розетками
std::string send_cmd_to_onoff(std::string url)
{
    std::string result = command_list();
    std::string command = gsb_utils::remove_before(url, "?"); // отрезаем "/command?"
    size_t pos = command.find_first_of("=");
    if (pos == command.npos)
        return result;
    std::string cmd = command.substr(0, pos);
    std::string param = command.substr(pos + 1); //!!!
    std::string res_cmd = "";
    uint64_t mac_addr = 0; // mac address
    int ep = 1;        // endpoint, default =1

    pos = command.find_first_of("ep=");
    if (pos == command.npos)
    {
        // пришел конкретный МАК-адрес
        sscanf(param.c_str(), "0x%" PRIx64 "", &mac_addr);
    }
    else
    {
        sscanf(param.c_str(), "0x%" PRIx64 "&ep=%d", &mac_addr, &ep);
        gsbutils::dprintf(1, "ep=%d\n", ep);
        if (ep > 2 || ep < 1)
            ep = 1;
    }
    if (cmd == "on")
    {
        zhub->switch_relay(mac_addr, 0x01, (uint8_t)ep);
        res_cmd = "Команда \"Включить\" " + param  + " исполнена";
    }
    else if (cmd == "off")
    {
        zhub->switch_relay(mac_addr, 0, (uint8_t)ep);
        res_cmd = "Команда \"Выключить\" " +  param  + " исполнена";
    }
    else
    {
        res_cmd = "Неизвестная команда: " + cmd;
    }
    result = result + "<p>" + res_cmd + "</p>";
    return result;
}

std::string send_cmd_to_device(std::string url)
{
    return send_cmd_to_device((char *)url.c_str());
}

// команды для блока защиты от протечек
std::string send_cmd_to_device(char *url)
{
    std::string result = command_list();
    std::string command = "";
    if (strlen(url) > 5)
    {
        command = gsb_utils::remove_before(std::string(url), "water"); // отрезаем "/water"
    }
    else
    {
        return result;
    }
    if (command.substr(0, 1) != "?")
    {
        return result;
    }

    command = gsb_utils::remove_before(command, "?"); // отрезаем   ?
    size_t pos = command.find_first_of("=");
    if (pos == command.npos)
        return result;
    std::string cmd = command.substr(0, pos);
    std::string param = command.substr(pos + 1); //!!!
    std::string res_cmd = "";
    uint64_t mac_addr = 0;

    if (param != "all")
    {
        // пришел конкретный МАК-адрес
        sscanf(param.c_str(), "0x%" PRIx64 "", &mac_addr);
    }

    if (cmd == "on")
    {
        zhub->ias_zone_command(0x01, mac_addr ? (uint64_t)mac_addr : (uint16_t)0);
        res_cmd = "Команда \"Включить\" " + (mac_addr ? param : "all") + " исполнена";
    }
    else if (cmd == "off")
    {
        zhub->ias_zone_command(0x00, mac_addr ? (uint64_t)mac_addr : (uint16_t)0);
        res_cmd = "Команда \"Выключить\" " + (mac_addr ? param : "all") + " исполнена";
    }
    else
    {
        res_cmd = "Неизвестная команда: " + cmd;
    }
    result = result + "<p>" + res_cmd + "</p>";
    return result;
}

std::string http_get_balance()
{
#ifdef WITH_SIM800
    gsmmodem->get_balance();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    return gsmmodem->show_balance() + command_list();
#else
    return "";
#endif
}

std::string http_join()
{
    zhub->permitJoin(std::chrono::seconds(60));
    std::string result = command_list();
    return result;
}
