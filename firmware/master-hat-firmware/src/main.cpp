#include <Arduino.h>
#include <I2S.h>
#include "hardware/pio.h"
#include "coco_bus.pio.h"

// Emulators
#include "pic7040.h"
#include "acia6551.h"
#include "v9958.h"
#include "vga_driver.h"

// Hardware Pin Definitions
#define PIN_E_CLOCK 21
#define PIN_RW 20

// I2S Pins (from build guide)
#define I2S_BCK 10
#define I2S_DIN 11
#define I2S_LCK 12

// PIO Configuration
PIO pio = pio0;
uint sm_addr, sm_data;

// Audio state for Orch-90
uint8_t dac_left = 128;
uint8_t dac_right = 128;

// Initialize I2S instance for audio output
I2S i2s(OUTPUT);

// Mode Selection
enum EmulatorMode {
    MODE_ORCH90,
    MODE_SPEECH_SOUND,
    MODE_RS232_PAK_LEGACY,
    MODE_RS232_PAK_TURBO,
    MODE_WORDPAK2
};
EmulatorMode current_mode = MODE_WORDPAK2; // Phase 4 testing

// RS-232 Hardware UART Pins (via SerialPIO to map to any GPIO)
SerialPIO rs232_serial(13, 15, 256); // TX=G13, RX=G15, 256-byte FIFO

// Global emulator instances
Pic7040 pic7040;
Acia6551 acia(&rs232_serial);
V9958 v9958;
VgaDriver vga_driver(&v9958);

// Function prototype
void update_orch90_audio();

void setup() {
    Serial.begin(115200);
    
    // 1. Initialize I2S DAC (PCM5102A)
    i2s.setBCLK(I2S_BCK);
    i2s.setDATA(I2S_DIN);
    i2s.setBitsPerSample(16); // 16-bit frames
    i2s.begin(44100);

    // 2. Load PIO Programs
    uint offset_addr = pio_add_program(pio, &coco_sniffer_addr_program);
    uint offset_data = pio_add_program(pio, &coco_sniffer_data_program);

    sm_addr = pio_claim_unused_sm(pio, true);
    sm_data = pio_claim_unused_sm(pio, true);

    // 3. Configure SM_ADDR
    pio_sm_config c_addr = coco_sniffer_addr_program_get_default_config(offset_addr);
    sm_config_set_in_pins(&c_addr, 32); // A0 is GPIO 32
    pio_sm_init(pio, sm_addr, offset_addr, &c_addr);

    // 4. Configure SM_DATA
    pio_sm_config c_data = coco_sniffer_data_program_get_default_config(offset_data);
    sm_config_set_in_pins(&c_data, 0);  // D0 is GPIO 0
    pio_sm_init(pio, sm_data, offset_data, &c_data);

    // 5. Initialize GPIO for data bus fast-turnaround
    // (Pins 0-7 are already input by default, just setting up masks)
    gpio_init_mask(0xFF);
    gpio_set_dir_in_masked(0xFF);

    // 6. Start both state machines synchronously
    pio_enable_sm_mask_in_sync(pio, (1u << sm_addr) | (1u << sm_data));

    // 7. Initialize RS-232 Emulation
    if (current_mode == MODE_RS232_PAK_LEGACY) {
        acia.init(false);
    } else if (current_mode == MODE_RS232_PAK_TURBO) {
        acia.init(true);
    }
    
    // 8. Initialize VGA Driver
    if (current_mode == MODE_WORDPAK2) {
        vga_driver.init();
    }
}

void loop() {
    // Core 0 handles system tasks and Audio Generation / VGA Rendering
    if (current_mode == MODE_SPEECH_SOUND) {
        pic7040.tick();
        
        int16_t sample_l = 0, sample_r = 0;
        pic7040.generate_audio(&sample_l, &sample_r);
        
        // Push to I2S DAC (blocks if buffer is full)
        i2s.write(sample_l);
        i2s.write(sample_r);
    } else if (current_mode == MODE_WORDPAK2) {
        vga_driver.tick();
    } else {
        delay(1);
    }
}

