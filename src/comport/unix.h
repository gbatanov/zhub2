#ifndef SERIAL_IMPL_UNIX_H
#define SERIAL_IMPL_UNIX_H

#include <termios.h>

namespace serial
{

    using std::invalid_argument;
    using std::size_t;
    using std::string;

    struct Timeout
    {
        uint32_t inter_byte_timeout;
        uint32_t read_timeout_constant;
        uint32_t write_timeout_constant;

        explicit Timeout(uint32_t inter_byte_timeout_ = 0,
                         uint32_t read_timeout_constant_ = 0,
                         uint32_t write_timeout_constant_ = 0)
            : inter_byte_timeout(inter_byte_timeout_),
              read_timeout_constant(read_timeout_constant_),
              write_timeout_constant(write_timeout_constant_)
        {
        }
    };

    class MillisecondTimer
    {
    public:
        MillisecondTimer(const uint32_t millis);
        int64_t remaining(); // оставшееся время до завершения таймера

    private:
        static timespec timespec_now();
        timespec expiry; // время завершения таймера
    };

    /// @brief ------------------------
    class SerialImpl
    {
    public:
        SerialImpl(const string &port,
                   unsigned long baudrate);

        ~SerialImpl();

        void open();
        void close();
        bool isOpen() const;
        size_t available();
        bool waitReadable(uint32_t timeout = 0);
        void waitByteTimes(size_t count);
        size_t read(uint8_t *buf, size_t size = 1);
        size_t write(const uint8_t *data, size_t length);

        void setPort(const string &port);
        void setTimeout(Timeout &timeout);
        void setBaudrate(unsigned long baudrate);
        bool set_dtr(bool level);
        bool set_rts(bool level);
        bool wait_for_change();
        int get_cts();
        int get_dsr();

    protected:
        void reconfigurePort();

    private:
        string port_; // Path to the file descriptor
        int fd_;      // The current file descriptor

        bool is_open_;

        Timeout timeout_ = Timeout(); // Timeout for read operations
        speed_t baudrate_;            // Baudrate
        uint32_t byte_time_ns_;       // Длительность в наносекундах приема/передачи одного байта
        uint8_t parity_ = 0;
        uint8_t bytesize_ = 8;
        uint8_t stopbits_ = 1;
    };

}

#endif
