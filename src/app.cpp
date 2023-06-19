#include "version.h"

#include <stddef.h>
#include <cstddef>
#include <array>
#include <memory>
#include <sstream>
#include <string>
#include <fstream>
#include <fcntl.h>
#include <sys/select.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <atomic>

#include "../telebot32/src/tlg32.h"
#include "../gsb_utils/gsbutils.h"
#include "comport/unix.h"
#include "comport/serial.h"
#include "pi4-gpio.h"
#include "usb2pin.h"
#include "http.h"
#include "httpserver.h"
#include "exposer.h"
#include "modem.h"
#include "zigbee/zigbee.h"
#include "app.h"

using gsbstring = gsbutils::SString;

extern std::shared_ptr<App> app;
/// ------- App -----------
App::App() { std::cout << "app constructor\n"; };
App::~App() { std::cout << "app destructor\n"; }

bool App::object_create()
{
    Flag.store(true);

    try
    {
        withTlg = false;

        // канал приема команд из телеграм
        tlgIn = std::make_shared<gsbutils::Channel<TlgMessage>>(2);
        // канал отправки сообщений в телеграм
        tlgOut = std::make_shared<gsbutils::Channel<TlgMessage>>(2);
        tlg32 = std::make_shared<Tlg32>(config.BotName, tlgIn, tlgOut);
        tlgInThread = new std::thread(&App::handle, this);
        tlg32->add_id(config.MyId);
        if (tlg32->run())
        {
            // Здесь будет выброшено исключение при отсутствии корректного токена
            withTlg = true;
            tlg32->send_message("Программа перезапущена.\n");
        }

        // Модем инициализируем вне зависимости от координатора
        // для возможности отладки самого модема
        init_modem();

        if (config.UsbUart)
        {
            usbPin = std::make_shared<Usb2pin>();
            withUsbPin = usbPin->connect(config.PortUsbUart);
        }
        cmdThread = std::thread(&App::cmd_func, this);

        zhub = std::make_shared<zigbee::Zhub>();
        noAdapter = zhub->init_adapter();

        if (noAdapter && !(config.Mode == "test"))
        {
            Flag.store(false);
            return false;
        }

        withGpio = false;
#ifdef __linux__
        if (config.Gpio)
        {
            gpio = std::make_shared<Pi4Gpio>();
            withGpio = true;
        }
#endif
    }
    catch (std::exception &e)
    {
        withTlg = false;
        noAdapter = true;
        return false;
    }

    return true;
}
bool App::start_app()
{
    startTime = gsbutils::DDate::current_time();
    if (!noAdapter)
    {
        if (withGpio)
            withGpio = gpio->initialize_gpio(&App::handle_power_off);
        zhub->start(config.Channels);
        http = std::make_unique<HttpServer>();
        http->start();
        if (config.Prometheus)
            exposerThread = std::thread(&App::exposer_handler, this);

        return true;
    }
    tempr_thread = std::thread(&App::get_main_temperature, this); // поток определения температуры платы

    return false;
}

void App::exposer_handler()
{
    if (config.Prometheus)
    {
        // 2.18.450 - сейчас IP фактически не используется, слушает запросы с любого адреса
        exposer = std::make_shared<Exposer>(std::string("localhost"), 8092);
        exposer->start();
    }
}

bool App::init_modem()
{
    gsbutils::dprintf(3, "Start init modem\n");
    gsmModem = std::make_shared<GsmModem>();
    if (config.Sim800)
    {
        try
        {
            withSim800 = gsmModem->connect(config.PortModem, 9600); // 115200  19200 9600 7200
            if (!withSim800)
            {
#ifdef DEBUG
                gsbutils::dprintf(1, "Модем SIM800 не обнаружен\n");
#endif
                if (withTlg)
                    tlg32->send_message("Модем SIM800 не обнаружен.\n");
                return false;
            }
            else
            {
                withSim800 = gsmModem->init_modem();
                if (!withSim800)
                {
#ifdef DEBUG
                    gsbutils::dprintf(1, "Модем SIM800 не активирован\n");
#endif
                    return false;
                }
#ifdef DEBUG
                gsbutils::dprintf(1, "Модем SIM800 активирован\n");
#endif

                if (withTlg)
                    tlg32->send_message("Модем SIM800 активирован.\n");
                gsmModem->get_battery_level(true);
                gsmModem->get_balance(); // для индикации корректной работы обмена по смс
            }
        }
        catch (std::exception &e)
        {
            withSim800 = false;
            if (withTlg)
                tlg32->send_message("Модем SIM800 не обнаружен.\n");
        }

        return true;
    }
    else
    {
#ifdef DEBUG
        gsbutils::dprintf(1, "Модем SIM800 не активирован\n");
#endif
        withSim800 = false;
        return false;
    }
}

