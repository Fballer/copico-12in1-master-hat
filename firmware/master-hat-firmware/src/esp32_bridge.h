#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "spi_protocol.h"

class Esp32Bridge {
public:
    Esp32Bridge();
    void init();
    
    // Core transaction method (blocks until Slave replies with something other than BUSY)
    bool transaction(SpiMasterPacket* tx_packet, SpiSlavePacket* rx_packet);

    // SDC Specific Helpers
    bool sdc_read_sector(uint8_t drive_id, uint32_t lsn, uint8_t* buffer);
    bool sdc_write_sector(uint8_t drive_id, uint32_t lsn, const uint8_t* buffer);
    bool sdc_mount(uint8_t drive_id, const char* filename);
    bool sdc_swap();

private:
    void send_packet(SpiMasterPacket* packet);
    bool receive_packet(SpiSlavePacket* packet);
    void transfer(const uint8_t* tx_buf, uint8_t* rx_buf, size_t len);
};

extern Esp32Bridge esp32;
