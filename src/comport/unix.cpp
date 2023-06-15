// linux and mac osx

#include <stdio.h>
#include <string.h>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <errno.h>
#include <paths.h>
#include <sysexits.h>
#include <termios.h>
#include <sys/param.h>
#include <memory>
#include <exception>
#include <stdexcept>
#include <limits>
#include <vector>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>

#if defined(__linux__)
#include <linux/serial.h>
#endif

#ifdef __MACH__
#include <AvailabilityMacros.h>
#include <mach/clock.h>
#include <mach/mach.h>
#if defined(MAC_OS_X_VERSION_10_3) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_3)
#include <IOKit/serial/ioss.h>
#endif
#endif

#include "unix.h"
#include "serial.h"

// необходимо для MAC OS
#ifndef TIOCINQ
#ifdef FIONREAD
#define TIOCINQ FIONREAD
#else
#define TIOCINQ 0x541B
#endif
#endif

using serial::MillisecondTimer;
using serial::Serial;
using serial::SerialImpl;
using std::invalid_argument;
using std::string;
using std::stringstream;

MillisecondTimer::MillisecondTimer(const uint32_t millis) : expiry(timespec_now())
{
  int64_t tv_nsec = expiry.tv_nsec + (millis * 1e6);
  if (tv_nsec >= 1e9)
  {
    int64_t sec_diff = tv_nsec / static_cast<int>(1e9);
    expiry.tv_nsec = tv_nsec % static_cast<int>(1e9);
    expiry.tv_sec += sec_diff;
  }
  else
  {
    expiry.tv_nsec = tv_nsec;
  }
}

// оставшееся время
int64_t MillisecondTimer::remaining()
{
  timespec now(timespec_now());
  int64_t millis = (expiry.tv_sec - now.tv_sec) * 1e3;
  millis += (expiry.tv_nsec - now.tv_nsec) / 1e6;
  return millis;
}

// текущее время с наносекундами
timespec MillisecondTimer::timespec_now()
{
  timespec time;
#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
  clock_serv_t cclock;
  mach_timespec_t mts;
  host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
  clock_get_time(cclock, &mts);
  mach_port_deallocate(mach_task_self(), cclock);
  time.tv_sec = mts.tv_sec;
  time.tv_nsec = mts.tv_nsec;
#else
  clock_gettime(CLOCK_MONOTONIC, &time);
#endif
  return time;
}

// структура timespec из миллисекунд
timespec timespec_from_ms(const uint32_t millis)
{
  timespec time;
  time.tv_sec = millis / 1e3;                          //  целое количество секунд
  time.tv_nsec = (millis - (time.tv_sec * 1e3)) * 1e6; // количество наносекунд
  return time;
}
///////////////////////////////////////////////////////////////////////
//
SerialImpl::SerialImpl(const string &port, unsigned long baudrate)
    : port_(port), fd_(-1), is_open_(false)
{
}

SerialImpl::~SerialImpl()
{
}

void SerialImpl::open()
{

  if (is_open_ == true)
  {
    return;
  }

  fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY);
  //  fd_ = ::open (port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK); // original

  if (fd_ == -1)
  {
    switch (errno)
    {
    case EINTR: // 4 Interrupted system call
      open();   // Recurse because this is a recoverable error.
      return;
    case ENOENT: // 2  No such file or directory
      throw std::runtime_error("Port is empty or not found.");
      return;
    default:
      throw std::runtime_error("Serial port opening error.");
      return;
    }
  }

  reconfigurePort();
  is_open_ = true;
}

void SerialImpl::reconfigurePort()
{
  if (fd_ == -1)
  {
    // Can only operate on a valid file descriptor
    throw std::runtime_error("Invalid file descriptor, is the serial port open?");
  }

  struct termios options; // The options for the file descriptor

  if (tcgetattr(fd_, &options) == -1)
  {
    throw std::runtime_error("::tcgetattr");
  }

  // set up raw mode / no echo / binary
  options.c_cflag |= (tcflag_t)(CLOCAL | CREAD);
  options.c_lflag &= (tcflag_t) ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG | IEXTEN); //|ECHOPRT

  options.c_oflag &= (tcflag_t) ~(OPOST);
  options.c_iflag &= (tcflag_t) ~(INLCR | IGNCR | ICRNL | IGNBRK);
#ifdef IUCLC
  options.c_iflag &= (tcflag_t)~IUCLC;
#endif
#ifdef PARMRK
  options.c_iflag &= (tcflag_t)~PARMRK;
