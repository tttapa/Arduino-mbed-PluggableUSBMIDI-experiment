#include "PluggableUSBMIDI.hpp"

constexpr static auto mo_rel = std::memory_order_release;
constexpr static auto mo_acq = std::memory_order_acquire;
constexpr static auto mo_rlx = std::memory_order_relaxed;
constexpr static auto mo_acq_rel = std::memory_order_acq_rel;

auto &ser = Serial1;

// ------------------------- CONSTRUCTOR/DESTRUCTOR ------------------------- //

PluggableUSBMIDI::PluggableUSBMIDI() : PluggableUSBModule(2) {
    PluggableUSBD().plug(this);
}

PluggableUSBMIDI::~PluggableUSBMIDI() { PluggableUSBD().deinit(); }

// ----------------------------- INITIALIZATION ----------------------------- //

void PluggableUSBMIDI::init(EndpointResolver &resolver) {
    bulk_in_ep = resolver.endpoint_in(USB_EP_TYPE_BULK, PacketSize);
    bulk_out_ep = resolver.endpoint_out(USB_EP_TYPE_BULK, PacketSize);
    MBED_ASSERT(resolver.valid());
}

// --------------------------------- USB API -------------------------------- //

void PluggableUSBMIDI::callback_state_change(DeviceState new_state) {
    assert_locked();
}

uint32_t PluggableUSBMIDI::callback_request(const setup_packet_t *setup,
                                            USBDevice::RequestResult *result,
                                            uint8_t **data) {
    assert_locked();
    *result = USBDevice::PassThrough;
    *data = nullptr;
    return 0;
}

bool PluggableUSBMIDI::callback_request_xfer_done(const setup_packet_t *setup,
                                                  bool aborted) {
    assert_locked();
    return false;
}

bool PluggableUSBMIDI::callback_set_configuration(uint8_t configuration) {
    assert_locked();

    PluggableUSBD().endpoint_add(
        bulk_in_ep, PacketSize, USB_EP_TYPE_BULK,
        mbed::callback(this, &PluggableUSBMIDI::in_callback));
    PluggableUSBD().endpoint_add(
        bulk_out_ep, PacketSize, USB_EP_TYPE_BULK,
        mbed::callback(this, &PluggableUSBMIDI::out_callback));

    reading.available.store(0, mo_rlx);
    reading.read_idx.store(0, mo_rlx);
    reading.write_idx.store(0, mo_rlx);
    reading.reading.store(true, mo_rlx);
    read_start(bulk_out_ep, reading.buffers[0].buffer, PacketSize);

    return true;
}

void PluggableUSBMIDI::callback_set_interface(uint16_t interface,
                                              uint8_t alternate) {
    assert_locked();
}

// ---------------------------------- WRITING ----------------------------------

void PluggableUSBMIDI::in_callback() {
    assert_locked();
    MBED_ASSERT(!"Not implemented");
}

// ---------------------------------- READING ----------------------------------

static void hexdump(const uint8_t *data, uint32_t count) {
    uint32_t i = 0;
    for (; i < count; i += 4) {
        char buf[] = "xx xx xx xx  ";
        std::snprintf(buf, sizeof(buf), "%02X %02X %02X %02X  ",
                      (unsigned int)data[i + 0], (unsigned int)data[i + 1],
                      (unsigned int)data[i + 2], (unsigned int)data[i + 3]);
        ser.print(buf);
    }
    ser.println();
}

void PluggableUSBMIDI::verify_data(const uint8_t *data, uint32_t size) {

    hexdump(data, size);

    const uint8_t *const end = data + size;
    for (const uint8_t *p = data; p < end; p += 4) {
        if (verify_cnt_total == 0) {
            // First message
            MBED_ASSERT(p[0] == 0x04);
            MBED_ASSERT(p[1] == 0xF0);
            MBED_ASSERT(p[2] == verify_cnt);
            verify_cnt = (verify_cnt + 1) == 127 ? 0 : verify_cnt + 1;
            MBED_ASSERT(p[3] == verify_cnt);
            verify_cnt = (verify_cnt + 1) == 127 ? 0 : verify_cnt + 1;
            verify_cnt_total += 3;
        } else if (verify_cnt_total < 2 + 2 * 127 - 3) {
            // Continuation
            MBED_ASSERT(p[0] == 0x04);
            MBED_ASSERT(p[1] == verify_cnt);
            verify_cnt = (verify_cnt + 1) == 127 ? 0 : verify_cnt + 1;
            MBED_ASSERT(p[2] == verify_cnt);
            verify_cnt = (verify_cnt + 1) == 127 ? 0 : verify_cnt + 1;
            MBED_ASSERT(p[3] == verify_cnt);
            verify_cnt = (verify_cnt + 1) == 127 ? 0 : verify_cnt + 1;
            verify_cnt_total += 3;
        } else {
            // Last message
            MBED_ASSERT(p[0] == 0x05);
            MBED_ASSERT(p[1] == 0xF7);
            verify_cnt = 0;
            verify_cnt_total = 0;
        }
    }
}

void PluggableUSBMIDI::read_and_verify() {
    // Check if there are any bytes available for reading
    uint32_t available = reading.available.load(mo_acq);
    if (available == 0)
        return;

    // Get the buffer with received data
    uint32_t r = reading.read_idx.load(mo_rlx);
    rbuffer_t &readbuffer = reading.buffers[r];

    verify_data(readbuffer.buffer, readbuffer.size);

    // Increment the read index (and wrap around)
    r = (r + 1 == NumRxPackets) ? 0 : r + 1;
    reading.read_idx.store(r, mo_rlx);
    reading.available.fetch_sub(1, mo_rel);
    // There is now space in the queue
    // Check if the next read is already in progress
    if (reading.reading.exchange(true, mo_acq) == false) {
        // If not, start the next read now
        uint32_t w = reading.write_idx.load(mo_rlx);
        ser.println("rsm"); // read start main
        read_start(bulk_out_ep, reading.buffers[w].buffer, PacketSize);
    }
}

void PluggableUSBMIDI::out_callback() {
    assert_locked();
    MBED_ASSERT(reading.reading.load(mo_rlx) == true);
    // Check how many bytes were read
    uint32_t num_bytes_read = read_finish(bulk_out_ep);
    ser.println(num_bytes_read);
    MBED_ASSERT(num_bytes_read % 4 == 0);

    // If no bytes were read, start a new read into the same buffer
    uint32_t w = reading.write_idx.load(mo_rlx);
    if (num_bytes_read == 0) {
        ser.println("rs0"); // read start interrupt (size 0)
        read_start(bulk_out_ep, reading.buffers[w].buffer, PacketSize);
        return;
    }

    // Otherwise, store how many bytes were read
    rbuffer_t &writebuffer = reading.buffers[w];
    writebuffer.size = num_bytes_read;
    // Increment the write index (and wrap around)
    w = (w + 1 == NumRxPackets) ? 0 : w + 1;
    reading.write_idx.store(w, mo_rlx);

    // Update number of available buffers in the queue
    uint32_t available = reading.available.fetch_add(1, mo_acq_rel) + 1;
    // If there's still space left in the queue, start the next read
    if (available < NumRxPackets) {
        ser.println("rsi"); // read start interrupt
        read_start(bulk_out_ep, reading.buffers[w].buffer, PacketSize);
    }
    // Otherwise, release the “reading” lock
    else {
        reading.reading.store(false, mo_rel);
    }
}

constexpr uint32_t PluggableUSBMIDI::PacketSize;
constexpr uint32_t PluggableUSBMIDI::NumRxPackets;
