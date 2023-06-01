
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
#include "../gsb_utils/gsbutils.h"
#include "comport/unix.h"
#include "comport/serial.h"
#include "common.h"
#include "zigbee/zigbee.h"
#include "exposer.h"
#ifdef WITH_TELEGA
#include "../telebot32/src/tlg32.h"
#include "telega32.h"
extern std::unique_ptr<Tlg32> tlg32;
#endif

#ifdef WITH_HTTP
#include "httpserver.h"
#include "http.h"
extern std::unique_ptr<HttpServer> http;
#endif

#ifdef WITH_PROMETHEUS
#include "exposer.h"
#endif

#include "main.h"

std::string startTime{};
std::atomic<bool> Flag{true};
char *program_version = nullptr;

std::unique_ptr<zigbee::Zhub> zhub;
// gsbutils::TTimer ikeaMotionTimer(180, ikeaMotionTimerCallback);
gsbutils::CycleTimer timer1Min(60, timer1min);

#ifdef WITH_SIM800
#include "modem.h"
GsmModem *gsmmodem;
#endif
#ifdef IS_PI
#include "modem.h"
#endif

bool with_sim800 = false;
bool noAdapter = true;
int ret = 0;

using gsb_utils = gsbutils::SString;

//-------------------------------------------
// Callback функция таймера для датчика движения ИКЕА
void ikeaMotionTimerCallback()
{
    std::shared_ptr<zigbee::EndDevice> ed = zhub->get_device_by_mac(0x0c4314fffe17d8a8);
    if (ed)
        zhub->handle_motion(ed, 0);
}

static void sig_int(int signo)
{
    syslog(LOG_ERR, (char *)"sig_int: %d .Shutting down...", signo);
    Flag.store(false);

    return; // тут сработает closeAll
}

static void signal_init(void)
{
    signal(SIGINT, sig_int);
    signal(SIGHUP, sig_int);
    signal(SIGTERM, sig_int);
    signal(SIGKILL, sig_int);
    //   signal(SIGSEGV, sig_int); приводит к зацикливанию!!!
}

static void closeAll()
{
}

// Функция потока ожидания команд с клавиатуры
static int cmdFunc()
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

#ifdef WITH_SIM800
            case 'b': // баланс
            {
                if (gsmmodem->get_balance())
                {
                    gsbutils::dprintf(1, "Запрос баланса отправлен\n");
                }
            }
            break;
            case 'u': // данные по батарее SIM800
            {
                char answer[256]{};
                std::array<int, 3> battery = gsmmodem->get_battery_level();
                std::string charge = battery[0] == 1 ? "Заряжается" : "Не заряжается";
                std::string level = battery[1] == -1 ? "" : std::to_string(battery[1]) + "%";
                std::string volt = battery[2] == -1 ? "" : std::to_string((float)(battery[2] / 1000)) + "V";
                int res = std::snprintf(answer, 256, "SIM800l battery: %s, %s, %0.2f V\n", charge.c_str(), level.c_str(), battery[2] == -1 ? 0.0 : (float)battery[2] / 1000);
                if (res > 0 && res < 256)
                    answer[res] = 0;
                else
                    strcat(answer, (char *)"Ошибка получения инфо о батарее");
                gsbutils::dprintf(1, "%s\n", answer);
            }
            break;
#endif
            } // switch
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    return 0;
}

#ifdef WITH_PROMETHEUS
void exposer_handler()
{
    // 2.18.450 - сейчас IP фактически не используется, слушает запросы с любого адреса
    Exposer exposer(std::string("localhost"), 8092);
    exposer.start();
}
#endif
///////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{

#ifdef IS_PI
    initialize_gpio();
#endif
    atexit(closeAll);
    signal_init();

    Flag.store(true);
#ifdef DEBUG
    gsbutils::init(0, (const char *)"zhub2");
    gsbutils::set_debug_level(3); // 0 - Отключение любого дебагового вывода
#else
    gsbutils::init(1, (const char *)"zhub2");
    gsbutils::set_debug_level(1); // 0 - Отключение любого дебагового вывода
