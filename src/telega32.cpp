// telega32
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <atomic>
#include <queue>
#include <utility>
#include <mutex>
#include <unordered_set>
#include <syslog.h>
#include <set>
#include <optional>
#include <any>
#include <termios.h>

#include "comport/unix.h"
#include "comport/serial.h"
#include "../gsb_utils/gsbutils.h"
#include "common.h"
#include "main.h"
#include "../telebot32/src/tlg32.h"
#include "telega32.h"

#ifdef WITH_SIM800
#include "modem.h"
extern GsmModem *gsmmodem;
#endif

extern std::atomic<bool> Flag;
// extern Zhub *zhub;
std::unique_ptr<Tlg32> tlg32;

// Здесь реализуется вся логика обработки принятых сообщений
// Указатель на функцию передается в класс Tlg32
void handle(Message msg)
{
    if (Flag.load() && !msg.text.empty())
    {
        Message answer{};
        answer.chat.id = msg.from.id;

        DBGLOG("Входное сообщение %s: %s \n", msg.from.firstName.c_str(), msg.text.c_str());
        if (!tlg32->client_valid(msg.from.id))
        {
            answer.text = "Я не понял Вас.\n";
            bool ret = tlg32->send_message(answer);
        }
        if (msg.text.starts_with("/start"))
            answer.text = "Привет, " + msg.from.firstName;
        else if (msg.text.starts_with("/stat"))
        {
            std::string stat = show_statuses();
            if (stat.size() == 0)
                answer.text = "Нет ответа\n";
            else
                answer.text = stat;
        }
        else if (msg.text.starts_with("/balance"))
        {
#ifdef WITH_SIM800
            if (gsmmodem->get_balance())
                answer.text = "Запрос баланса отправлен\n";
#else
            answer.text = "SIM800 не подключен\n";
#endif
        }
        else if (msg.text.starts_with("/join"))
        {
            //            zhub->permitJoin(60s)
            answer.text = "Join 60 sec.\n";
        }
        else
            answer.text = "Я не понял Вас.\n";

        bool ret = tlg32->send_message(answer);

        if (!ret)
            ERRLOG("Failed to send message \n");
    }
}