// Core 1 loop - Dedicated to processing the CoCo bus
void setup1() {
    // Core 1 setup
}

void loop1() {
    // Poll the FIFOs tightly
    if (!pio_sm_is_rx_fifo_empty(pio, sm_addr) && !pio_sm_is_rx_fifo_empty(pio, sm_data)) {
        uint32_t addr = pio_sm_get(pio, sm_addr);
        uint32_t data = pio_sm_get(pio, sm_data);
        bool is_read = gpio_get(PIN_RW);

        if (is_read) {
            // Read Cycle Logic
            if (current_mode == MODE_SPEECH_SOUND && addr == 0xFF7E) {
                uint8_t status = pic7040.read_status();
                
                // Turn around data bus (GPIO 0-7)
                gpio_set_dir_out_masked(0xFF);
                gpio_put_masked(0xFF, status);
                
                // Wait for E-clock to fall so CoCo can latch the data
                while (gpio_get(PIN_E_CLOCK)) {} 
                
                // Instantly revert to input
                gpio_set_dir_in_masked(0xFF);
            } else if ((current_mode == MODE_RS232_PAK_LEGACY || current_mode == MODE_RS232_PAK_TURBO) && (addr >= 0xFF68 && addr <= 0xFF6B)) {
                uint8_t data_out = 0;
                if (addr == 0xFF68) {
                    data_out = acia.read_data();
                } else if (addr == 0xFF69) {
                    data_out = acia.read_status();
                }
                
                gpio_set_dir_out_masked(0xFF);
                gpio_put_masked(0xFF, data_out);
                while (gpio_get(PIN_E_CLOCK)) {}
                gpio_set_dir_in_masked(0xFF);
            } else if (current_mode == MODE_WORDPAK2 && (addr == 0xFF78 || addr == 0xFF79)) {
                uint8_t data_out = v9958.read_port(addr);
                
                gpio_set_dir_out_masked(0xFF);
                gpio_put_masked(0xFF, data_out);
                while (gpio_get(PIN_E_CLOCK)) {}
                gpio_set_dir_in_masked(0xFF);
            }
        } else {
            // Write Cycle Logic
            switch (current_mode) {
                case MODE_ORCH90:
                    if (addr == 0xFF7A) {
                        dac_left = (uint8_t)data;
                        update_orch90_audio();
                    } else if (addr == 0xFF7B) {
                        dac_right = (uint8_t)data;
                        update_orch90_audio();
                    }
                    break;
                    
                case MODE_SPEECH_SOUND:
                    if (addr == 0xFF7D) {
                        if (data & 0x01) {
                            pic7040.reset();
                        }
                    } else if (addr == 0xFF7E) {
                        pic7040.write_data((uint8_t)data);
                    }
                    break;
                    
                case MODE_RS232_PAK_LEGACY:
                case MODE_RS232_PAK_TURBO:
                    if (addr == 0xFF68) {
                        acia.write_data((uint8_t)data);
                    } else if (addr == 0xFF6A) {
                        acia.write_command((uint8_t)data);
                    } else if (addr == 0xFF6B) {
                        acia.write_control((uint8_t)data);
                    }
                    break;
                    
                case MODE_WORDPAK2:
                    if (addr >= 0xFF78 && addr <= 0xFF7B) {
                        v9958.write_port(addr, data);
                    }
                    break;
            }
        }
    }
}

void update_orch90_audio() {
    // Convert 8-bit unsigned/signed to 16-bit I2S frame
    int16_t sample_l = ((int16_t)dac_left - 128) << 8;
    int16_t sample_r = ((int16_t)dac_right - 128) << 8;
    
    // Push to I2S DAC
    i2s.write(sample_l);
    i2s.write(sample_r);
}
