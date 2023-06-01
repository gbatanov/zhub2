#ifndef CONTROLLER_H
#define CONTROLLER_H

using zigbee::zcl::Cluster;
using zigbee::zcl::Frame;

class Controller : public Zdo
{
public:
    Controller();
    ~Controller();

    void OnLog(std::string logMsg);
    void OnError(std::string errMsg);
    bool start_network(std::vector<uint8_t> rfChannels);
    void finish() { disconnect(); }
    void on_message(Command command);
    void handle_power_off(int value);
    void handle_board_temperature(float temp);
    float get_board_temperature();
    void fan(bool work);
    void ringer();
    std::string show_sim800_battery();
    void read_attribute(zigbee::NetworkAddress address, zigbee::zcl::Cluster cl, std::vector<uint16_t> ids);
    std::map<uint16_t, uint64_t> readMapFromFile();
    void joinDevice(zigbee::NetworkAddress network_address, zigbee::IEEEAddress mac_address, bool norec = true);

    bool init_adapter();
    std::shared_ptr<zigbee::EndDevice> getDeviceByShortAddr(zigbee::NetworkAddress network_address);
    std::shared_ptr<zigbee::EndDevice> get_device_by_mac(zigbee::IEEEAddress di);
    virtual bool setDevicesMapToFile();
    virtual bool getDevicesMapFromFile(bool with_reg = true);
    virtual void onoff_command(zigbee::Message message){};
    virtual void level_command(zigbee::Message message){};
    virtual void handle_motion(std::shared_ptr<zigbee::EndDevice> ed, uint8_t cmd){};
    virtual void handle_sonoff_door(std::shared_ptr<zigbee::EndDevice> ed, uint8_t cmd){};
    virtual void ias_zone_command(uint8_t cmnd = 0x00, uint16_t one = 0){};
    virtual void ias_zone_command(uint8_t cmnd, uint64_t mac_addr){};
    zigbee::NetworkAddress getShortAddrByMacAddr(zigbee::IEEEAddress mac_address);
    void after_message_action();
    void switch_off_with_timeout();
    void switch_relay(uint64_t mac_addr, uint8_t cmd, uint8_t ep = 1);
    void sendCommandToOnOffDevice(zigbee::NetworkAddress address, uint8_t cmd, uint8_t ep = 1);
    void getIdentifier(zigbee::NetworkAddress address);
    virtual void on_attribute_report(zigbee::Endpoint endpoint, zigbee::zcl::Cluster cluster, std::vector<zigbee::zcl::Attribute> attributes);
    virtual bool configureReporting(zigbee::NetworkAddress address);
    virtual bool configureReporting(zigbee::NetworkAddress address,
                                    Cluster cluster,
                                    uint16_t attributeId = 0x0000,
                                    zigbee::zcl::Attribute::DataType attribute_data_type = zigbee::zcl::Attribute::DataType::UINT8,
                                    uint16_t reportable_change = 0x0000);
    bool get_ready() { return isReady_; };
    void set_ready() { isReady_ = true; }
    virtual void getPower(zigbee::NetworkAddress address, Cluster cluster = Cluster::BASIC);
    void onJoin(zigbee::NetworkAddress network_address, zigbee::IEEEAddress mac_address);
    void onLeave(zigbee::NetworkAddress network_address, zigbee::IEEEAddress mac_address);
    std::string showDeviceInfo(zigbee::NetworkAddress network_address);
    //   static const zigbee::NetworkConfiguration default_configuration_;
    //    static const zigbee::NetworkConfiguration test_configuration_;
    static const std::vector<uint8_t> DEFAULT_RF_CHANNELS;
    static const std::vector<uint8_t> TEST_RF_CHANNELS;

protected:
    std::map<zigbee::IEEEAddress, std::shared_ptr<zigbee::EndDevice>> devices_{};     // пары IEEEAddress<=>EndDevice
    std::map<zigbee::NetworkAddress, zigbee::IEEEAddress> end_devices_address_map_{}; // пары shortAddr<=>IEEEAddress
    std::time_t smartPlugTime = 0;
    std::time_t lastMotionSensorActivity = 0;
    static const std::vector<zigbee::SimpleDescriptor> default_endpoints_;
    std::mutex mtxJoin;
    bool isReady_ = false;
};

#endif
