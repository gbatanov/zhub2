//
// usb2pin.cpp
// created by Georgii Batanov, 2023
//
// использование адаптера USB-UART как устройства ввода/вывода с 2 пинами (1 вход, 1 выход)
//
#include <memory>
#include <string>
#include <vector>
#include "comport/unix.h"
#include "comport/serial.h"
#include "../gsb_utils/gsbutils.h"
#include "version.h"
#include "usb2pin.h"

Usb2pin::Usb2pin() {}
Usb2pin::~Usb2pin()
{
    disconnect();
}

bool Usb2pin::connect(std::string port)
{
    serial_->setPort(port);
    if (serial_->isOpen())
        return false;

    try
    {
        serial_->open();
    }
    catch (std::exception &e)
    {
#ifdef DEBUG
        gsbutils::dprintf(1, "Usb2Pin error: %s \n", e.what());
#endif
        return false;
    }
#ifdef DEBUG
    gsbutils::dprintf(1, "Usb2Pin open\n");
#endif

    return true;
}
void Usb2pin::disconnect()
{
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
}

bool Usb2pin::set_dtr(int level)
{
    return serial_->set_dtr(level);
}
int Usb2pin::get_dsr()
{
    return serial_->get_dsr();
}
int Usb2pin::get_cts()
{
    return serial_->get_cts();
}