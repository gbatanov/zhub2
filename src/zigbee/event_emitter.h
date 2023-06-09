#ifndef EVENT_EMITTER_H
#define EVENT_EMITTER_H

#include <chrono>
#include <mutex>
#include <condition_variable>
#include <map>

class Event
{

public:
    Event() {}
    ~Event() { set(); }

    // ждем заданное время,
    // если переменная установлена, возвращаем true,
    // иначе, если тайм-аут - false
    bool wait(std::chrono::duration<int, std::milli> timeout)
    {
        std::unique_lock<std::mutex> lock(m);
        return cond_var.wait_for(lock, timeout, [this]
                                 { return this->antiSpur_ == 1; });
    }

    // уведомление всех ожидающих условную переменную
    void set()
    {
        {
            std::lock_guard<std::mutex> lg(m);
            antiSpur_ = 1;
        }
        cond_var.notify_all();
    }
    void reset()
    {
        std::lock_guard<std::mutex> lg(m);
        antiSpur_ = 0;
    }
    int antiSpur_ = 0;

private:
    Event(const Event &copiedObject) = delete;
    Event &operator=(const Event &event) = delete;

    std::condition_variable cond_var;
    std::mutex m;
};

// eventCommand_ отслеживает поступление ответов на отправленную команду
// event_id - ID комманды
class EventCommand
{
public:
    struct Listener
    {
        std::shared_ptr<Event> event;      // событие
        std::shared_ptr<Command> argument; // аргумент, связанный с событием
    };

    // устанавливает событие с заданным ID и аргументом
    void emit(CommandId event_id, Command argument)
    {
#ifdef TEST
        gsbutils::dprintf(7, "Event emit %04x\n", (uint16_t)event_id);
#endif
        Listener listener = getListener(event_id);
        std::lock_guard<std::mutex> lock(argument_mutex);
        *(listener.argument) = argument;
        listener.event->set();
    }

    // Получаем по заданному ID событие, ждем установки условной переменной, связанной с этим событием
    // Если оно произошло, возвращаем сохраненную в событии команду, иначе по тайм-ауту возвращаем nullopt
    std::optional<Command> wait(CommandId event_id, std::chrono::duration<int, std::milli> timeout)
    {
#ifdef TEST
        gsbutils::dprintf(7, "Event wait %04x\n", (uint16_t)event_id);
#endif
        Listener listener = getListener(event_id);

        if (!listener.event->wait(timeout))
            return std::nullopt;
        else
        {
            std::lock_guard<std::mutex> lock(argument_mutex);
            return *(listener.argument);
        }
    }

    void clear(CommandId event_id)
    {
        Listener listener = getListener(event_id);

        std::lock_guard<std::mutex> lock(argument_mutex);
        *(listener.argument) = Command();
        listener.event->reset();
    }

private:
    // возвращаем аргумент события для заданного ID события.
    // Если такого события не зарегистрировано, создаем его с пустым аргументом и возвращаем его.
    Listener getListener(CommandId event_id)
    {
        std::lock_guard<std::mutex> lock(find_mutex);

        if (!listeners.count(event_id))
            listeners.insert(std::pair<CommandId, Listener>(event_id, {std::make_shared<Event>(), std::make_shared<Command>()}));

        return (listeners.find(event_id))->second;
    }

    std::map<CommandId, Listener> listeners;

    std::mutex find_mutex, argument_mutex;
};

// sim800_emitter_ отслеживает ответ на команды в сторону SIM800
class Sim800Event
{
public:
    struct Listener
    {
        std::shared_ptr<Event> event;          // событие
        std::shared_ptr<std::string> argument; // строка с ответом
    };

    // устанавливает событие с заданным кодом команды и ответом
    void emit(std::string event_id, std::string argument)
    {
        Listener listener = getListener(event_id);

        std::lock_guard<std::mutex> lock(argument_mutex);
        *(listener.argument) = argument;
        listener.event->set();
    }

    // Получаем по заданному ID событие, ждем установки условной переменной, связанной с этим событием
    // Если оно произошло, возвращаем сохраненный в событии ответ, иначе по тайм-ауту возвращаем пустую строку
    std::string wait(std::string event_id, std::chrono::duration<int, std::milli> timeout)
    {
        Listener listener = getListener(event_id);

        if (!listener.event->wait(timeout))
            return "";
        else
        {
            std::lock_guard<std::mutex> lock(argument_mutex);
            return *(listener.argument);
        }
    }

    void clear(std::string event_id)
    {
        Listener listener = getListener(event_id);

        std::lock_guard<std::mutex> lock(argument_mutex);
        *(listener.argument) = "";
        listener.event->reset();
    }

private:
    // возвращаем аргумент события для заданной команды SIM800.
    // Если такого события не зарегистрировано, создаем его с пустым аргументом и возвращаем его.
    Listener getListener(std::string event_id)
    {
        std::lock_guard<std::mutex> lock(find_mutex);

        if (!listeners.count(event_id))
            listeners.insert(std::pair<std::string, Listener>(event_id, {std::make_shared<Event>(), std::make_shared<std::string>()}));

        return (listeners.find(event_id))->second;
    }

    std::map<std::string, Listener> listeners;

    std::mutex find_mutex, argument_mutex;
};


#endif
