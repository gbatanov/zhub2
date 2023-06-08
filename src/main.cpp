
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
extern std::unique_ptr<HttpServer> http;

#include "exposer.h"
#include "app.h"
#include "main.h"

App app;

using gsb_utils = gsbutils::SString;

static void sig_int(int signo)
{
    syslog(LOG_ERR, (char *)"sig_int: %d .Shutting down...", signo);
    app.Flag.store(false);

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


///////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
    int ret = 0;

    atexit(closeAll);
    signal_init();
#ifdef DEBUG
    gsbutils::init(0, (const char *)"zhub2");
    gsbutils::set_debug_level(3); // 0 - Отключение любого дебагового вывода
#else
    gsbutils::init(1, (const char *)"zhub2");
    gsbutils::set_debug_level(1); // 0 - Отключение любого дебагового вывода
#endif

    gsbutils::dprintf(1, "Start zhub2 v%s.%s.%s \n", Project_VERSION_MAJOR, Project_VERSION_MINOR, Project_VERSION_PATCH);
    if (app.object_create())
        app.startApp();

    try
    {
        if (!app.noAdapter)
        {

#ifdef IS_PI
            std::thread pwr_thread(power_detect);           // поток определения наличия 220В
            std::thread tempr_thread(get_main_temperature); // поток определения температуры платы Raspberry
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
    if (app.cmdThread.joinable())
        app.cmdThread.join();

    app.stopApp();
    return ret;
}

// Примечания:
// Если на реле Aqara T1 три раза быстро нажать кнопку, отработает получение идентификатора устройства
// При включенном реле на кластере 000с на 15 эндпойнте отдается потребляемая нагрузкой мощность, через раз (через нулевое значение)
// При выключенном реле периодически идет команда в кластере Time
// Температуру реле можно получить по запросу аттрибута 0х0000 в кластере DEVICE_TEMPERATURE_CONFIGURATION
// На кранах воды нет датчиков температуры
