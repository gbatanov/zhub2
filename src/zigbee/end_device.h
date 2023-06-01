#ifndef end_device_h
#define end_device_h

struct DeviceInfo
{
    uint8_t deviceType;
    std::string manufacturer;
    std::string productCode;
    std::string engName;
    std::string humanName;
    zigbee::zcl::Cluster cluster;
    zigbee::zcl::Attributes::PowerSource powerSource;
    uint8_t available; // включать в рабочую конфигурацию
    uint8_t test;      // включать в тестовую конфигурацию
};
struct EndpointInfo
{
    std::vector<uint8_t> endpoits; // массив всех эндпойнтов в типе
    std::map<uint8_t, std::vector<uint8_t>> inputClusters;
    std::map<uint8_t, std::vector<uint8_t>> outputClusters;
};

class EndDevice : public virtual zigbee::Device
{
public:
    // Описания моих устройств
    static const std::map<const uint64_t, const DeviceInfo> KNOWN_DEVICES;
    static const std::map<const uint8_t, const std::string> DEVICE_TYPES;
    static const std::vector<uint64_t> OFF_LIST;
    static const std::vector<uint64_t> PROM_MOTION_LIST;
    static const std::vector<uint64_t> PROM_RELAY_LIST;
    static const std::vector<uint64_t> PROM_DOOR_LIST;

    EndDevice(zigbee::NetworkAddress shortAddr, zigbee::IEEEAddress IEEEAddress);
    ~EndDevice();

    void setProductCode(std::string productCode) { deviceInfo.productCode = productCode; };
    std::string getProductCode() { return deviceInfo.productCode; };
    void setManufacturer(std::string manufacturer) { deviceInfo.manufacturer = manufacturer; };
    std::string getManufacturer() { return deviceInfo.manufacturer; };
    void setModelIdentifier(std::string model) { modelIdentifier_ = model; };
    std::string getModelIdentifier() { return modelIdentifier_; };
    zigbee::NetworkAddress getNetworkAddress() { return shortAddr_; };
    zigbee::IEEEAddress getIEEEAddress() { return IEEEAddress_; };
    void setNetworkAddress(zigbee::NetworkAddress shortAddr) { shortAddr_ = shortAddr; };
    void setIEEEAddress(zigbee::IEEEAddress IEEEAddress) { IEEEAddress_ = IEEEAddress; };
    bool isBatteryPowered() { return deviceInfo.powerSource == zigbee::zcl::Attributes::PowerSource::BATTERY; };
    bool isACPowered() { return deviceInfo.powerSource == zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE; };
    void setPowerSource(uint8_t powerSource);
    std::string showPowerSource();
    void setBatteryParams(uint8_t battery_remain_percent, double battery_voltage);
    std::string showBatteryRemain();
    std::string showBatteryVoltage();
    virtual void actionHandle(){};
    void init();
    std::string get_current_state(uint8_t channel=1);
    void set_current_state(std::string state, uint8_t channel=1);
    std::string get_human_name() { return deviceInfo.humanName.empty() ? "Unknown" : deviceInfo.humanName; }
    void set_temperature(double temperature) { temperature_ = temperature; }
    double get_temperature() { return temperature_; }
    void set_humidity(double humidity) { humidity_ = humidity; }
    double get_humidity() { return humidity_; }
    void set_pressure(double pressure) { pressure_ = pressure * 0.00750063755419211; }
    double get_pressure() { return pressure_; }
    void set_linkquality(uint8_t lq) { linkquality_ = lq; }
    uint8_t get_linkquality() { return linkquality_; }
    void set_mains_voltage(double voltage) { mains_voltage_ = voltage; }
    double get_mains_voltage() { return mains_voltage_; }
    std::string show_mains_voltage();
    void set_current(double c)
    {
        if (c > -0.5)
            current_ = c;
    }
    double get_current() { return current_; }
    std::string show_current();
    uint64_t set_last_seen(uint64_t timestamp);
    uint64_t get_last_seen() { return last_seen_; }
    uint64_t set_last_action(uint64_t timestamp);
    uint64_t get_last_action() { return last_action_; }
    std::string show_last(int what = 0);

    void set_luminocity(uint8_t lum)
    {
        if (lum == 0 || lum == 1)
            luminocity_ = (int8_t)lum;
    }
    int8_t get_luminocity() { return luminocity_; }
    void set_motion_state(uint8_t state)
    {
        if (state == 0 || state == 1)
            motion_ = (int8_t)state;
    }
    int8_t get_motion_state() { return motion_; }
    std::vector<uint8_t> get_endpoints() { return ep_; }

    uint8_t get_device_type() { return deviceInfo.deviceType; }

    std::string get_prom_motion_string();
    std::string get_prom_relay_string();
    std::string get_prom_door_string();
    std::string get_prom_pressure();

    DeviceInfo deviceInfo;

protected:
    zigbee::NetworkAddress shortAddr_{}; // сетевой (короткий) адрес может меняться
    zigbee::IEEEAddress IEEEAddress_{}; // постоянная величина для одного устройства
    std::string modelIdentifier_{};
    zigbee::zcl::Cluster cluster_{zigbee::zcl::Cluster::UNKNOWN_CLUSTER};

    uint8_t linkquality_ = 0; // из сообщения
    uint8_t lqi_ = 0; // из параметров в ответах
    int16_t rssi_ = 0; // из параметров в ответах
    uint8_t battery_remain_percent_ = 0;
    double battery_voltage_ = 0.0;
    std::string state_ = "Unknown"; // текстовое описание состояния, зависящее от типа датчика
    std::string state2_ = "Unknown"; // текстовое описание состояния канала 2, зависящее от типа датчика
    double temperature_{-100.0};    // -100.0 - признак отсутствия датчика температуры у данного устройства
    double humidity_{-100.0};       // -100.0 - признак отсутствия датчика влажности у данного устройства
    double mains_voltage_ = 0.0;    // напряжение питания сети
    double current_ = -1.0;         // потребляемый ток, меньше нуля - данных нет
    double pressure_{-100.0};       // -100.0 - признак отсутствия датчика давления у данного устройства
    int8_t luminocity_ = -1;        // -1 - нет инфы, 0 - темно, 1 - светло
    int8_t motion_ = -1;            // -1 - нет инфы, 0 - нет движения, 1 - есть движение

private:

    std::atomic<bool> flag;
     uint64_t last_seen_ = 0;                                   // время последнего ответа устройства
    uint64_t last_action_ = 0;                                 // время последней активности устройства (может не совпадать со временем последнего ответа)
    // только для TEST конфигурации
    std::vector<uint8_t> ep_{};                                // endpoints TODO: прописывать в каждом типе устройств как константу
    std::map<uint8_t, std::vector<uint8_t>> inputClusters_{};  // input clusters for endpoints TODO: прописывать в каждом типе устройств как константу
    std::map<uint8_t, std::vector<uint8_t>> outputClusters_{}; // output clusters for endpoints TODO: прописывать в каждом типе устройств как константу
};

#endif /* end_device_h */
