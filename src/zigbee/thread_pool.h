#ifndef THREAD_POOL_GSB
#define THREAD_POOL_GSB

#define MAX_THREADS_GSB 20
#define MAX_QUEUE_SIZE_GSB 2

class ThreadPool
{
public:

    ThreadPool()
    {
        init();
    }
    ~ThreadPool()
    {
    }

    void stop()
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
    // если очередь начинает заполняться, добавляем новый поток
    void add_command(Command cmd)
    {
        if (!flag.load())
            return;
        {
            std::lock_guard<std::mutex> lg(tqMtx);
            taskQueue.push(cmd);
            cv_queue.notify_one();
        }
    }

    // получение команды из очереди
    Command get_command()
    {
        std::unique_lock<std::mutex> ul(tqMtx);
        Command cmd((uint16_t)0);
        cv_queue.wait(ul, [this]()
                      { return !taskQueue.empty() || !flag.load(); });

        if (flag.load() && !taskQueue.empty())
        {
            cmd = taskQueue.front();
            taskQueue.pop();
        }
        ul.unlock();
        return cmd;
    }

    std::atomic<bool> flag{true};

private:
    // по умолчанию на старте сразу запускаем несколько потоков
    void init()
    {
        threadMap.reserve(MAX_THREADS_GSB);
        uint8_t hc = std::thread::hardware_concurrency();
        uint8_t tc = std::max(hc, (uint8_t)10);
        while (tc--)
        {
            std::thread *t = new std::thread(&ThreadPool::on_command, this);
            threadMap.push_back(t);
            threadCount_++;
        }
    }

protected:
    virtual void on_command(){};
    uint8_t threadCount_ = 0;               // текущее количество открытых потоков
    std::vector<std::thread *> threadMap{}; // вектор указателей на поток
                                            //   th_function cmd_function_;              // функция-обработчик
    std::queue<Command> taskQueue{};        // очередь команд
    std::mutex tqMtx{};                     // мьютех на запись/чтение очереди комманд
    std::condition_variable cv_queue;       // cv на очередь команд
};

#endif