#ifndef COORDINATOR_H
#define COORDINATOR_H

class Zdo;
class Controller;

class Zhub : public Controller
{
public:
    Zhub();
    ~Zhub();

    void start(std::vector<uint8_t> rfChannels);
    std::string show_device_statuses(bool as_html = false);
    std::string get_device_list(bool as_html = false);
    void onoff_command(zigbee::Message message);
    void alarms_command(zigbee::Message message);
    void level_command(zigbee::Message message);
    void ias_zone_command(uint8_t cmnd = 0x00, uint16_t one = 0);
    void ias_zone_command(uint8_t cmnd, uint64_t mac_addr);
    void height_temperature(uint16_t short_addr);
    void handle_motion(std::shared_ptr<zigbee::EndDevice> ed, uint8_t cmd);
    void handle_sonoff_door(std::shared_ptr<zigbee::EndDevice> ed, uint8_t cmd);
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
    std::string show_one_type(std::shared_ptr<zigbee::EndDevice> d, bool as_html);
    inline void switch_off_with_list();
    virtual void send_tlg_message(std::string msg);

private:
bool edcheck(std::shared_ptr<zigbee::EndDevice> ed);
    zigbee::EventCommand eventCommand_;
};

#endif