#endif

  // setup baud rate
  bool custom_baud = false;
  speed_t baud;
  switch (baudrate_)
  {
#ifdef B0
  case 0:
    baud = B0;
    break;
#endif
#ifdef B50
  case 50:
    baud = B50;
    break;
#endif
#ifdef B75
  case 75:
    baud = B75;
    break;
#endif
#ifdef B110
  case 110:
    baud = B110;
    break;
#endif
#ifdef B134
  case 134:
    baud = B134;
    break;
#endif
#ifdef B150
  case 150:
    baud = B150;
    break;
#endif
#ifdef B200
  case 200:
    baud = B200;
    break;
#endif
#ifdef B300
  case 300:
    baud = B300;
    break;
#endif
#ifdef B600
  case 600:
    baud = B600;
    break;
#endif
#ifdef B1200
  case 1200:
    baud = B1200;
    break;
#endif
#ifdef B1800
  case 1800:
    baud = B1800;
    break;
#endif
#ifdef B2400
  case 2400:
    baud = B2400;
    break;
#endif
#ifdef B4800
  case 4800:
    baud = B4800;
    break;
#endif
#ifdef B7200
  case 7200:
    baud = B7200;
    break;
#endif
#ifdef B9600
  case 9600:
    baud = B9600;
    break;
#endif
#ifdef B14400
  case 14400:
    baud = B14400;
    break;
#endif
#ifdef B19200
  case 19200:
    baud = B19200;
    break;
#endif
#ifdef B28800
  case 28800:
    baud = B28800;
    break;
#endif
#ifdef B57600
  case 57600:
    baud = B57600;
    break;
#endif
#ifdef B76800
  case 76800:
    baud = B76800;
    break;
#endif
#ifdef B38400
  case 38400:
    baud = B38400;
    break;
#endif
#ifdef B115200
  case 115200:
    baud = B115200;
    break;
#endif
#ifdef B128000
  case 128000:
    baud = B128000;
    break;
#endif
#ifdef B153600
  case 153600:
    baud = B153600;
    break;
#endif
#ifdef B230400
  case 230400:
    baud = B230400;
    break;
#endif
#ifdef B256000
  case 256000:
    baud = B256000;
    break;
#endif
#ifdef B460800
  case 460800:
    baud = B460800;
    break;
#endif
#ifdef B500000
  case 500000:
    baud = B500000;
    break;
#endif
#ifdef B576000
  case 576000:
    baud = B576000;
    break;
#endif
#ifdef B921600
  case 921600:
    baud = B921600;
    break;
#endif
#ifdef B1000000
  case 1000000:
    baud = B1000000;
    break;
#endif
#ifdef B1152000
  case 1152000:
    baud = B1152000;
    break;
#endif
#ifdef B1500000
  case 1500000:
    baud = B1500000;
    break;
#endif
#ifdef B2000000
  case 2000000:
    baud = B2000000;
    break;
#endif
#ifdef B2500000
  case 2500000:
    baud = B2500000;
    break;
#endif
#ifdef B3000000
  case 3000000:
    baud = B3000000;
    break;
#endif
#ifdef B3500000
  case 3500000:
    baud = B3500000;
    break;
#endif
#ifdef B4000000
  case 4000000:
    baud = B4000000;
    break;
#endif
  default:
    custom_baud = true;
  }
  if (custom_baud == false)
  {
#ifdef _BSD_SOURCE
    ::cfsetspeed(&options, baud);
#else
    ::cfsetispeed(&options, baud);
    ::cfsetospeed(&options, baud);
#endif
  }

  // setup char len
  options.c_cflag &= (tcflag_t)~CSIZE;

  options.c_cflag |= CS8;
  // setup stopbits

  options.c_cflag &= (tcflag_t) ~(CSTOPB);
  // setup parity
  options.c_iflag &= (tcflag_t) ~(INPCK | ISTRIP);

  options.c_cflag &= (tcflag_t) ~(PARENB | PARODD);

  // setup flow control

  // xonxoff
#ifdef IXANY

  options.c_iflag &= (tcflag_t) ~(IXON | IXOFF | IXANY);
#else

  options.c_iflag &= (tcflag_t) ~(IXON | IXOFF);
#endif
  // rtscts
#ifdef CRTSCTS
  options.c_cflag &= (unsigned long)~(CRTSCTS);
#elif defined CNEW_RTSCTS
  options.c_cflag &= (unsigned long)~(CNEW_RTSCTS);
