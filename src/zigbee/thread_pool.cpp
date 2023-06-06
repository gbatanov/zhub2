#include "../version.h"
#include <algorithm>
#include <mutex>
#include <thread>
#include <string>
#include <string.h>
#include <inttypes.h>
#include <sstream>
#include <array>
#include <set>
#include <queue>
#include <optional>
#include <memory>
#include <any>
#include <termios.h>
#include <functional>

#include "../version.h"
#include "../comport/unix.h"
#include "../comport/serial.h"
#include "../../gsb_utils/gsbutils.h"
#include "../common.h"
#include "zigbee.h"
#include "../main.h"

using namespace zigbee;

ThreadPool::ThreadPool()
{
}
ThreadPool::~ThreadPool()
{
    if (flag.load())
        stop_threads();
}

void ThreadPool::init_threads(thread_func handle)
{
    handle_ = handle;
    uint8_t tc = std::max((uint8_t)std::thread::hardware_concurrency(), (uint8_t)8);
    threadVec.reserve(tc);
    while (tc--)
        threadVec.push_back(new std::thread(&ThreadPool::on_command, this));
}

void ThreadPool::stop_threads()
{
    flag.store(false);
    cv_queue.notify_all();
    for (std::thread *t : threadVec)
        if (t->joinable())
            t->join();
}

// добавление команды в очередь
void ThreadPool::add_command(Command cmd)
{
    if (cmd.uid() != 0)
    {
        std::lock_guard<std::mutex> lg(tqMtx);
        taskQueue.push(cmd);
        cv_queue.notify_all();
    }
}

void ThreadPool::on_command()
{
    while (flag.load())
    {
        Command cmd = get_command();
        if (cmd.uid() != 0 && flag.load())
            handle_(cmd);
    }
}
// получение команды из очереди
Command ThreadPool::get_command()
{
    Command cmd;
#ifdef TEST
//    std::cout << "In get command " << get_thread_id() << std::endl;
#endif
    {
        std::lock_guard<std::mutex> lg(tqMtx);
        if (!taskQueue.empty())
        {
            cmd = taskQueue.front();
            taskQueue.pop();
#ifdef TEST
            //           std::cout << "Without wait get command " << get_thread_id() << std::endl;
#endif

            return cmd;
        }
    }

    std::unique_lock<std::mutex> ul(tqMtx);
    cv_queue.wait(ul, [this]()
                  { return !taskQueue.empty() || !flag.load(); });

    if (flag.load() && !taskQueue.empty())
    {
        cmd = taskQueue.front();
        taskQueue.pop();
    }
    ul.unlock();
#ifdef TEST
    //   std::cout << "After wait get command " << get_thread_id() << std::endl;
#endif

    return cmd;
}
#ifdef TEST
// Возвращаю 4 последних цифры ID потока
unsigned long long ThreadPool::get_thread_id()
{
#ifdef __MACH__
    return (unsigned long long)1;
#else
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::string id_str = ss.str();
    return std::stoull(id_str.substr(id_str.size() - 4));
#endif
}
#endif