// modem.cpp
//  Модуль связи с модемом SIM800
#include "version.h"
#include <string>
#include <cstring>
#include <atomic>
#include <mutex>
#include <array>
#include <queue>
#include <thread>
#include <chrono>
#include <set>
#include <optional>
#include <any>
#include <termios.h>

#include "../gsb_utils/gsbutils.h"
#include "../telebot32/src/tlg32.h"
#include "comport/unix.h"
#include "comport/serial.h"
#include "app.h"
#include "modem.h"

extern std::shared_ptr<App> app;

using gsbstring = gsbutils::SString;

//--------------------------------------------------------------
// TODO: сейчас все команды идут с одним идентификатором ОК,
// потому что модем другой ответ не посылает. Надо пробовать включить эхо
// и отслеживать в эмиттере связку команда + ОК
GsmModem::GsmModem()
{
  rx_buff_.reserve(PI_RX_BUFFER_SIZE);
  tx_buff_.reserve(PI_TX_BUFFER_SIZE);

  is_call_ = false;
  tone_cmd = "";
  tone_cmd_started = false;
}

GsmModem::~GsmModem()
{
  if (connected)
    disconnect();
}

// будет выполнять еще функцию stop
void GsmModem::disconnect()
{
  connected = false;
  execute_flag_.store(false);
  app->withSim800 = false;
  if (receiver_thread_.joinable())
    receiver_thread_.join();

  if (serial_->isOpen())
  {
    try
    {
      serial_->close();
    }
    catch (std::exception &error)
    {
    }
  }

  OnDisconnect(); // Посылаем сигнал, что порт отвалился
}

// функция потока обработки команд модема
void GsmModem::on_command(void *cmd_)
{
  std::vector<uint8_t> command = *(static_cast<std::vector<uint8_t> *>(cmd_));

  app->gsmModem->command_handler(command);
}

bool GsmModem::connect(std::string port, unsigned int baud_rate)
{
  serial_->setPort(port);
  serial_->setBaudrate(baud_rate);

  if (serial_->isOpen())
    return false;

  try
  {
    serial_->open();
    execute_flag_.store(true);

    threadPoolModem = std::make_shared<gsbutils::ThreadPool<std::vector<uint8_t>>>();
    uint8_t max_threads = 2;
    threadPoolModem->init_threads(&GsmModem::on_command, max_threads);

    receiver_thread_ = std::thread(&GsmModem::loop, this);
    connected = true;
    return true;
  }
  catch (std::exception &error)
  {
    gsbutils::dprintf(1, "GsmModem::connect: %s \n", error.what());
  }
  return false;
}

// Стартовая инициализация модема:
// устанавливаем ответ с эхом
// включаем АОН
// текстовый режим СМС
// включаем тональный режим для приема команд
// Очищаем очередь смс-сообщений
bool GsmModem::init_modem()
{
  if (app->withSim800)
  {
    send_command("AT\r", "At");
    set_echo(true);
    set_aon();
    send_command("AT+CMGF=1\r", "AT+CMGF");
    send_command("AT+DDET=1,0,0\r", "AT+DDET");
    send_command("AT+CMGD=1,4\r", "AT+CMGD"); // Удаление всех сообщений, второй вариант
    send_command("AT+COLP=1\r", "AT+COLP");
    return true;
  }
  else
  {
    return false;
  }
}

// фактическая отправка команды (синхронное выполнение)
bool GsmModem::send_command_(std::string command)
{
  if (!serial_->isOpen())
  {
    gsbutils::dprintf(1, "GsmModem::send_command_ порт закрыт\n");
    return false;
  }
  if (command.size() < 1)
    return false;

  bool res = false;
  try
  {
    std::lock_guard<std::mutex> transmit_lock(transmit_mutex_);
    tx_buff_.clear();
    for (uint8_t sym : command)
    {
      tx_buff_.push_back(sym);
    }
    size_t written = serial_->write(tx_buff_);
    if (written == tx_buff_.size())
      return true;
    else
      throw std::runtime_error("Ошибка записи команды\n");
  }
  catch (std::exception &e)
  {
    gsbutils::dprintf(1, "GsmModem::send_command_ exception: %s\n", e.what());
  }
  return false;
}

// Отправка команды с ожиданием ответа (синхронный запрос)
// Если команда не отправилась или не получен ответ, возвращается пустая строка
// Ответ, возможно, будет получен асинхронно
std::string GsmModem::send_command(std::string command, std::string id)
{
  sim800_event_emitter_.clear(id);

  if (send_command_(command))
  {
    return sim800_event_emitter_.wait(id, std::chrono::duration(std::chrono::seconds(5)));
  }
  else
  {
    return "";
  }
}

