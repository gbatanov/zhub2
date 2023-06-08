#ifndef GSB_APP_H
#define GSB_APP_H


class App
{
public:
    App(){};
    ~App(){};
    bool object_create();
    bool startApp();
    void stopApp();
    bool init_modem();
    int cmdFunc();
    //    gsbutils::CycleTimer timer1Min(60, &timer1min);
    static void timer1min();
    std::shared_ptr<zigbee::Zhub> zhub;
    std::shared_ptr<GsmModem> gsmModem;
    std::shared_ptr<gsbutils::ThreadPool<std::vector<uint8_t>>> tpm;

    void exposer_handler();

    char *program_version = nullptr;
    std::atomic<bool> Flag{true};
    std::string startTime{};
    bool with_sim800 = false;
    bool noAdapter;
    std::thread cmd_thread; // поток приема команд с клавиатуры
    std::thread exposer_thread; // поток ответа прометею
    std::thread http_thread;
};
#endif
