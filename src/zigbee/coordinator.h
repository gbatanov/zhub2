#ifndef COORDINATOR_H
#define COORDINATOR_H

class Zdo;

class Coordinator : public Controller
{
public:
    Coordinator();
    ~Coordinator();

    void start(zigbee::NetworkConfiguration configuration = Controller::default_configuration_);
    void onJoin(zigbee::NetworkAddress network_address, zigbee::IEEEAddress mac_address);
    void onLeave(zigbee::NetworkAddress network_address, zigbee::IEEEAddress mac_address);
    void getPower(zigbee::NetworkAddress address, Cluster cluster = Cluster::BASIC);
    std::string showDeviceInfo(zigbee::NetworkAddress network_address);

    bool setDevicesMapToFile();
    bool getDevicesMapFromFile(bool with_reg = true);
    std::string get_device_list(bool as_html = false);
    std::map<uint16_t, uint64_t> readMapFromFile();
    std::string show_device_statuses(bool as_html = false);
    void onoff_command(zigbee::Message message);
    void alarms_command(zigbee::Message message);
    void level_command(zigbee::Message message);
    void ias_zone_command(uint8_t cmnd = 0x00, uint16_t one = 0);
    void ias_zone_command(uint8_t cmnd, uint64_t mac_addr);
    void height_temperature(uint16_t short_addr);

    bool get_ready() { return isReady_; };
    void set_ready() { isReady_ = true; }
    void handle_motion(std::shared_ptr<zigbee::EndDevice>ed, uint8_t cmd);
    void handle_sonoff_door(std::shared_ptr<zigbee::EndDevice>ed, uint8_t cmd);
     void ikea_button_action(uint8_t cmd);
    std::string get_leak_state();
    std::string get_motion_state();
    void setLastMotionSensorActivity(std::time_t lastTime)
    {
        if (lastTime > lastMotionSensorActivity)
            lastMotionSensorActivity = lastTime;
    }
    std::time_t getLastMotionSensorActivity() { return lastMotionSensorActivity; }
    void check_motion_activity();
    std::string show_one_type(std::shared_ptr<zigbee::EndDevice>d, bool as_html);

    void getIdentifier(zigbee::NetworkAddress address);
    bool configureReporting(zigbee::NetworkAddress address);
    bool configureReporting(zigbee::NetworkAddress address,
                            Cluster cluster,
                            uint16_t attributeId = 0x0000,
                            zigbee::zcl::Attribute::DataType attribute_data_type = zigbee::zcl::Attribute::DataType::UINT8,
                            uint16_t reportable_change = 0x0000);

    void on_attribute_report(zigbee::Endpoint endpoint, zigbee::zcl::Cluster cluster, std::vector<zigbee::zcl::Attribute> attributes);
    inline void switch_off_with_list();
 
private:
    void joinDevice(zigbee::NetworkAddress network_address, zigbee::IEEEAddress mac_address, bool norec=true);
    zigbee::EventCommand event_command_;
    bool isReady_ = false;

    std::mutex mtxJoin;
};

#endif