bool GsmModem::set_echo(bool echo)
{
  std::string cmd = std::string("ATE") + (echo ? "1" : "0") + "\r";
  return !(send_command(cmd, "ATE") == "");
}
bool GsmModem::set_echo()
{
  std::string cmd = "ATE0\r";
  return !(send_command(cmd, "ATE") == "");
}
bool GsmModem::set_aon()
{
  std::string cmd = "AT+CLIP=1\r";
  return !(send_command(cmd, "AT+CLIP") == "");
}

// Обработчик команд с модема
void GsmModem::command_handler(std::vector<uint8_t> &data)
{
  std::string res{};
  for (size_t i = 0; i < data.size(); i++)
  {
    if ((data.at(i) == '\r' || data.at(i) == '\n'))
    {
      // заменим на |
      res.push_back('|');
    }
    else
    {
      res.push_back(data.at(i));
    }
  }
  // Удалить ответ из одних пробелов
  if (res.size())
  {
    gsbutils::dprintf(1, "Принята команда %s \n", res.c_str());

    // та сторона положила трубку, надо прекратить обработку тоновых сигналов и сбросить команду
    if (res.find("NO_CARRIER") != std::string::npos)
    {
      is_call_ = false;
    }
    //
    // Проверим на основные незапрашиваемые ответы модема
    if (res.find("RING") != std::string::npos)
    {
      if (res.find("CLIP") != std::string::npos)
      {
        // АОН сработал в этом же ответе
        if (res.find(app->config.PhoneNumber) != std::string::npos) // TODO: номер в конфигурацию
        {
          // Мой номер, продолжаем дальше
          send_command("ATA\r", "ATA"); // отправка команды (синхронная, макс. 3сек.)
          is_call_ = true;
        }
        else
        {
          // Чужой номер, сбрасываем
          send_command("ATH0\r", "ATH0");
          is_call_ = false;
        }
      }
      // На RING не реагируем
    }
    else if (res.find("CMTI") != std::string::npos)
    {
      on_sms(res);
    }
    else if (res.find("CUSD") != std::string::npos)
    {
      // хотя приходит в ответ на команду AT+CUSD, но самой команды в ответе нет,
      //  поэтому обрабатываю в той ветке
      //  Баланс
      // Если OK и CUSD пришли в одном ответе и ОК после CUSD, это ответ на саму команду, а не баланс
      if (res.find("OK") != std::string::npos && res.find("CUSD:") == std::string::npos)
      {
        sim800_event_emitter_.emit("AT+CUSD", "OK"); //
      }
      else
      {
        // Это пришел баланс
        size_t pos = res.find("\"");
        pos++;
        size_t pos2 = res.find("00200440002E");
        if (pos2 == std::string::npos)
          res = res.substr(pos);
        else
          res = res.substr(pos, pos2 - pos);
        gsbstring::remove_all(res, "003");
        gsbstring::replace_all(res, "002E", ".");
        //       gsbutils::dprintf(1, "Balance: %s\n", res.c_str());
        balance_ = res;

        // отправляем баланс в телеграм
        if (app->withTlg)
          app->tlg32->send_message("Баланс: " + res + " руб.");

        // если была команда запроса баланса с тонового набора
        if (balance_to_sms)
        {
          // отправляем баланс в смс
          send_sms("Balance " + res + " rub");
          balance_to_sms = false;
        }
      }
    }
    else if (res.find("+CLIP") != std::string::npos)
    {
      // АОН при входящем звонке
      if (res.find(app->config.PhoneNumber) != std::string::npos) // TODO: телефон в настройки
      {
        send_command("ATA\r", "ATA");
        is_call_ = true;
      }
      else
      {
        send_command("ATH0\r", "ATH0");
        is_call_ = false;
      }
      // Далее реагируем только на DTMF-команды
    }
    else if (res.find("DTMF") != std::string::npos)
    {
      // Прием тональных команд
      on_tone_command(res);
    }
    else if (res.find("CMTE") != std::string::npos)
    {
      // Перегрев модуля, пока не обрабатываю
    }
    else if (res.find("VOLTAGE") != std::string::npos)
    {
      // Проблемы с питанием, пока не обрабатываю
    }
    else if (res.find(">") != std::string::npos)
    {
      gsbutils::dprintf(7, "Есть > \n");
      sim800_event_emitter_.emit(">", ">"); ///???
    }
    else
    {
      // Ответы на запросы, должны начинаться с команды.
      // AT
      // ATE1
      // AT+CLIP
      // ATH0
      // AT+CMGF
      // AT+DDET
      // AT+CMGD
      // AT+COLP
      // AT+CBC

      std::string inCmd = gsbstring::remove_after(res, "|");
      inCmd = gsbstring::remove_after(inCmd, "=");
      res = gsbstring::remove_before(res, "|");
#ifdef DEBUG
      gsbutils::dprintf(1, "Принят ответ %s на команду %s \n", res.c_str(), inCmd.c_str());
#endif
      if (inCmd == "AT" ||
          inCmd == "ATE1" ||
          inCmd == "ATH0" ||
          inCmd == "AT+CMGF" ||
          inCmd == "AT+DDET" ||
          inCmd == "AT+CMGD" ||
          inCmd == "AT+CLIP")
      {
        sim800_event_emitter_.emit(inCmd, "OK");
      }

      else if (inCmd == "AT+CMGR")
      {
        // В ответе от модема передается группа сообщений, номер телефона отправителя, дата и время отправки, текст сообщения
        on_sms_command(res);
      }

      else if (inCmd == "AT+CBC")
      {
        // ||+CBC: 0,100,4685||||OK||
        // Получение параметров батареи
        res = gsbstring::remove_before(res, ":");
        std::string answer = gsbstring::remove_after(res, "|");
        int charge = -1, level = -1, voltage = -1;
        sscanf(answer.c_str(), "%d,%d,%d", &charge, &level, &voltage);
        //        gsbutils::dprintf(1, "ПришлоCBC:  %s %d %d %d \n", answer.c_str(), charge, level, voltage);
        charge_ = charge == -1 ? charge_ : charge;
        level_ = level == -1 ? level_ : level;
        voltage_ = voltage == -1 ? voltage_ : voltage;
      }

      else
      {
        if (res.find("OK") != std::string::npos ||
            res.find("NO DIALTONE") != std::string::npos ||
            res.find("BUSY") != std::string::npos ||
            res.find("ERROR") != std::string::npos ||
            res.find("NO CARRIER") != std::string::npos ||
            res.find("NO ANSWER") != std::string::npos

        )
        {
          // Ответ будет распарсен в вызывающей функции
          sim800_event_emitter_.emit("OK", res);
        }
      }
    }
  }
}

