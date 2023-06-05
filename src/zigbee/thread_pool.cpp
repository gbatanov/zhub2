

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

#include "../version.h"
#include "../comport/unix.h"
#include "../comport/serial.h"
#include "../../gsb_utils/gsbutils.h"
#include "../common.h"
#include "zigbee.h"
#include "../main.h"

using namespace zigbee;

#define MAX_THREADS_GSB 20
#define MAX_QUEUE_SIZE_GSB 2

ThreadPool::ThreadPool()
{
}
ThreadPool::~ThreadPool()
{
}

void ThreadPool::stop()
{
    flag.store(false);
    cv_queue.notify_all();
    for (auto t : threadMap)
    {
        if (t->joinable())
            t->join();
    }
}

// добавление команды в очередь
void ThreadPool::add_command(Command cmd)
{
    if (flag.load())
    {
        std::lock_guard<std::mutex> lg(tqMtx);
        taskQueue.push(cmd);
        cv_queue.notify_all();
    }
}

// получение команды из очереди
Command ThreadPool::get_command()
{
    Command cmd((CommandId)0);

    if (flag.load())
    {
        {
            std::lock_guard<std::mutex> lg(tqMtx);
            if (!taskQueue.empty())
            {
                cmd = taskQueue.front();
                taskQueue.pop();
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
    }
    return cmd;
}

// по умолчанию на старте сразу запускаем несколько потоков
void ThreadPool::initThreads()
{

    threadMap.reserve(MAX_THREADS_GSB);
    uint8_t hc = std::thread::hardware_concurrency();
    uint8_t tc = std::max(hc, (uint8_t)6);
    while (tc--)
    {
        std::thread *t = new std::thread(&ThreadPool::on_command, this);
        threadMap.push_back(t);
        threadCount_++;
    }
}
