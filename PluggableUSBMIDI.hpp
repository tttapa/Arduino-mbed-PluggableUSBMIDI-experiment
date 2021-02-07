#include <Arduino.h>

#include <atomic>
#include <cstdint>
#include <type_traits>

#include <USB/PluggableUSBDevice.h>
#include <mbed/drivers/Timeout.h>
#include <mbed/platform/Callback.h>

class PluggableUSBMIDI : protected arduino::internal::PluggableUSBModule {
  public:
    using setup_packet_t = USBDevice::setup_packet_t;
    using DeviceState = USBDevice::DeviceState;

    PluggableUSBMIDI();
    ~PluggableUSBMIDI();

    void read_and_verify();

  protected:
    void init(EndpointResolver &resolver) override;
    void callback_state_change(DeviceState new_state) override;
    uint32_t callback_request(const setup_packet_t *setup,
                              USBDevice::RequestResult *result,
                              uint8_t **data) override;
    bool callback_request_xfer_done(const setup_packet_t *setup,
                                    bool aborted) override;
    bool callback_set_configuration(uint8_t configuration) override;
    void callback_set_interface(uint16_t interface, uint8_t alternate) override;

    const uint8_t *string_iinterface_desc() override;
    const uint8_t *configuration_desc(uint8_t index) override;

  public:
    // TODO: why does increasing the packet size beyond 64 not work?
    static constexpr uint32_t PacketSize = 64; // Must be a power of two

  protected:
    static constexpr uint32_t NumRxPackets = 3;

    struct {
        struct Buffer {
            uint32_t size = 0;
            uint32_t index = 0;
            alignas(uint32_t) uint8_t buffer[PacketSize];
        } buffers[NumRxPackets];
        std::atomic<uint32_t> available{0};
        std::atomic<uint32_t> read_idx{0};
        std::atomic<uint32_t> write_idx{0};
        std::atomic<bool> reading{false};
    } reading;
    using rbuffer_t = std::remove_reference_t<decltype(reading.buffers[0])>;

    uint32_t verify_cnt = 0;
    uint32_t verify_cnt_total = 0;

    usb_ep_t bulk_in_ep;
    usb_ep_t bulk_out_ep;
    uint8_t config_descriptor[0x65];

    void in_callback();
    void out_callback();
    void verify_data(const uint8_t *data, uint32_t size);
};