void GsmModem::loop()
{
  while (execute_flag_.load())
  {
    try
    {
      if (serial_->waitReadable())
      {
        rx_buff_.resize(rx_buff_.capacity());
        rx_buff_.clear();
        rx_buff_.resize(serial_->read(rx_buff_, rx_buff_.capacity()));
        threadPoolModem->add_command(rx_buff_);
      }
      else
      {
        std::this_thread::sleep_for(std::chrono::seconds(5));
      }
    }
    catch (std::exception &error)
    {
      gsbutils::dprintf(1, "modem loop exception: %s\n", error.what());
    }
  }
}

// Здесь обрабатываем тоновые команды
void GsmModem::on_tone_command(std::string command)
{

  if (!is_call_)
  {
    tone_cmd = "";
    return;
  }
  if (!tone_cmd_started)
  {
    if (command.find("*") != std::string::npos)
    {
      tone_cmd_started = true;
    }
  }
  // Команду собираем только приняв *
  if (tone_cmd_started)
  {
    tone_cmd += command;
    // обрезаем все до последней *
    size_t pos = tone_cmd.find_last_of("*");
    tone_cmd = tone_cmd.substr(pos + 1);
#ifdef DEBUG
    gsbutils::dprintf(1, tone_cmd + "\n");
#endif
    gsbstring::remove_all(tone_cmd, " ");
    gsbstring::remove_all(tone_cmd, "|");
    gsbstring::remove_all(tone_cmd, "OK");
    gsbstring::remove_all(tone_cmd, "+");
    gsbstring::remove_all(tone_cmd, "DTMF:");
#ifdef DEBUG
    gsbutils::dprintf(1, tone_cmd + "\n");
#endif
    if (tone_cmd.find("#") != std::string::npos)
    {
      gsbstring::remove_all(tone_cmd, "#");

      // выполняем команду, она должна быть строго из 3 цифр, первая - 4
      if (tone_cmd.size() == 3 && tone_cmd[0] == '4')
      {
        gsbutils::dprintf(1, "Execute command: " + tone_cmd + "\n");
        execute_tone_command(tone_cmd);
      }
      else
      {
        gsbutils::dprintf(1, "Uncorrect command: " + tone_cmd + "\n");
      }
      tone_cmd = "";
    }
    send_command("ATH0\r", "ATH0"); // Повесить трубку
  }
}

