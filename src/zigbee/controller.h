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
    bool startNetwork(zigbee::NetworkConfiguration configuration);
    void finish() { disconnect(); }
    void on_message(Command command);
    void handle_power_off(int value);
    void handle_board_temperature(float temp);
    float get_board_temperature();
    void fan(bool work);
    void ringer();
    std::string show_sim800_battery();
    void read_attribute(zigbee::NetworkAddress address, zigbee::zcl::Cluster cl, std::vector<uint16_t> ids);

    bool init_adapter();
    std::shared_ptr<zigbee::EndDevice> getDeviceByShortAddr(zigbee::NetworkAddress network_address);
    std::shared_ptr<zigbee::EndDevice> get_device_by_mac(zigbee::IEEEAddress di);
    virtual void on_attribute_report(zigbee::Endpoint endpoint, zigbee::zcl::Cluster cluster, std::vector<zigbee::zcl::Attribute> attributes){};
    virtual void onoff_command(zigbee::Message message){};
    virtual void level_command(zigbee::Message message){};
    virtual void handle_motion(std::shared_ptr<zigbee::EndDevice> ed, uint8_t cmd){};
    virtual void getPower(zigbee::NetworkAddress address, Cluster cluster = Cluster::BASIC){};
    virtual void handle_sonoff_door(std::shared_ptr<zigbee::EndDevice> ed, uint8_t cmd){};
    virtual void ias_zone_command(uint8_t cmnd = 0x00, uint16_t one = 0){};
    virtual void ias_zone_command(uint8_t cmnd, uint64_t mac_addr){};
    virtual zigbee::NetworkAddress getShortAddrByMacAddr(zigbee::IEEEAddress mac_address);
    virtual void after_message_action();
    virtual void switch_off_with_timeout();
    virtual void switch_relay(uint64_t mac_addr, uint8_t cmd, uint8_t ep = 1);
    virtual void sendCommandToOnOffDevice(zigbee::NetworkAddress address, uint8_t cmd, uint8_t ep = 1);
    static const zigbee::NetworkConfiguration test_configuration_;
    
protected:
    std::map<zigbee::IEEEAddress, std::shared_ptr<zigbee::EndDevice>> devices_{};     // пары IEEEAddress<=>EndDevice
    std::map<zigbee::NetworkAddress, zigbee::IEEEAddress> end_devices_address_map_{}; // пары shortAddr<=>IEEEAddress
    std::time_t smartPlugTime = 0;
    std::time_t lastMotionSensorActivity = 0;
    static const std::vector<zigbee::SimpleDescriptor> default_endpoints_;
    static const zigbee::NetworkConfiguration default_configuration_;

};

#endif