#else
#error "OS Support seems wrong."
#endif

  // http://www.unixwiz.net/techtips/termios-vmin-vtime.html
  // this basically sets the read call up to be a polling read,
  // but we are using select to ensure there is data available
  // to read before each call, so we should never needlessly poll
  options.c_cc[VMIN] = 0;
  options.c_cc[VTIME] = 0;

  // activate settings
  ::tcsetattr(fd_, TCSANOW, &options);

  // apply custom baud rate, if any
  if (custom_baud == true)
  {
    // OS X support
#if defined(MAC_OS_X_VERSION_10_4) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_4)
    // Starting with Tiger, the IOSSIOSPEED ioctl can be used to set arbitrary baud rates
    // other than those specified by POSIX. The driver for the underlying serial hardware
    // ultimately determines which baud rates can be used. This ioctl sets both the input
    // and output speed.
    speed_t new_baud = static_cast<speed_t>(baudrate_);
    // PySerial uses IOSSIOSPEED=0x80045402
    if (-1 == ioctl(fd_, IOSSIOSPEED, &new_baud, 1))
    {
      throw std::runtime_error("a");
    }
    // Linux Support
#elif defined(__linux__) && defined(TIOCSSERIAL)
    struct serial_struct ser;

    if (-1 == ioctl(fd_, TIOCGSERIAL, &ser))
    {
      throw std::runtime_error("b");
    }

    // set custom divisor
    ser.custom_divisor = ser.baud_base / static_cast<int>(baudrate_);
    // update flags
    ser.flags &= ~ASYNC_SPD_MASK;
    ser.flags |= ASYNC_SPD_CUST;

    if (-1 == ioctl(fd_, TIOCSSERIAL, &ser))
    {
      throw std::runtime_error("c");
    }
#else
    throw invalid_argument("OS does not currently support custom bauds");
#endif
  }

  // Update byte_time_ based on the new settings.
  uint32_t bit_time_ns = 1e9 / baudrate_;
  byte_time_ns_ = bit_time_ns * (1 + bytesize_ + parity_ + stopbits_);
}

void SerialImpl::close()
{
  if (is_open_ == true)
  {
    if (fd_ != -1)
    {
      int ret;
      ret = ::close(fd_);
      if (ret == 0)
        fd_ = -1;
      else
        throw std::runtime_error("SerialImpl::close");
    }
    is_open_ = false;
  }
}

bool SerialImpl::isOpen() const
{
  return is_open_;
}

size_t SerialImpl::available()
{
  if (!is_open_)
    return 0;

  size_t count = 0;
  if (-1 == ioctl(fd_, TIOCINQ, &count)) // Получает число байтов в буфере ввода. TIOCINQ Эквивалентно FIONREAD.
    throw std::runtime_error("SerialImpl::available");
  else
    return count;
}

bool SerialImpl::waitReadable(uint32_t timeout)
{
  if (timeout == 0)
    //  timeout = timeout_.read_timeout_constant;
    timeout = 100;

  // Setup a select call to block for serial data or a timeout
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(fd_, &readfds);
  timespec timeout_ts(timespec_from_ms(timeout));
  // Если timeout_ts = 0, pselect возвращается немедленно, если NULL, то ждет бесконечно
  int r = pselect(fd_ + 1, &readfds, NULL, NULL, &timeout_ts, NULL);
  return (r > 0 && FD_ISSET(fd_, &readfds));
}

// Точное ожидание времени получения count байтов
void SerialImpl::waitByteTimes(size_t count)
{
  timespec wait_time = {0, static_cast<long>(byte_time_ns_ * count)};
  pselect(0, NULL, NULL, NULL, &wait_time, NULL);
}

size_t SerialImpl::read(uint8_t *buf, size_t size)
{
  if (!is_open_)
  {
    throw std::runtime_error("Serial::read");
  }
  size_t bytes_read = 0;

  //  timeout in milliseconds
  // MillisecondTimer total_timeout(timeout_.read_timeout_constant);
  MillisecondTimer total_timeout(100);

  // Pre-fill buffer with available bytes
  {
    ssize_t bytes_read_now = ::read(fd_, buf, size);
    if (bytes_read_now > 0)
    {
      bytes_read = bytes_read_now;
    }
  }

  while (bytes_read < size)
  {
    int64_t timeout_remaining_ms = total_timeout.remaining();
    if (timeout_remaining_ms <= 0)
      break;

    // Timeout for the next select is whichever is less of the remaining
    // total read timeout and the inter-byte timeout.
    uint32_t timeout = std::min(static_cast<uint32_t>(timeout_remaining_ms), timeout_.inter_byte_timeout);
    // Wait for the device to be readable, and then attempt to read.
    if (waitReadable(timeout))
    {
      // If it's a fixed-length multi-byte read, insert a wait here so that
      // we can attempt to grab the whole thing in a single IO call. Skip
      // this wait if a non-max inter_byte_timeout is specified.
      if (size > 1 && timeout_.inter_byte_timeout == std::numeric_limits<uint32_t>::max())
      {
        size_t bytes_available = available();
        if (bytes_available + bytes_read < size)
        {
          waitByteTimes(size - (bytes_available + bytes_read));
        }
      }
      // This should be non-blocking returning only what is available now
      //  Then returning so that select can block again.
      ssize_t bytes_read_now = ::read(fd_, buf + bytes_read, size - bytes_read);
      // read should always return some data as select reported it was
      // ready to read when we get to this point.
      if (bytes_read_now < 1)
      {
        // Disconnected devices, at least on Linux, show the
        // behavior that they are always ready to read immediately
        // but reading returns nothing.
        throw std::runtime_error("device reports readiness to read but returned no data (device disconnected?)");
      }
      // Update bytes_read
      bytes_read += static_cast<size_t>(bytes_read_now);

      // If bytes_read == size then we have read everything we need
      if (bytes_read == size)
        break;
    }
  } // while
  return bytes_read;
}