// Обработчик смс-комманд
// Команда обязательно начинается с /cmnd, через один пробел код команды,
// аналогичный тоновым командам
// ||CMGR: "REC UNREAD","+70250109365","","22/09/03,12:42:54+12"||test 5||||OK||
// ||+CMGR: "REC UNREAD","+79050109365","","23/06/08,17:32:30+12"||/cmnd401||||OK||
void GsmModem::on_sms_command(std::string answer)
{
  gsbutils::dprintf(1, "on_sms_command: %s\n", answer.c_str());

  if (answer.find("7" + app->config.PhoneNumber) != std::string::npos && answer.find("/cmnd") != std::string::npos)
  {
    // принимаю команды пока только со своего телефона
    //    gsbutils::dprintf(1, "%s\n", answer.c_str());
    answer = gsbstring::remove_before(answer, "/cmnd");
    //    gsbutils::dprintf(1, "%s\n", answer.c_str());
    answer = gsbstring::remove_after(answer, "||||");
    //   gsbutils::dprintf(1, "%s\n", answer.c_str());
    gsbstring::remove_all(answer, " ");
    //    gsbutils::dprintf(1, "%s\n", answer.c_str());
    execute_tone_command(answer);
  }
  // AT+CMGD=1,4
  send_command(std::string("AT+CMGD=1,4\r"), "AT+CMGD"); // Удаление всех сообщений
}
// Обработчик приема смс. Получает уведомление о приходе смс и отправляет команду на чтение.
// ||CMTI: "SM",4||
// (Получено СМС сообщение)  +CMTI: "SM",4 Уведомление о приходе СМС.
//                            Второй параметр - номер пришедшего сообщения
void GsmModem::on_sms(std::string answer)
{
  //  gsbutils::dprintf(1, "%s\n", answer.c_str());
  answer = gsbstring::remove_before(answer, ",");
  // gsbutils::dprintf(1, "%s\n", answer.c_str());
  gsbstring::remove_all(answer, "||");
  // gsbutils::dprintf(1, "%s\n", answer.c_str());
  int num_sms = 0;
  int res = sscanf(answer.c_str(), "%d", &num_sms);

  //  gsbutils::dprintf(1, "on sms: %s num_sms: %d \n", answer.c_str(), num_sms);
  if (res != 1 || num_sms == 0)
    return;
  char buf[32]{};
  res = snprintf(buf, 32, "AT+CMGR=%d\r", num_sms);
  if (res > 0)
  {
    buf[res] = 0;
    std::string rd_sms = std::string(buf);
    send_command(rd_sms, "AT+CMGR");
    //    gsbutils::dprintf(1, "%s\n", buf);
  }
}

void GsmModem::OnDisconnect()
{
  gsbutils::dprintf(1, "GsmModem::OnDisconnect.\n");
  if (app->Flag.load())
  {
    app->withSim800 = false;
    app->zhub->send_tlg_message("Модем отключился.");
  }
}

// +CBC: 0,95,4134 OK
// Первый параметр: 0 - не заряжается 1 - заряжается
// Второй параметр: Процент заряда батареи
// Третий параметр: Напряжение питания модуля в милливольтах

// Если ответ не пришел за время тайм-аута, отдавать предыдущее сохраненное значение
// В обработчике ответа записывать  свойства класса
std::array<int, 3> GsmModem::get_battery_level(bool need_query)
{
  std::array<int, 3> result = {-1, -1, 1};

  if (need_query)
  {
    std::string cmd = "AT+CBC\r";
    send_command(cmd, "AT+CBC");
  }
  else
  {
    result[0] = charge_;
    result[1] = level_;
    result[2] = voltage_;
  }
  return result;
}

// запрос баланса, прямо тут ответа не будет
bool GsmModem::get_balance()
{
  send_command("AT+CMGF=1\r", "AT+CMGF");
  std::string cmd = "AT+CUSD=1,\"*100#\"\r";
  send_command(cmd, "AT+CUSD");
  return true;
}

// вызов на основной номер
bool GsmModem::master_call()
{
  std::string cmd = "ATD+7" + app->config.PhoneNumber + ";\r"; // TODO: номер в конфиг
  std::string res = send_command(cmd, "ATD");
  // При ожидании звонка и при сбросе звонка приходит пустой ответ
  // При ответе абонента приходит ответ COLP сразу после ответа
  gsbutils::dprintf(1, "Call answer: " + res + "\n");
  return !(res == "");
}

