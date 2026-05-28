#include "cocosdc.h"
#include "esp32_bridge.h"
#include "flash_rom_manager.h"

// Hardware Pin Definitions (From Phase 10 Refactor)
#define SDC_LED 17

Cocosdc::Cocosdc() {
    extended_mode_active = false;
    status_reg = 0;
    current_lsn = 0;
    buffer_index = 0;
    pending_command = 0;
    active_write_command = 0;
}

void Cocosdc::init() {
    pinMode(SDC_LED, OUTPUT);
    digitalWrite(SDC_LED, LOW);
    
    // Initialize the SPI Bridge
    esp32.init();
    
    if (!flash_rom.load_rom_to_buffer(SLOT_COCOSDC, rom_buffer, nullptr, nullptr)) {
        Serial.println("[CoCoSDC] SDC-DOS not in flash — ROM shadow empty");
        memset(rom_buffer, 0xFF, sizeof(rom_buffer));
    } else {
        Serial.println("[CoCoSDC] SDC-DOS loaded into ROM shadow");
    }
    
    // We will ask the ESP32 to mount the default startup disks
    // This could also be a command we send, but we'll let the ESP32 handle default mounting internally on boot.
}

void Cocosdc::parse_startup_cfg() {
    // Deprecated: ESP32 now handles startup.cfg internally.
}

void Cocosdc::mount_drive(uint8_t drive_num, const String& filename) {
    if (drive_num > 1) return;
    
    Serial.print("CoCoSDC: Requesting ESP32 to mount ");
    Serial.print(filename);
    Serial.print(" to Drive ");
    Serial.println(drive_num);
    
    if (esp32.sdc_mount(drive_num, filename.c_str())) {
        drive_paths[drive_num] = filename;
        Serial.println("CoCoSDC: Mount successful.");
    } else {
        Serial.println("CoCoSDC: Mount failed (ESP32 error).");
    }
}

void Cocosdc::tick() {
    if (pending_command == 0) return;
    
    digitalWrite(SDC_LED, HIGH);
    
    uint8_t cmd = pending_command;
    
    if (cmd == 0x80) { // Read Sector
        // Send request to ESP32
        if (esp32.sdc_read_sector(0, current_lsn, sector_buffer)) {
            status_reg = 0x02; // READY
        } else {
            status_reg = 0x80; // ERROR
        }
    } 
    else if (cmd == 0xA0) { // Write Sector
        if (esp32.sdc_write_sector(0, current_lsn, sector_buffer)) {
            status_reg = 0x00; // Success
        } else {
            status_reg = 0x80; // ERROR
        }
    }
    else if (cmd == 0xE0 || cmd == 0xE1) { // Mount Drive 0 or 1
        uint8_t drive_num = (cmd == 0xE0) ? 0 : 1;
        // filename is passed in the sector buffer (null-terminated or space padded)
        char filename[257];
        memcpy(filename, sector_buffer, 256);
        filename[256] = '\0'; // Ensure null termination
        
        // Trim trailing spaces and nulls
        for (int i = 255; i >= 0; i--) {
            if (filename[i] == ' ' || filename[i] == '\0') {
                filename[i] = '\0';
            } else {
                break;
            }
        }
        
        mount_drive(drive_num, String(filename));
        status_reg = 0x00; // Success
    }
    
    pending_command = 0;
    digitalWrite(SDC_LED, LOW);
}

uint8_t Cocosdc::read_rom(uint16_t addr) {
    if (addr >= 0xC000 && addr <= 0xDFFF) {
        return rom_buffer[addr - 0xC000];
    }
    return 0xFF;
}

uint8_t Cocosdc::read_register(uint16_t addr) {
    if (!extended_mode_active) return 0xFF; // Fallback for standard FDC logic not implemented yet
    
    if (addr == 0xFF48) {
        return status_reg;
    } else if (addr == 0xFF49) {
        return (current_lsn >> 16) & 0xFF;
    } else if (addr == 0xFF4A) {
        if (status_reg & 0x02) { // READY to transfer data to CoCo
            uint8_t val = sector_buffer[buffer_index++];
            if (buffer_index >= 256) {
                status_reg = 0x00; // Clear READY
            }
            return val;
        } else {
            return (current_lsn >> 8) & 0xFF;
        }
    } else if (addr == 0xFF4B) {
        if (status_reg & 0x02) { // READY to transfer data to CoCo
            uint8_t val = sector_buffer[buffer_index++];
            if (buffer_index >= 256) {
                status_reg = 0x00; // Clear READY
            }
            return val;
        } else {
            return current_lsn & 0xFF;
        }
    }
    
    return 0xFF;
}

void Cocosdc::write_register(uint16_t addr, uint8_t data) {
    if (addr == 0xFF40) {
        if (data == 0x43) {
            extended_mode_active = true;
        }
    } else if (extended_mode_active) {
        if (addr == 0xFF48) {
            if (data == 0x80) { // Read
                buffer_index = 0;
                status_reg = 0x01; // BUSY
                pending_command = data;
            } else if (data == 0xA0 || data == 0xE0 || data == 0xE1) { // Write or Mount
                buffer_index = 0;
                status_reg = 0x02; // READY for CoCo to fill buffer
                active_write_command = data;
            }
        } else if (addr == 0xFF49) {
            current_lsn = (current_lsn & 0x00FFFF) | ((uint32_t)data << 16);
        } else if (addr == 0xFF4A) {
            if (status_reg & 0x02) { // READY to receive data from CoCo
                sector_buffer[buffer_index++] = data;
                if (buffer_index >= 256) {
                    status_reg = 0x01; // BUSY
                    pending_command = active_write_command;
                    active_write_command = 0;
                }
            } else {
                current_lsn = (current_lsn & 0xFF00FF) | ((uint32_t)data << 8);
            }
        } else if (addr == 0xFF4B) {
            if (status_reg & 0x02) { // READY to receive data from CoCo
                sector_buffer[buffer_index++] = data;
                if (buffer_index >= 256) {
                    status_reg = 0x01; // BUSY
                    pending_command = active_write_command;
                    active_write_command = 0;
                }
            } else {
                current_lsn = (current_lsn & 0xFFFF00) | data;
            }
        }
    }
}