size_t SerialImpl::write(const uint8_t *data, size_t length)
{
  if (is_open_ == false)
  {
    throw std::runtime_error("Serial::write");
  }
  fd_set writefds;
  size_t bytes_written = 0;

  // Calculate total timeout in milliseconds t_c + (t_m * N)
  long total_timeout_ms = timeout_.write_timeout_constant;
  MillisecondTimer total_timeout(total_timeout_ms);

  bool first_iteration = true;
  while (bytes_written < length)
  {
    int64_t timeout_remaining_ms = total_timeout.remaining();
    // Only consider the timeout if it's not the first iteration of the loop
    // otherwise a timeout of 0 won't be allowed through
    if (!first_iteration && (timeout_remaining_ms <= 0)) // Timed out
      break;

    first_iteration = false;

    timespec timeout(timespec_from_ms(timeout_remaining_ms));

    FD_ZERO(&writefds);
    FD_SET(fd_, &writefds);

    // Do the select
    int r = pselect(fd_ + 1, NULL, &writefds, NULL, &timeout, NULL);

    // Figure out what happened by looking at select's response 'r'
    // Error
    if (r < 0)
    {
      // Select was interrupted, try again
      if (errno == EINTR)
      {
        continue;
      }
      // Otherwise there was some error
      throw std::runtime_error("SerialImpl::write");
    }
    // Timeout
    if (r == 0)
      break;

    // Port ready to write
    if (r > 0)
    {
      // Make sure our file descriptor is in the ready to write list
      if (FD_ISSET(fd_, &writefds))
      {
        // This will write some
        ssize_t bytes_written_now = ::write(fd_, data + bytes_written, length - bytes_written);

        // even though pselect returned readiness the call might still be
        // interrupted. In that case simply retry.
        if (bytes_written_now == -1 && errno == EINTR)
          continue;

        // write should always return some data as select reported it was
        // ready to write when we get to this point.
        if (bytes_written_now < 1)
        {
          // Disconnected devices, at least on Linux, show the
          // behavior that they are always ready to write immediately
          // but writing returns nothing.
          std::stringstream strs;
          strs << "device reports readiness to write but "
                  "returned no data (device disconnected?)";
          strs << " errno=" << errno;
          strs << " bytes_written_now= " << bytes_written_now;
          strs << " bytes_written=" << bytes_written;
          strs << " length=" << length;
          throw std::runtime_error(strs.str().c_str());
        }
        // Update bytes_written
        bytes_written += static_cast<size_t>(bytes_written_now);
        // If bytes_written == size then we have written everything we need to
        if (bytes_written == length)
          break;

        // If bytes_written < size then we have more to write
        if (bytes_written < length)
          continue;

        // If bytes_written > size then we have over written, which shouldn't happen
        if (bytes_written > length)
          throw std::runtime_error("write over wrote, too many bytes where ");
      }
      // This shouldn't happen, if r > 0 our fd has to be in the list!
      throw std::runtime_error("select reports ready to write, but our fd isn't in the list!");
    }
  }
  return bytes_written;
}

void SerialImpl::setPort(const string &port)
{
  port_ = port;
}

void SerialImpl::setTimeout(serial::Timeout &timeout)
{
  timeout_ = timeout;
}

void SerialImpl::setBaudrate(unsigned long baudrate)
{
  baudrate_ = baudrate;
}

void SerialImpl::set_DTR(bool level)
{
  if (is_open_ == false)
  {
    throw std::runtime_error("Serial::setDTR");
  }

  int command = TIOCM_DTR;

  if (level)
  {
    if (-1 == ioctl(fd_, TIOCMBIS, &command))
    {
      stringstream ss;
      ss << "setDTR failed on a call to ioctl(TIOCMBIS): " << errno << " " << strerror(errno);
      throw std::runtime_error(ss.str().c_str());
    }
  }
  else
  {
    if (-1 == ioctl(fd_, TIOCMBIC, &command))
    {
      stringstream ss;
      ss << "setDTR failed on a call to ioctl(TIOCMBIC): " << errno << " " << strerror(errno);
      throw std::runtime_error(ss.str().c_str());
    }
  }
}
