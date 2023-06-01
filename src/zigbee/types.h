#ifndef TYPES_H
#define TYPES_H

typedef uint16_t NetworkAddress;
typedef uint64_t IEEEAddress;

struct SimpleDescriptor {
	unsigned int endpoint_number = 0;
	unsigned int profile_id = 0;
	unsigned int device_id = 0;
	unsigned int device_version = 0;
	std::vector<unsigned int> input_clusters;
	std::vector<unsigned int> output_clusters;
};

struct Endpoint {
	NetworkAddress address;
	uint8_t number; //endpoint

	bool operator==(const Endpoint &endpoint) const { return (this->address == endpoint.address && this->number == endpoint.number); }
	bool operator!=(const Endpoint &endpoint) const { return (!(*this == endpoint)); }
};

struct Message {
	Endpoint source;
	Endpoint destination;
	zigbee::zcl::Cluster cluster;
	zigbee::zcl::Frame zcl_frame;
	uint8_t linkquality;
};

enum class LogicalType {
	COORDINATOR = 0,
	ROUTER = 1,
	END_DEVICE = 2
}; 

struct NetworkConfiguration {
	uint16_t pan_id = 0;
	uint64_t extended_pan_id = 0;
	LogicalType logical_type = LogicalType::COORDINATOR;
	std::vector<unsigned int> channels;
	uint8_t precfg_key[16]{};
	bool precfg_key_enable = false; // value: 0 (FALSE) only coord defualtKey need to be set, and OTA to set other devices in the network.
									// value: 1 (TRUE) Not only coord, but also all devices need to set their defualtKey (the same key). Or they can't not join the network.

	bool operator==(const NetworkConfiguration &configuration) const {
		return (this->pan_id == configuration.pan_id && 
				this->extended_pan_id == configuration.extended_pan_id && 
				this->logical_type == configuration.logical_type && 
				this->channels == configuration.channels
				&& precfg_key_enable ? std::equal(std::begin(this->precfg_key), std::end(this->precfg_key), std::begin(configuration.precfg_key), std::end(configuration.precfg_key)):true
                );
	}

	bool operator!=(const NetworkConfiguration &configuration) const { return (!(*this == configuration)); }
};


// общий предок всех zigbee-устройств. Каждое устройство должно иметь как минимум shortAddr(NetworkAddress) и macAddr
// Реализую как интерфейс
class Device {
public:

	virtual zigbee::NetworkAddress getNetworkAddress() = 0;
	virtual zigbee::IEEEAddress getIEEEAddress() = 0;
	Device(){};
	virtual ~Device(){};
};

#endif

