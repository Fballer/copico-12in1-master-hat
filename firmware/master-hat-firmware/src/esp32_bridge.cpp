#include "esp32_bridge.h"

// Define the SPI pins (From Phase 10 Refactor / Phase 7 Build Guide)
#define BRIDGE_MISO 23
#define BRIDGE_MOSI 24
#define BRIDGE_CS   26
#define BRIDGE_SCK  28

Esp32Bridge esp32;

Esp32Bridge::Esp32Bridge() {
}

void Esp32Bridge::init() {
    pinMode(BRIDGE_CS, OUTPUT);
    digitalWrite(BRIDGE_CS, HIGH);

    // Initialize SPI hardware on RP2350
    SPI.setRX(BRIDGE_MISO);
    SPI.setSCK(BRIDGE_SCK);
    SPI.setTX(BRIDGE_MOSI);
    SPI.begin();
}

void Esp32Bridge::transfer(const uint8_t* tx_buf, uint8_t* rx_buf, size_t len) {
    digitalWrite(BRIDGE_CS, LOW);
    // Give the ESP32 SPI Slave a tiny microsecond delay to recognize CS
    delayMicroseconds(5); 
    
    for (size_t i = 0; i < len; i++) {
        rx_buf[i] = SPI.transfer(tx_buf[i]);
    }
    
    digitalWrite(BRIDGE_CS, HIGH);
    // Give ESP32 time to process after transaction
    delayMicroseconds(50); 
}

bool Esp32Bridge::transaction(SpiMasterPacket* tx_packet, SpiSlavePacket* rx_packet) {
    // 1. Send the primary packet
    tx_packet->sync = SPI_SYNC_BYTE;
    tx_packet->checksum = calc_checksum((uint8_t*)tx_packet, sizeof(SpiMasterPacket) - 1);
    
    transfer((uint8_t*)tx_packet, (uint8_t*)rx_packet, sizeof(SpiMasterPacket));

    // 2. Poll if ESP32 is busy
    SpiMasterPacket poll_packet;
    memset(&poll_packet, 0, sizeof(poll_packet));
    poll_packet.sync = SPI_SYNC_BYTE;
    poll_packet.command = CMD_POLL;
    poll_packet.checksum = calc_checksum((uint8_t*)&poll_packet, sizeof(poll_packet) - 1);

    uint32_t timeout = millis() + 5000; // 5 second maximum timeout for SD operations
    
    while (millis() < timeout) {
        // If we received a valid response on the first try, break
        if (rx_packet->sync == SPI_SYNC_BYTE && calc_checksum((uint8_t*)rx_packet, sizeof(SpiSlavePacket) - 1) == rx_packet->checksum) {
            if (rx_packet->status != STATUS_SDC_BUSY) {
                return true; // Valid terminal status
            }
        }
        
        // Wait a short moment before polling again to not hammer the SPI bus
        delay(1);
        
        // Send POLL
        transfer((uint8_t*)&poll_packet, (uint8_t*)rx_packet, sizeof(SpiMasterPacket));
    }
    
    return false; // Timeout
}

bool Esp32Bridge::sdc_read_sector(uint8_t drive_id, uint32_t lsn, uint8_t* buffer) {
    SpiMasterPacket tx;
    SpiSlavePacket rx;
    memset(&tx, 0, sizeof(tx));
    
    tx.command = CMD_SDC_READ;
    tx.length = 4;
    tx.payload[0] = drive_id;
    tx.payload[1] = (lsn >> 16) & 0xFF;
    tx.payload[2] = (lsn >> 8) & 0xFF;
    tx.payload[3] = lsn & 0xFF;
    
    if (transaction(&tx, &rx)) {
        if (rx.status == STATUS_SDC_SECTOR) {
            memcpy(buffer, rx.payload, 256);
            return true;
        }
    }
    return false;
}

bool Esp32Bridge::sdc_write_sector(uint8_t drive_id, uint32_t lsn, const uint8_t* buffer) {
    SpiMasterPacket tx;
    SpiSlavePacket rx;
    memset(&tx, 0, sizeof(tx));
    
    tx.command = CMD_SDC_WRITE;
    tx.length = 4 + 256;
    tx.payload[0] = drive_id;
    tx.payload[1] = (lsn >> 16) & 0xFF;
    tx.payload[2] = (lsn >> 8) & 0xFF;
    tx.payload[3] = lsn & 0xFF;
    memcpy(&tx.payload[4], buffer, 256);
    
    if (transaction(&tx, &rx)) {
        if (rx.status == STATUS_SDC_ACK && rx.payload[0] == 0) {
            return true;
        }
    }
    return false;
}

bool Esp32Bridge::sdc_mount(uint8_t drive_id, const char* filename) {
    SpiMasterPacket tx;
    SpiSlavePacket rx;
    memset(&tx, 0, sizeof(tx));
    
    tx.command = CMD_SDC_MOUNT;
    tx.payload[0] = drive_id;
    
    size_t len = strlen(filename);
    if (len > 250) len = 250; // Safety limit
    
    memcpy(&tx.payload[1], filename, len);
    tx.payload[1 + len] = '\0';
    tx.length = 1 + len + 1;
    
    if (transaction(&tx, &rx)) {
        if (rx.status == STATUS_SDC_ACK && rx.payload[0] == 0) {
            return true;
        }
    }
    return false;
}

bool Esp32Bridge::sdc_swap() {
    SpiMasterPacket tx;
    SpiSlavePacket rx;
    memset(&tx, 0, sizeof(tx));
    
    tx.command = CMD_SDC_SWAP;
    tx.length = 0;
    
    if (transaction(&tx, &rx)) {
        if (rx.status == STATUS_SDC_ACK && rx.payload[0] == 0) {
            return true;
        }
    }
    return false;
}
