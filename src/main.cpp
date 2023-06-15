
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
#include "app.h"
#include "main.h"

std::shared_ptr<App> app;

using gsb_utils = gsbutils::SString;

static void sig_int(int signo)
{
    syslog(LOG_ERR, (char *)"sig_int: %d .Shutting down...", signo);
    app->Flag.store(false);

    return; // тут сработает closeAll
}

static void signal_init(void)
{
    signal(SIGINT, sig_int);
    signal(SIGHUP, sig_int);
    signal(SIGTERM, sig_int);
    signal(SIGKILL, sig_int);
    //    signal(SIGSEGV, sig_int); // приводит к зацикливанию сервиса !!!
    
}

static void closeAll()
{
    app->stop_app();
}

///////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
    int ret = 0;

    atexit(closeAll);
    signal_init();
#ifdef DEBUG
    gsbutils::init(0, (const char *)"zhub2");
#else
    gsbutils::init(1, (const char *)"zhub2");
#endif
    gsbutils::set_debug_level(1); // 0 - Отключение любого дебагового вывода

    gsbutils::dprintf(1, "Start zhub2 v%s.%s.%s \n", Project_VERSION_MAJOR, Project_VERSION_MINOR, Project_VERSION_PATCH);
    app = std::make_shared<App>();
    if (!app->parse_config())
    {
        gsbutils::stop(); // остановка вывода сообщений
        return -100;
    }
    if (app->config.Mode == "debug" || app->config.Mode == "test")
        gsbutils::set_debug_level(3);

    if (app->object_create())
        app->start_app();

    app->stop_app();

    gsbutils::stop(); // остановка вывода сообщений
    return ret;
}

