#ifndef THREAD_POOL_GSB
#define THREAD_POOL_GSB

typedef void (*thread_func)(Command);

class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    void stop_threads();
    void add_command(Command cmd);
    std::atomic<bool> flag{true};
    void init_threads(thread_func handle);
    Command get_command();

protected:
    std::vector<std::thread *> threadVec{}; // вектор указателей на поток
    std::queue<Command> taskQueue{};        // очередь команд
    std::mutex tqMtx{};                     // мьютех на запись/чтение очереди комманд
    std::condition_variable cv_queue;       // cv на очередь команд
private:
    thread_func handle_;
    void on_command();
#ifdef TEST
    unsigned long long get_thread_id();
#endif
};

#endif