#endif
    startTime = gsbutils::DDate::current_time();
    gsbutils::dprintf(1, "Start zhub2 v%s.%s.%s \n", Project_VERSION_MAJOR, Project_VERSION_MINOR, Project_VERSION_PATCH);

    std::thread cmd_thread(cmdFunc); // поток приема команд с клавиатуры
    try
    {
#ifdef WITH_TELEGA
        tlg32 = std::make_unique<Tlg32>(BOT_NAME);
        tlg32->add_id(836487770);

        if (tlg32->run(&handle))
        { // Здесь будет выброшено исключение при отсутствии корректного токена
            tlg32->send_message("Программа перезапущена.\n");
        }
#endif

        try
        {
            zhub = std::make_unique<zigbee::Zhub>();
            noAdapter = zhub->init_adapter();
        }
        catch (std::exception &e)
        {
            noAdapter = true;
        }

        if (!noAdapter)
        {
            zhub->start(
#ifdef TEST
                zigbee::Controller::TEST_RF_CHANNELS
#else
                zigbee::Controller::DEFAULT_RF_CHANNELS
#endif
            );
        }

#ifdef WITH_SIM800
        try
        {
            gsmmodem = new GsmModem();
#ifdef __MACH__
            with_sim800 = gsmmodem->connect("/dev/cu.usbserial-0001", 9600); // 115200  19200 9600 7200
#else
            with_sim800 = gsmmodem->connect("/dev/ttyUSB0");
#endif
            if (!with_sim800)
            {
#ifdef WITH_TELEGA
                tlg32->send_message("Модем SIM800 не обнаружен.\n");
#endif
                if (gsmmodem)
                    delete gsmmodem;
            }
            else
            {
#ifdef WITH_TELEGA
                tlg32->send_message("Модем SIM800 активирован.\n");
#endif
                gsmmodem->init();
                gsmmodem->get_battery_level(true);
            }
        }
        catch (std::exception &e)
        {
            with_sim800 = false;
#ifdef WITH_TELEGA
            tlg32->send_message("Модем SIM800 не обнаружен.\n");
#endif
            if (gsmmodem)
                delete gsmmodem;
        }
        if (with_sim800)
            gsmmodem->get_balance(); // для индикации корректной работы обмена по смс
#endif

        if (!noAdapter)
        {
            timer1Min.run();

#ifdef IS_PI
            std::thread pwr_thread(power_detect);           // поток определения наличия 220В
            std::thread tempr_thread(get_main_temperature); // поток определения температуры платы Raspberry
#endif

#ifdef WITH_HTTP
            std::thread http_thread(http_server);
#endif
#ifdef WITH_PROMETHEUS
            std::thread exposer_thread(exposer_handler);
#endif

#ifdef WITH_HTTP
            if (http_thread.joinable())
                http_thread.join();
#endif
#ifdef WITH_PROMETHEUS
            if (exposer_thread.joinable())
                exposer_thread.join();
#endif
#ifdef IS_PI
            pwr_thread.join();
            tempr_thread.join();
#endif
        }
        ret = 0;
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        ret = 1;
    }
    cmd_thread.join();
    Flag.store(false);

#ifdef IS_PI
    close_gpio();
#endif
#ifdef WITH_TELEGA
    tlg32->stop();
#endif
    gsbutils::stop();
    zhub->stop();

    return ret;
}

// используется в телеграм
std::string show_statuses()
{
    std::string statuses = zhub->show_device_statuses(false);
    if (statuses.empty())
        return "Нет активных устройств\n";
    else
        return "Статусы устройств:\n" + statuses + "\n";
}

// timer 1 min - callback function
void timer1min()
{
    zhub->check_motion_activity();
}

// Примечания:
// Если на реле Aqara T1 три раза быстро нажать кнопку, отработает получение идентификатора устройства
// При включенном реле на кластере 000с на 15 эндпойнте отдается потребляемая нагрузкой мощность, через раз (через нулевое значение)
// При выключенном реле периодически идет команда в кластере Time
// Температуру реле можно получить по запросу аттрибута 0х0000 в кластере DEVICE_TEMPERATURE_CONFIGURATION
// На кранах воды нет датчиков температуры
