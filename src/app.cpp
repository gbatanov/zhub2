
#include <memory>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <ctime>
#include <fcntl.h>
#include <cstdlib>
#include <signal.h>
#include <sys/select.h>
#include <string>
#include <mutex>
#include <sstream>
#include <queue>
#include <chrono>
#include <syslog.h>
#include <array>
#include <set>
#include <unordered_map>
#include <iostream>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <utility>
#include <algorithm>
#include <optional>
#include <any>
#include <termios.h>

#include "version.h"

#include "../telebot32/src/tlg32.h"
#include "../gsb_utils/gsbutils.h"
#include "comport/unix.h"
#include "comport/serial.h"
#include "common.h"
#include "zigbee/zigbee.h"
#include "exposer.h"
#include "modem.h"
#include "httpserver.h"
#include "http.h"
#include "exposer.h"
#include "pi4-gpio.h"
#include "app.h"

using gsb_utils = gsbutils::SString;
extern std::unique_ptr<HttpServer> http;
extern App app;
/// ------- App -----------
bool App::object_create()
{
    Flag.store(true);

    try
    {

        cmdThread = std::thread(&App::cmd_func, this);
        zhub = std::make_shared<zigbee::Zhub>();
        zhub->tlg32->add_id(836487770);

        if (zhub->tlg32->run())
        { // Здесь будет выброшено исключение при отсутствии корректного токена
            zhub->tlg32->send_message("Программа перезапущена.\n");
        }
        noAdapter = zhub->init_adapter();

        tpm = std::make_shared<gsbutils::ThreadPool<std::vector<uint8_t>>>();
        uint8_t max_threads = 2;
        tpm->init_threads(&GsmModem::on_command, max_threads);

        init_modem();
    }
    catch (std::exception &e)
    {
        noAdapter = true;
        return false;
    }

    return true;
}
bool App::startApp()
{
    startTime = gsbutils::DDate::current_time();
    if (!noAdapter)
    {
        zhub->start(
#ifdef TEST
            zigbee::Controller::TEST_RF_CHANNELS
#else
            zigbee::Controller::DEFAULT_RF_CHANNELS
#endif
        );
        httpThread = std::thread(http_server);
        exposerThread = std::thread(&App::exposer_handler, this);
        timer1Min = std::make_shared<gsbutils::CycleTimer>(60, &App::timer1min);

        timer1Min->run();
    }
    return true;
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
            case 'P': //
            case 'p': //
            {
                zhub->switch_relay(0x00158d0009414d7e, c == 'P' ? 1 : 0, 1);
            }
            break;
            case 'R':
            case 'r':
            {
                zhub->switch_relay(0x00158d0009414d7e, c == 'R' ? 1 : 0, 2);
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

            case 'q': // завершение программы
                fprintf(stderr, "Exit\n");
                Flag.store(false);

                return 0;

            case 'j': // команда разрешения привязки
                zhub->permitJoin(std::chrono::seconds(60));
                break;
            } // switch
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    return 0;
}

void App::exposer_handler()
{
    // 2.18.450 - сейчас IP фактически не используется, слушает запросы с любого адреса
    Exposer exposer(std::string("localhost"), 8092);
    exposer.start();
}

bool App::init_modem()
{
    gsmModem = std::make_shared<GsmModem>();
#ifdef WITH_SIM800
    try
    {

#ifdef __MACH__
        with_sim800 = gsmModem->connect("/dev/cu.usbserial-A50285BI", 9600); // 115200  19200 9600 7200
#else
        with_sim800 = gsmModem->connect("/dev/ttyUSB0");
#endif
        if (!with_sim800)
        {
            zhub->tlg32->send_message("Модем SIM800 не обнаружен.\n");
            return false;
        }
        else
        {
            with_sim800 = gsmModem->init_modem();
            if (!with_sim800)
                return false;
            zhub->tlg32->send_message("Модем SIM800 активирован.\n");
            gsmModem->get_battery_level(true);
            gsmModem->get_balance(); // для индикации корректной работы обмена по смс
        }
    }
    catch (std::exception &e)
    {
        with_sim800 = false;
        zhub->tlg32->send_message("Модем SIM800 не обнаружен.\n");
    }

    return true;
#else
    with_sim800 = false;
    return false;
#endif
}
// timer 1 min - callback function
void App::timer1min()
{
    app.zhub->check_motion_activity();
}

// Остановка приложения
void App::stopApp()
{
    Flag.store(false);

#ifdef IS_PI
    close_gpio();
#endif

    gsmModem->disconnect();
    zhub->tlg32->stop();
    zhub->stop(); // остановка пулла потоков
    if (exposerThread.joinable())
        exposerThread.join();
    http_stop();
    if (httpThread.joinable())
        httpThread.join();

    gsbutils::stop(); // остановка вывода сообщений
}

// Параметры питания модема
std::string App::show_sim800_battery()
{
#ifdef WITH_SIM800
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

#else
    return "";
#endif
}
// Измеряем температуру основной платы
// Если она больше 70 градусов, посылаем сообщение в телеграм
// включаем вентилятор, выключаем при температуре ниже 60
void App::get_main_temperature()
{

    bool notify_high_send = false;
    while (app.Flag.load())
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
}
// Получить значение температуры управляющей платы
float App::get_board_temperature()
{
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
}

// обработчик пропажи 220В
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

    zhub->tlg32->send_message(alarm_msg);
}

// Обработчик показаний температуры корпуса
// Включение/выключение вентилятора
void App::handle_board_temperature(float temp)
{
    char buff[128]{0};

    if (temp > 70.0)
        gpio.write_pin(16, 1);
    else
        gpio.write_pin(16, 0);

    size_t len = snprintf(buff, 128, "Температура платы управления: %0.1f \n", temp);
    buff[len] = 0;
    std::string temp_msg = std::string(buff);
    gsbutils::dprintf(1, temp_msg);

    zhub->tlg32->send_message(temp_msg);
}

// Включение звонка
void App::ringer()
{
    gpio.write_pin(26, 1);
    sleep(1);
    gpio.write_pin(26, 0);
}

// Управление вентилятором обдува платы управления
void App::fan(bool work)
{
    gpio.write_pin(16, work ? 1 : 0);
}
