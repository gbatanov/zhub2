
#ifndef UART_H
#define UART_H

#define SOF 0xFE

#define RX_BUFFER_SIZE 512
#define TX_BUFFER_SIZE 512

class Zdo;

class Uart
{
public:
  Uart();
  ~Uart();

  bool connect(std::string port, unsigned int baud_rate = 115200);
  void disconnect();
  void OnCommand(Command command);
  void OnDisconnect();
  bool sendCommandToDevice(Command command);
  bool send_command_to_device(Command command);
  bool start();

private:
  std::unique_ptr<serial::Serial> serial_= std::make_unique<serial::Serial>();

  std::vector<Command> parseReceivedData(std::vector<uint8_t> &data);
  void loop();
  void snd_loop();

  std::thread receiver_thread_;
  std::thread send_thread_;
  std::vector<uint8_t> tx_buff_;
  std::vector<uint8_t> rx_buff_;
  std::mutex transmit_mutex_, sndMutex_;
  std::queue<Command> sndQueue_{};
};

#endif
