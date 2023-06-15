# Проект домашней охранной сигнализации zhub2
Устройство управления - Raspberry Pi 4 64x Ubuntu 22 server aarch64. Без специфических для малинки функций работает на Linux и MacOs (В дальнейшем предполагается отказ от использования GPIO и пинов Raspberry в пользу большей переносимости между разными системами).

## Модем SIM800l

Важно! Cим-карту вставлять ключом наружу!!!

Управление модемом:

| Команда   | Ответ        |   Описание |
|-----------|--------------|--------------------------------|
| AT+CPAS   | +CPAS:0      | <p>Информация о состоянии модуля: |
|           | OK        | 0 - готов к работе            |
|           |               | 2 - неизвестно |
|           |               |3 - входящий звонок |
|           |              | 4 - голосовое соединение |
| AT+CSQ    | +CSQ: 17,0   | Уровень сигнала |
|           | OK           | 0 - < -115 дБ |
|           |              |   1 - -112 дБ |
|           |              |    2-30 -110...-54 дБ|
|           |              |    31 - > -52 дБ|
|           |              |    99 - нет сигнала|
| AT+CBC    | +CBC: 0,95,4134 | Монитор напряжения питания модуля|
|           | OK              | Первый параметр:|
|           |                 | 0 - не заряжается|
|           |                 | 1 - заряжается|
|           |                 | Второй параметр:|
|           |                 | Процент заряда батареи|
|           |                 | Третий параметр:|
|           |                 | Напряжение питания модуля в милливольтах|
| AT+CLIP=1 | OK              | АОН 1 - вкл, 0 - выкл|
| AT+CCLK="13/09/25,13:25:33+05" | ОК | Установка часов «yy/mm/dd,hh:mm:ss+zz»|
| ATD+70250109365; | OK        |   Позвонить на номер +70250109365|
|                 | NO DIALTONE | Нет сигнала|
|                 | BUSY        | Вызов отклонен|
|                 | NO CARRIER  | Повесили трубку|
|                 | NO ANSWER   | Нет ответа|
| ATA              | OK          | Ответить на звонок|
| ATH0             | OK          | Повесить трубку|
| (Входящий звонок) | RING        |                        Входящий звонок|
|                    |             |                     При включенном АОН:|
|                  | +CLIP: "+70250109365",145,"",,"",0 | Номер телефона,(другие параметры мне не интересны)|
| AT+CMGF=1       | OK          |  1 - Включить текстовый режим|
| AT+CSCS="GSM"  | OK           | Кодировка GSM|
| AT+CMGS="+70250109365" |        >  |                     Отправка СМС на номер (в кавычках), после кавычек передаем LF (13)|
| >Water leak 1           |       +CMGS: 15 |              Модуль ответит >, передаем сообщение, в конце передаем символ SUB (26)|
|                          |     OK | |
| (Получено СМС сообщение)  |     +CMTI: "SM",4 |          Уведомление о приходе СМС.|
|                            |                   |        Второй параметр - номер пришедшего сообщения|
| AT+CMGR=2                   |   +CMGR: "REC READ","+790XXXXXXXX","","13/09/21,11:57:46+24"| Чтение СМС сообщений. |
|                              | cmnd1                                                     | В параметре передается номер сообщения.|
|                              | OK                                                        | В ответе передается группа сообщений, |
|                              |                                                           | номер телефона отправителя,|
|                              |                                                           | дата и время отправки, текст сообщения|
| AT+CMGDA="DEL ALL"            | OK      |                 Удаление всех сообщений |
| AT+CMGD=4                     | ОК       |                Удаление указанного сообщения|
| ATD*100#;                     | OK        |                Запрос баланса, баланс приходит в ответе.|
|                              | +CUSD: 0,"Balance:240,68r ", | |
| AT+DDET=<mode>[,<interval>][,<reportMode>][,<ssdet>] | OK  |Включение режима DTMF|
|                                                      |    | mode:  0 - выключен, 1 - включен|
|                                                      |   | interval - минимальный интервал в миллисекундах между двумя нажатиями одной и той же клавиши (диапазон допустимых значений 0-10000). По умолчанию — 0.|
|                                                      |   | reportMode: режим предоставления информации:0 — только код нажатой кнопки, 1 — код нажатой кнопки и время удержания нажатия, в мс|
|                                                      |   | ssdet - не используем|
|                                                      |   |В ответе:|
|                                                      |   |Если <reportMode>=0, то: +DTMF: <key>|
|                                                      |   |Если <reportMode>=1, то: +DTMF: <key>,<last time>|
|                                                      |   |<key> — идентификатор нажатой кнопки (0-9, *, #, A, B, C, D)|
|                                                      |   |<last time> — продолжительность удержания нажатой кнопки, в мс|


| Уведомление |	                Описание	|                                        Пример |
|-------------|-----------------------------|-----------------------------------------------|
| RING	   |                 Уведомление входящего вызова	 |                   RING|
| +CMTI	   |                 Уведомление прихода нового SMS-сообщения	|        +CMTI: "SM",2|
| +CLIP	   |                 Автоопределитель номера во время входящего звонка|	+CLIP: "+78004522441",145,"",0,"",0|
| +CUSD	   |                 Получение ответа на отправленный USSD-запрос	   | +CUSD: 0, " Vash balans 198.02 r.|
|          |                                                                    |  Dlya Vas — nedelya besplatnogo SMS-obsh'eniya s druz'yami! Podkl.: *319#", 15|
| UNDER-VOLTAGE POWER DOWN | | |
| UNDER-VOLTAGE WARNNING | | |
| OVER-VOLTAGE POWER DOWN | | |
| OVER-VOLTAGE WARNNING	    | Сообщения о некорректном напряжении модуля	 |  |    
| UNDER-VOLTAGE WARNNING| | |
| +CMTE	                   | Сообщения о некорректной температуре модуля	  |      +CMTE: 1|

## Примечания к командам
- На длительность нажатия на тональную кнопку ориентироваться не стоит, сильно нестабильно и незакономерно.

## Инструкция по созданию клона SD-карты на маке
### Создание клона

1. Вставьте SD-карту в ваш Mac с помощью USB или встроенного кард-ридера. Теперь откройте окно терминала и введите команду
```
 diskutil list 
```
Попробуйте определить идентификатор устройства вашей SD-карты. (Например, /dev/disk5 )
2. Размонтируйте вашу SD-карту:
```
 diskutil unmountDisk /dev/disk5 
```
Здесь замените disk5 именем вашей SD-карты, которое вы определили в шаге 1.

3. Используйте команду dd чтобы записать образ на жесткий диск. Например:
```
sudo dd if=/dev/disk5 of=~/pi4_backup.img
```
Здесь параметр if (входной файл) указывает файл для клонирования. (Например, /dev/disk5, это имя устройства  SD-карты. Замените его на имя вашего устройства). Параметр of (выходной файл) указывает имя файла для записи.

Примечание . Будьте внимательны и дважды проверьте параметры перед выполнением команды dd, так как ввод неправильных параметров здесь может потенциально уничтожить данные на ваших дисках.

Вы не увидите никаких выводов команды до завершения клонирования, и это может занять некоторое (очень долгое время на USB2!) время, в зависимости от размера вашей SD-карты. Может создаться впечатление, что процесс завис, но это не так. В конце концов он завершится.

### Восстановить Raspberry Pi SD Card

1. Вставьте SD-карту в ваш Mac. Откройте окно терминала и размонтируйте его, используя следующую команду:
```
diskutil unmountDisk /dev/disk5 
```
Здесь замените disk3 именем вашей SD, которое вы определили в шаге 1 предыдущего раздела.

2. Используйте команду dd для записи файла образа на SD-карту:
```
sudo dd if=~/pi4_backup.img of=/dev/disk5 
```
Это похоже на команду, которую мы использовали для создания клона, но в обратном порядке . На этот раз входной файл if является резервной копией, а выходной файл - устройством SD-карты.

Снова, проверьте и дважды проверьте параметры здесь, поскольку ввод неправильной команды здесь вызовет постоянную потерю данных.

Как только запись будет завершена, вы увидите подтверждение от dd. Затем вы можете извлечь карту из вашего Mac и использовать ее в Raspberry Pi.


##### Примечания:
- На маках папка по умолчанию для поиска программ и библиотек /opt/local, в отличие от Linux, где /usr/local
- 38 ножка задействована для определения наличия 220В. Реле подключается к БП 220В/5В. Средний контакт на 38 ножку гребенки, НЗ контакт на общий провод, НО на +3,3В через резистор 30кОм ( В дальнейшем будет использоваться датчик напряжения с zigbee).
- 36 ножка задействована для управления вентилятором охлаждения. При температуре платы больше 70 градусов, вентилятор включается, при понижении до 50 градусов выключается (В дальнейшем будет использоваться адаптер USB-UART с использованием сигналов управления).
- <p>Для токена телеграм-бота создавать файл .token_<BOT_NAME> в который необходимо скопировать токен.<br>При инсталляции этот файл скопируется в папку /usr/local/etc/telebot32/.token<BOT_NAME>/.token</p>
-  в маке пишет в сислог, но посмотреть можно только командой в консоли: 
```
 log show --info --debug --predicate "process == 'zhub2'"
 или
 log show --info --debug  --process zhub2 --last 10m
```
- Если на реле Aqara T1 три раза быстро нажать кнопку, отработает получение идентификатора устройства
- При включенном реле на кластере 0x000C (ANALOG_INPUT) на 21 эндпойнте отдается потребляемая нагрузкой мощность 
- При выключенном реле периодически идет команда в кластере Time
- Температуру реле можно получить по запросу аттрибута 0х0000 в кластере DEVICE_TEMPERATURE_CONFIGURATION
- На кранах воды нет датчиков температуры
- https://ru.wikipedia.org/wiki/Атмосферное_давление На небольших высотах каждые 12 м подъёма уменьшают атмосферное давление на 1 мм рт. ст. На больших высотах эта закономерность нарушается.
- Changelog вынесен в CHANGELOG.md
