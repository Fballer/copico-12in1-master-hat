#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "spi_protocol.h"

extern volatile bool spi_bus_locked;

// 256-byte ring buffers for serial data
#define SPI_STREAM_BUFFER_SIZE 256

class SpiStream : public Stream {
public:
    SpiStream();

    void begin();
    void tick(); // Call this frequently in Core 0

    // Stream interface implementation
    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;

    // Direct byte access for Core 1 (faster than virtual calls)
    inline bool has_rx() const { return _rx_head != _rx_tail; }
    inline uint8_t pop_rx() {
        if (_rx_head == _rx_tail) return 0;
        uint8_t val = _rx_buffer[_rx_tail];
        _rx_tail = (_rx_tail + 1) % SPI_STREAM_BUFFER_SIZE;
        return val;
    }
    
    inline bool has_tx_space() const {
        return ((_tx_head + 1) % SPI_STREAM_BUFFER_SIZE) != _tx_tail;
    }
    inline void push_tx(uint8_t val) {
        uint16_t next = (_tx_head + 1) % SPI_STREAM_BUFFER_SIZE;
        if (next != _tx_tail) {
            _tx_buffer[_tx_head] = val;
            _tx_head = next;
        }
    }

private:
    uint8_t _rx_buffer[SPI_STREAM_BUFFER_SIZE];
    volatile uint16_t _rx_head = 0;
    volatile uint16_t _rx_tail = 0;

    uint8_t _tx_buffer[SPI_STREAM_BUFFER_SIZE];
    volatile uint16_t _tx_head = 0;
    volatile uint16_t _tx_tail = 0;

    uint32_t _last_poll_time = 0;
    const uint32_t POLL_INTERVAL_MS = 5;

    SpiMasterPacket _tx_packet;
    SpiSlavePacket _rx_packet;
};
