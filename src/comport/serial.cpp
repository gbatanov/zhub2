#include <string>
#include <algorithm>
#include <iostream>
#include <atomic>
#include <memory>
#include <mutex>
#include <limits>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <exception>
#include <stdexcept>
#include <termios.h>

#include "unix.h"
#include "serial.h"

std::atomic<bool> serFlag{true};

namespace serial
{

  Serial::Serial(const string &port, uint32_t baudrate) : serial::SerialImpl(port, (unsigned long)baudrate)
  {
  }

  Serial::~Serial()
  {
  }

  size_t Serial::read(std::vector<uint8_t> &buffer, size_t size)
  {
    uint8_t *buffer_ = new uint8_t[size];
    size_t bytes_read = 0;


    try
    {
      bytes_read = SerialImpl::read(buffer_, size);
    }
    catch (const std::exception &e)
    {
      delete[] buffer_;
      throw;
    }

    buffer.insert(buffer.end(), buffer_, buffer_ + bytes_read);
    delete[] buffer_;
    return bytes_read;
  }

  size_t Serial::write(const std::vector<uint8_t> &data)
  {
    return SerialImpl::write(&data[0], data.size());
  }

}