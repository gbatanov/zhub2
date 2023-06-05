#ifndef THREAD_POOL_GSB
#define THREAD_POOL_GSB

#define MAX_THREADS_GSB 20
#define MAX_QUEUE_SIZE_GSB 2

class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    void stop();
    void add_command(Command cmd);
    std::atomic<bool> flag{true};
    void initThreads();

protected:
    Command get_command();
    virtual void on_command(){};
    uint8_t threadCount_ = 0;               // текущее количество открытых потоков
    std::vector<std::thread *> threadMap{}; // вектор указателей на поток
    std::queue<Command> taskQueue{};        // очередь команд
    std::mutex tqMtx{};                     // мьютех на запись/чтение очереди комманд
    std::condition_variable cv_queue;       // cv на очередь команд
};

#endif