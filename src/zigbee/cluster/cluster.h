#ifndef GSB_CLUSTER_H
#define GSB_CLUSTER_H

namespace clusters
{
    class Basic
    {
    public:
        Basic(std::shared_ptr<zigbee::EndDevice> dev)
        {
            ed = dev;
        };
        void attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint ep);

    protected:
        std::shared_ptr<zigbee::EndDevice> ed;
    };

    class PowerConfiguration
    {
    public:
        PowerConfiguration(std::shared_ptr<zigbee::EndDevice> dev)
        {
            ed = dev;
        };
        void attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint ep);

    protected:
        std::shared_ptr<zigbee::EndDevice> ed;
    };

    class OnOff
    {
    public:
        OnOff(std::shared_ptr<zigbee::EndDevice> dev)
        {
            ed = dev;
        };
        void attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint ep);

    protected:
        std::shared_ptr<zigbee::EndDevice> ed;
    };

    class DeviceTemperatureConfiguration
    {
    public:
        DeviceTemperatureConfiguration(std::shared_ptr<zigbee::EndDevice> dev)
        {
            ed = dev;
        };
        void attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint ep);

    protected:
        std::shared_ptr<zigbee::EndDevice> ed;
    };

    class AnalogInput
    {
    public:
        AnalogInput(std::shared_ptr<zigbee::EndDevice> dev)
        {
            ed = dev;
        };
        void attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint ep);

    protected:
        std::shared_ptr<zigbee::EndDevice> ed;
    };

    class MultistateInput
    {
    public:
        MultistateInput(std::shared_ptr<zigbee::EndDevice> dev)
        {
            ed = dev;
        };
        void attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint ep);

    protected:
        std::shared_ptr<zigbee::EndDevice> ed;
    };

    class Xiaomi
    {
    public:
        Xiaomi(std::shared_ptr<zigbee::EndDevice> dev)
        {
            ed = dev;
        };
        void attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint ep);

    protected:
        std::shared_ptr<zigbee::EndDevice> ed;
    };

    class SimpleMetering
    {
    public:
        SimpleMetering(std::shared_ptr<zigbee::EndDevice> dev)
        {
            ed = dev;
        };
        void attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint ep);

    protected:
        std::shared_ptr<zigbee::EndDevice> ed;
    };

    class ElectricalMeasurements
    {
    public:
        ElectricalMeasurements(std::shared_ptr<zigbee::EndDevice> dev)
        {
            ed = dev;
        };
        void attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint ep);

    protected:
        std::shared_ptr<zigbee::EndDevice> ed;
    };

    class Tuya
    {
    public:
        Tuya(std::shared_ptr<zigbee::EndDevice> dev)
        {
            ed = dev;
        };
        void attribute_handler_private(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint ep);
        void attribute_handler_switch_mode(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint);

            protected : std::shared_ptr<zigbee::EndDevice> ed;
    };

    class Other
    {
    public:
        Other(std::shared_ptr<zigbee::EndDevice> dev, zigbee::zcl::Cluster cl)
        {
            ed = dev;
            cluster = cl;
        };
        void attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint ep);

    protected:
        std::shared_ptr<zigbee::EndDevice> ed;
        zigbee::zcl::Cluster cluster;
    };


}
#endif