// Остановка приложения
void App::stop_app()
{
    if (!stoped)
    {
        stoped = true;
        if (cmdThread.joinable())
            cmdThread.join();
        Flag.store(false);
#ifdef DEBUG
        gsbutils::dprintf(1, "Stopped cmd thread \n");
#endif
        if (withSim800)
            gsmModem->disconnect();
        //            threadPoolModem->stop_threads();
        if (!noAdapter)
        {

            if (withGpio)
                gpio->close_gpio();
#ifdef DEBUG
            gsbutils::dprintf(1, "Stopped modem, gpio \n");
#endif

            zhub->stop();

            if (config.Prometheus)
            {
                exposer->stop_exposer();
                if (exposerThread.joinable())
                    exposerThread.join();
            }
            http->stop_http();

            if (httpThread.joinable())
                httpThread.join();
        }
#ifdef DEBUG
        gsbutils::dprintf(1, "Stopped zhub,http, exposer \n");
#endif

        if (tempr_thread.joinable())
            tempr_thread.join();
#ifdef DEBUG
        gsbutils::dprintf(1, "Stopped temperature thread \n");
#endif

        tlg32->stop();
        tlgIn->stop();
        tlgOut->stop();
        if (tlgInThread->joinable())
            tlgInThread->join();
#ifdef DEBUG
        gsbutils::dprintf(1, "Stopped telegram \n");
#endif
    }
    std::this_thread::sleep_for(std::chrono::seconds(10));
}

// Параметры питания модема
std::string App::show_sim800_battery()
{
    if (withSim800)
    {
        static uint8_t counter = 0;
        char answer[256]{};
        std::array<int, 3> battery = gsmModem->get_battery_level(false);
        std::string charge = "";
        if (battery[0] != -1)
            charge = (battery[1] == 100 && battery[2] > 4400) ? "от сети" : "от батареи";
        std::string level = battery[1] == -1 ? "" : std::to_string(battery[1]) + "%";
        std::string volt = battery[2] == -1 ? "" : std::to_string((float)(battery[2] / 1000)) + "V";
        int res = std::snprintf(answer, 256, "SIM800l питание: %s, %s, %0.2f V\n", charge.c_str(), level.c_str(), battery[2] == -1 ? 0.0 : (float)battery[2] / 1000);
        if (res > 0 && res < 256)
            answer[res] = 0;
        else
            strcat(answer, (char *)"Ошибка получения инфо о батарее");

        counter++;
        if (counter > 5)
        {
            counter = 0;
            gsmModem->get_battery_level(true);
        }
        return std::string(answer);
    }
    else
    {
        return "";
    }
}
// Измеряем температуру основной платы
// Если она больше 70 градусов, посылаем сообщение в телеграм
// включаем вентилятор, выключаем при температуре ниже 60
void App::get_main_temperature()
{

    bool notify_high_send = false;
    while (Flag.load())
    {

        float temp_f = get_board_temperature();
        if (temp_f > 0.0)
        {
            if (temp_f > 70.0 && !notify_high_send)
            {
                // посылаем уведомление о высокой температуре и включаем вентилятор
                notify_high_send = true;
                handle_board_temperature(temp_f);
            }
            else if (temp_f < 50.0 && notify_high_send)
            {
                // посылаем уведомление о нормальной температуре и выключаем вентилятор
                notify_high_send = false;
                handle_board_temperature(temp_f);
            }
        }
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10s);
    }
#ifdef DEBUG
    gsbutils::dprintf(1, "Thread main temperature, end\n");
#endif
}
// Получить значение температуры управляющей платы
float App::get_board_temperature()
{
#ifdef __MACH__
    return -100.0;
#else
    char *fname = (char *)"/sys/class/thermal/thermal_zone0/temp";
    uint32_t temp_int = 0; // uint16_t не пролезает !!!!
    float temp_f = 0.0;

    int fd = open(fname, O_RDONLY);
    if (!fd)
        return -200.0;

    char buff[32]{0};
    size_t len = read(fd, buff, 32);
    close(fd);
    if (len < 0)
    {
        return -100.0;
    }
    buff[len - 1] = 0;
    if (sscanf(buff, "%d", &temp_int))
    {
        temp_f = (float)temp_int / 1000;
    }
    return temp_f;
#endif
}

