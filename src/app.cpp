
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
#ifdef IS_PI
        initialize_gpio();
#endif

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

        httpThread = std::thread(http_server);
        exposerThread = std::thread(&App::exposer_handler, this);
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

        //       timer1Min.run();
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
                zhub->fan(1);
                break;
            case 'f': // выключить вентилятор
                zhub->fan(0);
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

    if (httpThread.joinable())
        httpThread.join();

    gsbutils::stop(); // остановка вывода сообщений
}
