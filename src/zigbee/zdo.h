#ifndef ZDO_H
#define ZDO_H

using namespace std::chrono_literals;

#define DEFAULT_RESPONSE_TIMEOUT 3s
#define SYNC_RESPONSE_TIMEOUT 3s
#define ASYNC_RESPONSE_TIMEOUT 3s
// RESET_TIMEOUT делаю большим, так как после подачи питания первый сброс идет очень долго
#define RESET_TIMEOUT 60s
#define DEFAULT_RADIUS 7
#define BIND_RESPONSE_TIMEOUT 5s

class Uart;

enum class Status
{
    SUCCESS = 0x00,
    FAILURE = 0x01,
    INVALID_PARAMETR = 0x02,
    NV_ITEM_UNINIT = 0x09,
    NV_OPER_FAILED = 0x0a,
    NV_BAD_ITEM_LENGTH = 0x0c,
    MEMORY_ERROR = 0x10,
    BUFFER_FULL = 0x11,
    UNSUPPORTED_MODE = 0x12,
    MAC_MEMMORY_ERROR = 0x13,
    ZDO_INVALID_REQUEST_TYPE = 0x80,
    ZDO_INVALID_ENDPOINT = 0x82,
    ZDO_UNSUPPORTED = 0x84,
    ZDO_TIMEOUT = 0x85,
    ZDO_NO_MUTCH = 0x86,
    ZDO_TABLE_FULL = 0x87,
    ZDO_NO_BIND_ENTRY = 0x88,
    SEC_NO_KEY = 0xa1,
    SEC_MAX_FRM_COUNT = 0xa3,
    APS_FAIL = 0xb1,
    APS_TABLE_FULL = 0xb2,
    APS_ILLEGAL_REQEST = 0xb3,
    APS_INVALID_BINDING = 0xb4,
    APS_UNSUPPORTED_ATTRIBUTE = 0xb5,
    APS_NOT_SUPPORTED = 0xb6,
    APS_NO_ACK = 0xb7,
    APS_DUPLICATE_ENTRY = 0xb8,
    APS_NO_BOUND_DEVICE = 0xb9,
    NWK_INVALID_PARAM = 0xc1,
    NWK_INVALID_REQUEST = 0xc2,
    NWK_NOT_PERMITTED = 0xc3,
    NWK_STARTUP_FAILURE = 0xc4,
    NWK_TABLE_FULL = 0xc7,
    NWK_UNKNOWN_DEVICE = 0xc8,
    NWK_UNSUPPORTED_ATTRIBUTE = 0xc9,
    NWK_NO_NETWORK = 0xca,
    NWK_LEAVE_UNCONFIRMED = 0xcb,
    NWK_NO_ACK = 0xcc,
    NWK_NO_ROUTE = 0xcd,
    MAC_NO_ACK = 0xe9,
    MAC_TRANSACTION_EXPIRED = 0xf0
};
enum class ZdoState
{
    NOT_STARTED = 0,          // Initialized - not started automatically
    NOT_CONNECTED = 1,        // Initialized - not connected to anything
    DISCOVERING = 2,          // Discovering PAN's to join
    JOINING = 3,              // Joining a PAN
    END_DEVICE_REJOINING = 4, // Rejoining a PAN, only for end devices
    END_DEVICE_UNAUTH = 5,    // Joined but not yet authenticated by trust center
    END_DEVICE_ = 6,          // Started as device after authentication
    ROUTER = 7,               // Device joined, authenticated and is a router
    COORDINATOR_STARTING = 8, // Starting as ZigBee Zhub
    COORDINATOR = 9,          // Started as ZigBee Zhub
    LOST_PARENT = 10          // Device has lost information about its parent
};