/// static function
/// обработчик пропажи 220В
void App::handle_power_off(int value)
{
    std::string alarm_msg = "";
    if (value == 1)
        alarm_msg = "Восстановлено 220В\n";
    else if (value == 0)
        alarm_msg = "Отсутствует 220В\n";
    else
        return;
    gsbutils::dprintf(1, alarm_msg);
    if (app->withTlg)
        app->tlg32->send_message(alarm_msg);
}

// Обработчик показаний температуры корпуса
// Включение/выключение вентилятора
void App::handle_board_temperature(float temp)
{
    char buff[128]{0};

    if (temp > 70.0)
        gpio->write_pin(16, 1);
    else
        gpio->write_pin(16, 0);

    size_t len = snprintf(buff, 128, "Температура платы управления: %0.1f \n", temp);
    buff[len] = 0;
    std::string temp_msg = std::string(buff);
    gsbutils::dprintf(1, temp_msg);

    tlg32->send_message(temp_msg);
}

// Включение звонка
void App::ringer()
{
    gpio->write_pin(26, 1);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    gpio->write_pin(26, 0);
}

// Управление вентилятором обдува платы управления
void App::fan(bool work)
{
    gpio->write_pin(16, work ? 1 : 0);
}

// используется в телеграм
std::string App::show_statuses()
{
    std::string statuses = zhub->show_device_statuses(false);
    if (statuses.empty())
        return "Нет активных устройств\n";
    else
        return "Статусы устройств:\n" + statuses + "\n";
}

// Здесь реализуется вся логика обработки принятых сообщений из телеграм
void App::handle()
{
    while (Flag.load() && withTlg)
    {
        TlgMessage msg = tlgIn->read();
        if (!msg.text.empty())
        {
            TlgMessage answer{};
            answer.chat.id = msg.from.id;

            DBGLOG("Входное сообщение %s: %s \n", msg.from.firstName.c_str(), msg.text.c_str());
            if (!tlg32->client_valid(msg.from.id))
            {
                answer.text = "Я не понял Вас.\n";
                bool ret = tlg32->send_message(answer);
            }
            if (msg.text.starts_with("/start"))
                answer.text = "Привет, " + msg.from.firstName;
            else if (msg.text.starts_with("/stat"))
            {
                std::string stat = show_statuses();
                if (stat.size() == 0)
                    answer.text = "Нет ответа\n";
                else
                    answer.text = stat;
            }
            else if (msg.text.starts_with("/balance"))
            {
                if (app->withSim800)
                {
                    if (app->gsmModem->get_balance())
                        answer.text = "Запрос баланса отправлен\n";
                }
                else
                {
                    answer.text = "SIM800 не подключен\n";
                }
            }
            else
                answer.text = "Я не понял Вас.\n";

            tlgOut->write(answer);
        }
    }
}

