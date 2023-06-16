#ifndef GSB_USB2PIN_H
#define GSB_USB2PIN_H

class Usb2pin
{
public:
    Usb2pin();
    ~Usb2pin();
    bool connect(std::string port);
    void disconnect();

    bool set_dtr(int level);
    int get_dsr();
    int get_cts();

private:
    std::unique_ptr<serial::Serial> serial_ = std::make_unique<serial::Serial>();
};

#endif