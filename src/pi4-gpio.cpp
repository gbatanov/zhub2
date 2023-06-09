// Прога на основе libgpiod, работает
//  BCM / GPIO pin name GPIO20 - 38 pin на гребенке - датчик 220В
//  GPIO16 - 36 пин на гребенке - управление вентилятором охлаждения
//  GPIO26 - 37 пин - включение звонка
#include <thread>
#include <chrono>
#include <array>
#include <iostream>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <atomic>
#include <set>
#include <queue>
#include <memory>
#include <optional>
#include <any>
#include <termios.h>

#include "../gsb_utils/gsbutils.h"

#include "version.h"

#include "comport/unix.h"
#include "comport/serial.h"
#include "common.h"
#include "zigbee/zigbee.h"
#include "modem.h"
#include "http.h"
#include "httpserver.h"
#include "app.h"
#include "pi4-gpio.h"

extern App app;

#ifdef IS_PI
#include <gpiod.h>
struct gpiod_chip *chip = nullptr;

void initialize_gpio()
{
	// Открываем устройство
	chip = gpiod_chip_open("/dev/gpiochip0");
}

void close_gpio()
{
	// Закрываем устройство
	if (chip)
		gpiod_chip_close(chip);
}
// Значение может быть только 0 или 1, поэтому -1 можно вернуть как ошибку
// Пока так и не понял, как сделать программно подтяжку к питанию,
// поэтому реле переключается с земли на +3В через резистор, без этой цепочки не работает
int read_pin(int pin)
{
	struct gpiod_line *line;
	int req = -6;

	line = gpiod_chip_get_line(chip, pin);
	if (!line)
		return -2;

	if (gpiod_line_is_free(line))
	{
		req = gpiod_line_request_input(line, "gpio_pin_value");
		if (req)
			return -3;
	}

	int value = gpiod_line_get_value(line);
	return value;
}

int write_pin(int pin, int value)
{
	struct gpiod_line *line;
	int req = -1;
	value = value == 0 ? 0 : 1;

	line = gpiod_chip_get_line(chip, pin);
	if (!line)
		return -2;

	if (gpiod_line_is_free(line))
	{
		int req = gpiod_line_request_output(line, "fun", 0);
		if (req != 0)
			return -3;
	}
	req = gpiod_line_set_value(line, value);

	return req;
}

// функция потока наличия напряжения 220В
// на малинке определяет по наличию +3В на контакте 38(GPIO20),
// подается через реле, подключеннному к БП от 220В
// TODO: убрать вызовы функций, переписать на каналы
void power_detect()
{

	int value = -1; // -1 заведомо кривое значение, могут быть 0 или 1
	static bool notify_off = false;
	static bool notify_on = true;

	while (app.Flag.load())
	{
		value = read_pin(20);
		gsbutils::dprintf(7, "GPIO 20 %d\n", value);
		if (value == 0 && !notify_off)
		{
			notify_off = true;
			notify_on = false;
			app.zhub->handle_power_off(value);
		}
		else if (value == 1 && !notify_on)
		{
			notify_on = true;
			notify_off = false;
			app.zhub->handle_power_off(value);
		}

		using namespace std::chrono_literals;
		std::this_thread::sleep_for(10s);
	}
}
#else

void initialize_gpio(){}
void close_gpio(){}
int read_pin(int pin) { return -1; }
int write_pin(int pin, int value) { return -1; }
void power_detect(){}

#endif
