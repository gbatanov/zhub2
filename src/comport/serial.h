#ifndef SERIAL_H
#define SERIAL_H

#define THROW(exceptionClass, message) throw exceptionClass(__FILE__, __LINE__, (message))

namespace serial
{

  class Serial : public serial::SerialImpl
  {
  public:
    Serial(const Serial &) = delete;
    Serial &operator=(const Serial &) = delete;
    Serial(const std::string &port = "",
           uint32_t baudrate = 9600);

    ~Serial();

    size_t read(std::vector<uint8_t> &buffer, size_t size = 1);
    size_t write(const std::vector<uint8_t> &data);
  };

} // namespace serial

#endif
