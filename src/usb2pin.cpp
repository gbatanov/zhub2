//
// usb2pin.cpp
// created by Georgii Batanov, 2023
//
// использование адаптера USB-UART как устройства ввода/вывода с 2 пинами (1 вход, 1 выход)
//
#include <memory>
#include "comport/unix.h"
#include "comport/serial.h"
#include "usb2pin.h"

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
        return false;
    }
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