// Получение глобального конфига настроек
bool App::parse_config()
{
#ifdef __MACH__
    config.Os = "darwin";
#endif
#ifdef __linux__
    config.Os = "linux";
#endif
    // некоторые предустановки по умолчанию, если параметр в конфиге пропущен или некорректен,
    // будет взят дефолтный
    config.Prometheus = false;
    config.Sim800 = false;
    config.Gpio = false;
    config.UsbUart = false;

    std::string filename = "/usr/local/etc/zhub2/config.txt";
    std::ifstream infile(filename.c_str());
    if (infile.is_open())
    {
        std::string mode = "";   // имя текущей секции, пустая строка - параметры до секций
        bool sectionMode = true; // параметр находится в нужной секции (true) или нет (false)
        std::string line;
        while (std::getline(infile, line))
        {
            if (line.starts_with("//") || line.size() < 3)
                continue;
            line = gsbstring::trim(line);

            // find mode
            if (mode.size() == 0)
            {
                std::pair<std::string, std::string> val = gsbstring::split_string_with_delimiter(line, " ");
                std::string key = gsbstring::trim(val.first);
                if (key == "Mode")
                {
                    if (val.second.size() > 0)
                        mode = gsbstring::trim(val.second);

                    config.Mode = mode;
                }

                continue;
            }
            if (line.starts_with("["))
            {
                // section start
                line = gsbstring::remove_before(line, "[");
                line = gsbstring::remove_after(line, "]");
                sectionMode = config.Mode == line;
                continue;
            }
            if (!sectionMode)
                continue; // pass unnecessary section

            // Далее идут параметры нужной секции
            std::pair<std::string, std::string> val = gsbstring::split_string_with_delimiter(line, " ");
            std::string key = gsbstring::trim(val.first);
            std::string value = gsbstring::trim(val.second);
            if (key == "BotName")
            {
                // TODO: check length
                config.BotName = value;
            }
            else if (key == "MyId")
            {
// TODO: check int64
#ifdef __MACH__
                sscanf(value.c_str(), "%lld", &config.MyId);
#else
                sscanf(value.c_str(), "%ld", &config.MyId);
#endif
            }
            else if (key == "TokenPath")
            {
                // TODO: check valid path
                config.TokenPath = value;
            }
            else if (key == "MapPath")
            {
                // TODO: check valid path
                config.MapPath = value;
            }
            else if (key == "Port")
            {
                // TODO: check valid
                config.Port = value;
            }
            else if (key == "PortModem")
            {
                // TODO: check valid
                config.PortModem = value;
            }
            else if (key == "PortUsbUart")
            {
                // TODO: check valid
                config.PortUsbUart = value;
            }
            else if (key == "PhoneNumber")
            {
                // TODO: check valid
                // Плюс в начале убираю
                config.PhoneNumber = gsbstring::remove_before(value, "+");
            }
            else if (key == "Channels")
            {
                std::vector<std::string> resChan = gsbstring::split(value, ",");
                if (resChan.size() > 0)
                {
                    unsigned ch = 0;
                    for (auto &s : resChan)
                    {
                        if (sscanf(s.c_str(), "%u", &ch) > 0)
                            if (ch > 0)
                                config.Channels.push_back((uint8_t)ch);
                    }
                }
            }
            else if (key == "Prometheus")
            {
                unsigned v;
                if (sscanf(value.c_str(), "%u", &v) > 0)
                    config.Prometheus = v != 0;
            }
            else if (key == "Sim800")
            {
                unsigned v;
                if (sscanf(value.c_str(), "%u", &v) > 0)
                    config.Sim800 = v != 0;
            }
            else if (key == "Gpio")
            {
                unsigned v;
                if (sscanf(value.c_str(), "%u", &v) > 0)
                    config.Gpio = v != 0;
            }
            else if (key == "UsbUart")
            {
                unsigned v;
                if (sscanf(value.c_str(), "%u", &v) > 0)
                    config.UsbUart = v != 0;
            }

        } // while
        return true;
    } // if

    return false;
}
// Функция потока ожидания команд с клавиатуры
int App::cmd_func()
{
    time_t waitTime = 1;
    struct timeval tv;
    char first_command = 0;

    while (Flag.load())
    {
        char c = '\0';
        int nfds = 1;

        tv.tv_sec = (long)5;
        tv.tv_usec = (long)0;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(0, &readfds); // 0 - STDIN

        int count = select(nfds, &readfds, NULL, NULL, (timeval *)&tv);
        if (count > 0)
        {
            if (FD_ISSET(0, &readfds))
            {
                c = getchar();
            }
        }

        if (c <= '9' && c >= '0' && first_command == 'd')
        {
#ifdef DEBUG
            gsbutils::set_debug_level(static_cast<unsigned int>(c - '0'));
            first_command = 0;
#endif
        }
        else
        {
            switch (c)
            {
            case 'c': // get CTS | DSR level
                if (withUsbPin)
                {
                    int cts = usbPin->get_cts();
                    int dsr = usbPin->get_dsr();

                    gsbutils::dprintf(1, "CTS | DSR: %d \n", cts + dsr * 2);
                }
                break;
            case 'T': // DTR and RTS High level
                if (withUsbPin)
                {
                    usbPin->set_dtr(1);
                    usbPin->set_rts(1);
                }
                break;
            case 't': // DTR and  RTS Low level
                if (withUsbPin)
                {
                    usbPin->set_dtr(0);
                    usbPin->set_rts(0);
                }
                break;

            case 'd': // уровень отладки
            {
                first_command = 'd';
            }
            break;
            case 'F': // включить вентилятор
                fan(1);
                break;
            case 'f': // выключить вентилятор
                fan(0);
                break;

            case 'j': // команда разрешения привязки
                zhub->permitJoin(std::chrono::seconds(60));
                break;

            case 'q': // завершение программы
                fprintf(stderr, "Exit\n");
                Flag.store(false);

                return 0;

            } // switch
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    return 0;
}