enum class NvItems
{
    STARTUP_OPTION = 0x0003,     // 1 : default - 0, bit 0 - STARTOPT_CLEAR_CONFIG, bit 1 - STARTOPT_CLEAR_STATE (need reset).
    PAN_ID = 0x0083,             // 2 : 0xFFFF to indicate dont care (auto).
    EXTENDED_PAN_ID = 0x002D,    // 8: (0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD) - from zigbee2mqtt
    CHANNEL_LIST = 0x0084,       // 4 : channel bit mask. Little endian. Default is 0x00000800 for CH11;  Ex: value: [ 0x00, 0x00, 0x00, 0x04 ] for CH26, [ 0x00, 0x00, 0x20, 0x00 ] for CH15.
    LOGICAL_TYPE = 0x0087,       // 1 : COORDINATOR: 0, ROUTER : 1, END_DEVICE : 2
    PRECFG_KEY = 0x0062,         // 16 : (0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0D) - from zigbee2mqtt
    PRECFG_KEYS_ENABLE = 0x0063, // 1 : defalt - 1
    ZDO_DIRECT_CB = 0x008F,      // 1 : defaul - 0, need for callbacks - 1
    ZNP_HAS_CONFIGURED = 0x0F00, // 1 : 0x55
    MY_DEVICES_MAP = 0x0FF3      // 150 : для хранения своих данных ( не более 150 байт)
};

enum class ResetType
{
    HARD = 0,
    SOFT = 1
};

class Zdo : public virtual Device, public ThreadPool
{
public:
    Zdo();
    ~Zdo();
void init();
    int reset(ResetType resetType, bool clearNetworkState, bool clearConfig);
    int reset() { return reset(ResetType::SOFT, false, false); }
    bool startup(std::chrono::duration<int, std::milli> delay);
    bool startup() { return startup(100ms); }
    bool connect(std::string port, unsigned int baud_rate);
    void disconnect();
    void on_disconnect();
    bool registerEndpoint(zigbee::SimpleDescriptor endpointDescriptor);
    bool permitJoin(const std::chrono::duration<int, std::ratio<1>> duration);
    void activeEndpoints(zigbee::NetworkAddress address);
    void simpleDescriptor(zigbee::NetworkAddress address, unsigned int endpointNumber);
    std::optional<int> setTransmitPower(int requestedPower); // Range: -22 ... 3 dBm.
    void sendMessage(zigbee::Endpoint endpoint, zigbee::zcl::Cluster cluster, zigbee::zcl::Frame frame, std::chrono::duration<int, std::milli> timeout = 3s);

    zigbee::NetworkAddress getNetworkAddress() { return network_address_; }
    zigbee::IEEEAddress getIEEEAddress() { return mac_address_; }

    std::vector<uint8_t> read_rf_channels();
    bool write_rf_channels(std::vector<uint8_t> rf);
    bool finish_configuration();
    void handle_command(Command command);
    bool ping();
    bool bind(zigbee::NetworkAddress network_address, zigbee::IEEEAddress src_mac_address, uint8_t src_endpoint, zigbee::zcl::Cluster cluster);

    std::optional<Command> syncRequest(Command request, std::chrono::duration<int, std::milli> timeout = SYNC_RESPONSE_TIMEOUT);
    bool asyncRequest(Command &request, std::chrono::duration<int, std::milli> timeout = ASYNC_RESPONSE_TIMEOUT);

    std::optional<std::vector<uint8_t>> readNv(NvItems item);
    bool writeNv(NvItems item, std::vector<uint8_t> value);
    bool initNv(NvItems item, uint16_t length, std::vector<uint8_t> value);
    bool deleteNvItem(NvItems item);
    inline uint8_t generateTransactionNumber() // TODO
    {
        static uint8_t transactionNumber = 0;
        return transactionNumber++;
    }
    zigbee::zcl::Frame parseZclData(std::vector<uint8_t> data);

    ZdoState current_state_;
    std::array<unsigned, 3> version_{0, 0, 0};
    zigbee::NetworkAddress network_address_ = 0;
    zigbee::IEEEAddress mac_address_ = 0;
    zigbee::EventCommand event_command_;
   void on_command();
    void get_attribute_RSSI_Power(zigbee::NetworkAddress address);
    std::string getCommandStr(Command &command);
    std::string get_cluster_string(zcl::Cluster cl);
    std::shared_ptr<gsbutils::Channel<Command>> chan_out,chan_in;
    std::shared_ptr<Uart> uart_;

protected:
    uint8_t generateTransactionSequenceNumber();
    std::thread *thr_cmdin;
};

#endif