// AT+CMGS="+70250109365"         >                       Отправка СМС на номер (в кавычках), после кавычек передаем LF (13)
//>Water leak 1                  +CMGS: 15               Модуль ответит >, передаем сообщение, в конце передаем символ SUB (26)
//                                OK
//  Если в сообщении есть кириллица, нужно сменить текстовый режим на UCS2
//  и перекодировать сообщение
bool GsmModem::send_sms(std::string sms)
{
  std::string cmd = "";
  std::string res = "";

  cmd = "AT+CMGF=0\r";
  res = send_command(cmd, "AT+CMGF");

  sms = convert_sms(sms);
  unsigned txt_len = sms.size() / 2;
  unsigned msg_len = txt_len + 14;

  char buff[3]{0};
  snprintf(buff, 3, "%02X", txt_len);

  std::string msg = std::string("0011000B91") + std::string("9752109063F5") + std::string("0008C1");
  msg = msg + std::string(buff) + sms;

  snprintf(buff, 3, "%02X", msg_len);

  cmd = "AT+CMGS=" + std::to_string(msg_len) + "\r";
  res = send_command(cmd, "AT+CMGS");
  //
  // 00 - Длина и номер SMS центра. 0 - означает, что будет использоваться дефолтный номер.
  // 11 - SMS-SUBMIT
  // 00 - Длина и номер отправителя. 0 - означает что будет использоваться дефолтный номер.
  // 0B - Длина номера получателя (11 цифр)
  // 91 - Тип-адреса. (91 указывает международный формат телефонного номера, 81 - местный формат).
  //
  // 9752109063F5 - Телефонный номер получателя в международном формате. (Пары цифр переставлены местами, если номер с нечетным количеством цифр, добавляется F) 70250109365 -> 9752109063F5
  // 00 - Идентификатор протокола
  // 08 - Старший полубайт означает сохранять SMS у получателя или нет (Flash SMS),  Младший полубайт - кодировка(0-латиница 8-кирилица).
  // C1 - Срок доставки сообщения. С1 - неделя
  // 46 - Длина текста сообщения
  // Далее само сообщение в кодировке UCS2 (35 символов кириллицы, 70 байт, 2 байта на символ)

  //+ std::string("46")+ std::string("043F0440043804320435044200200445043004310440002C0020044D0442043E00200442043504410442043E0432043E043500200441043E043E043104490435043D04380435");
  //                                  043F0440043804320435044200200445043004310440002C0020044D0442043E00200442043504410442043E0432043E043500200441043E043E043104490435043D04380435//
  msg.push_back((char)0x1A);     // Ctrl+Z
  res = send_command(msg, "OK"); /// ??
                                 //  gsbutils::dprintf(1, "SMS answer2: " + res + "\n");
  std::this_thread::sleep_for(std::chrono::seconds(3));
  cmd = "AT+CMGF=1\r";
  res = send_command(cmd, "AT+CMGF");
  return !(res == "");
}

// Перекодировка кириллицы
// TODO: ёЁ Ё 0401 ё 0451
std::string GsmModem::convert_sms(std::string msg)
{
  char buff[5]{0};

  std::string result = "";
  /*
  gsbutils::dprintf(1, msg+"\n");
  wchar_t symbol;
    for(auto IT = msg.begin(); IT != msg.end(); IT++)
    {
      unsigned char symbol= *IT;
      std::cout << std::hex << (int) symbol << " ";
    }
    std::cout << std::endl;
    */
  for (unsigned i = 0; i < msg.size(); i++)
  {
    uint16_t r = 0;
    uint8_t sym = (uint8_t)msg.at(i);
    if (sym == 0xD0)
    {
      result = result + std::string("04");
      i++;
      sym = msg.at(i) - 0x80;
    }
    else if (sym == 0xD1)
    {
      result = result + std::string("04");
      i++;
      sym = msg.at(i) - 0x40;
    }
    else
    {
      result = result + std::string("00");
    }
    snprintf(buff, 5, "%02X", sym);
    result = result + std::string(buff);
  }

  return result;
}

// Выполнение тоновых команд
void GsmModem::execute_tone_command(std::string command)
{
  int code = 0;
  int num = std::sscanf(command.c_str(), "%d", &code);

  if (num != 1)
    return;

  switch (code)
  {
  case 401: // запрос баланса
  {
    balance_to_sms = true;
    get_balance();
  }
  break;
  case 412: // состояние датчиков протечки
  {
    std::string states = app->zhub->get_leak_state();
    send_sms(states);
  }
  break;
  case 423: // состояние датчиков движения
  {
    std::string states = app->zhub->get_motion_state();
    send_sms(states);
  }
  break;
  default:
    break;
  }
}
