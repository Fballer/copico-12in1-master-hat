#include <Arduino.h>
#include <EEPROM.h>
#include <I2S.h>
#include "hardware/pio.h"
#include "coco_bus.pio.h"

// Emulators
#include "pic7040.h"
#include "acia6551.h"
#include "v9958.h"
#include "vga_driver.h"
#include "emulator_mode.h"
#include "boot_menu.h"
#include "cocosdc.h"
#include "spi_stream.h"
#include "esp32_bridge.h"
#include "rtc.h"

// Hardware Pin Definitions
#define PIN_E_CLOCK 21
#define PIN_RW 20
#define PIN_BTN_DUAL 19
#define PIN_STATUS_LED 17

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
EmulatorMode current_mode = MODE_BOOT_MENU;

// Boot Menu instance
BootMenu boot_menu;

// RTC Emulation instance
Rtc rtc;

// RS-232 Hardware UART Pins (via SerialPIO to map to any GPIO)
SerialPIO rs232_serial(13, 15, 256); // TX=G13, RX=G15, 256-byte FIFO

// Global emulator instances
Pic7040 pic7040;
Acia6551 acia(&rs232_serial);
SpiStream spi_wimodem_stream;
Acia6551 wimodem(&spi_wimodem_stream);
V9958 v9958;
VgaDriver vga_driver(&v9958);
Cocosdc cocosdc;

// Button Timer State
uint32_t btn_press_start = 0;
bool btn_is_pressed = false;
bool btn_warning_active = false;

// Function prototypes
void update_orch90_audio();
void switch_mode(EmulatorMode new_mode);

void setup() {
    Serial.begin(115200);
#ifdef PICO_DEFAULT_LED_PIN
    pinMode(PICO_DEFAULT_LED_PIN, OUTPUT);
#endif

    pinMode(PIN_BTN_DUAL, INPUT_PULLUP);
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);
    delay(50); // Debounce settle
    
    EEPROM.begin(512);
    uint8_t saved_mode = EEPROM.read(0);
    if (saved_mode > MODE_MAX) saved_mode = (uint8_t)MODE_COCOSDC;
    
    // Check if button is held during boot
    if (digitalRead(PIN_BTN_DUAL) == LOW) {
        current_mode = MODE_BOOT_MENU;
        Serial.println("Booting to: BOOT MENU (Button Held)");
    } else {
        current_mode = (EmulatorMode)saved_mode;
        Serial.print("Booting to Saved Mode: ");
        Serial.println(current_mode);
    }
    
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

    // 7. Initialize SpiStream for Coprocessor
    spi_wimodem_stream.begin();
    
    // 8. Initialize RTC Emulation
    rtc.init();

    // 9. Delegate initial peripheral setup
    switch_mode(current_mode);
}

void loop() {
    // 1. Check Dual-Action Button
    if (digitalRead(PIN_BTN_DUAL) == LOW) {
        if (!btn_is_pressed) {
            btn_is_pressed = true;
            btn_press_start = millis();
            btn_warning_active = false;
        } else {
            uint32_t hold_time = millis() - btn_press_start;
            
            if (hold_time >= 5000) {
                // 5-Second Hold -> Reset to Boot Menu
                digitalWrite(PIN_STATUS_LED, LOW);
                btn_is_pressed = false; // Reset state before jumping
                Serial.println("System Reset Triggered -> Returning to Boot Menu");
                switch_mode(MODE_BOOT_MENU);
            } 
            else if (hold_time >= 3000) {
                // 3-Second Warning -> Flash LED rapidly
                digitalWrite(PIN_STATUS_LED, (millis() / 100) % 2);
                btn_warning_active = true;
            }
        }
    } else {
        if (btn_is_pressed) {
            uint32_t hold_time = millis() - btn_press_start;
            btn_is_pressed = false;
            
            if (btn_warning_active) {
                // They let go after warning but before reset. Just turn off LED.
                digitalWrite(PIN_STATUS_LED, LOW);
                btn_warning_active = false;
            } 
            else if (hold_time < 1000 && current_mode == MODE_COCOSDC) {
                // Short press -> Disk Swap
                Serial.println("Disk Swap Triggered");
                esp32.sdc_swap();
            }
        }
    }

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
    } else if (current_mode == MODE_COCOSDC) {
        cocosdc.tick();
    } else if (current_mode == MODE_WIMODEM) {
        spi_wimodem_stream.tick();
    } else if (current_mode == MODE_BOOT_MENU) {
        // Heartbeat LED
#ifdef PICO_DEFAULT_LED_PIN
        digitalWrite(PICO_DEFAULT_LED_PIN, (millis() / 500) % 2);
#endif
        delay(10);
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
            if (current_mode == MODE_BOOT_MENU && addr >= 0xC000 && addr <= 0xDFFF) {
                uint8_t data_out = boot_menu.read_rom(addr);
                gpio_set_dir_out_masked(0xFF);
                gpio_put_masked(0xFF, data_out);
                while (gpio_get(PIN_E_CLOCK)) {}
                gpio_set_dir_in_masked(0xFF);
            } else if (current_mode == MODE_COCOSDC && addr >= 0xC000 && addr <= 0xDFFF) {
                uint8_t data_out = cocosdc.read_rom(addr);
                gpio_set_dir_out_masked(0xFF);
                gpio_put_masked(0xFF, data_out);
                while (gpio_get(PIN_E_CLOCK)) {}
                gpio_set_dir_in_masked(0xFF);
            } else if (addr == 0xFF7F) {
                // Readback for current mode (Trigger Port)
                gpio_set_dir_out_masked(0xFF);
                gpio_put_masked(0xFF, (uint8_t)current_mode);
                while (gpio_get(PIN_E_CLOCK)) {}
                gpio_set_dir_in_masked(0xFF);
            } else if (current_mode == MODE_SPEECH_SOUND && addr == 0xFF7E) {
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
            } else if (current_mode == MODE_WIMODEM && (addr >= 0xFF68 && addr <= 0xFF6B)) {
                uint8_t data_out = 0;
                if (addr == 0xFF68) {
                    data_out = wimodem.read_data();
                } else if (addr == 0xFF69) {
                    data_out = wimodem.read_status();
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
            } else if (current_mode == MODE_COCOSDC && (addr == 0xFF40 || (addr >= 0xFF48 && addr <= 0xFF4B))) {
                uint8_t data_out = cocosdc.read_register(addr);
                
                gpio_set_dir_out_masked(0xFF);
                gpio_put_masked(0xFF, data_out);
                while (gpio_get(PIN_E_CLOCK)) {}
                gpio_set_dir_in_masked(0xFF);
            } else if (addr == 0xFF50) {
                uint8_t data_out = rtc.read(addr);
                
                gpio_set_dir_out_masked(0xFF);
                gpio_put_masked(0xFF, data_out);
                while (gpio_get(PIN_E_CLOCK)) {}
                gpio_set_dir_in_masked(0xFF);
            }
        } else {
            // Write Cycle Logic
            if (current_mode == MODE_BOOT_MENU && addr >= 0xFF70 && addr <= 0xFF73) {
                boot_menu.set_config(addr, (uint8_t)data);
                return;
            }
            if (addr == 0xFF7F) {
                if (current_mode == MODE_BOOT_MENU && data == 0x55) {
                    EmulatorMode new_mode = boot_menu.calculate_mode();
                    EEPROM.write(0, (uint8_t)new_mode);
                    EEPROM.commit();
                    switch_mode(new_mode);
                } else {
                    switch_mode((EmulatorMode)data);
                }
                return;
            } else if (addr == 0xFF51 || addr == 0xFF75) {
                rtc.write(addr, (uint8_t)data);
                return;
            }
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
                    
                case MODE_WIMODEM:
                    if (addr == 0xFF68) {
                        wimodem.write_data((uint8_t)data);
                    } else if (addr == 0xFF6A) {
                        wimodem.write_command((uint8_t)data);
                    } else if (addr == 0xFF6B) {
                        wimodem.write_control((uint8_t)data);
                    }
                    break;
                    
                case MODE_WORDPAK2:
                    if (addr >= 0xFF78 && addr <= 0xFF7B) {
                        v9958.write_port(addr, data);
                    }
                    break;
                    
                case MODE_COCOSDC:
                    if (addr == 0xFF40 || (addr >= 0xFF48 && addr <= 0xFF4B)) {
                        cocosdc.write_register(addr, data);
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

void switch_mode(EmulatorMode new_mode) {
    if (current_mode == new_mode) return;
    
    Serial.print("Switching mode from ");
    Serial.print(current_mode);
    Serial.print(" to ");
    Serial.println(new_mode);
    
    // 1. Cleanup old mode
    // Send soft-reset to ESP32 to clear any active connections or buffers
    esp32.send_command(CMD_RESET, nullptr, 0);
    
    current_mode = new_mode;
    
    // 2. Initialize new mode
    switch (current_mode) {
        case MODE_BOOT_MENU:
            boot_menu.init();
            break;
        case MODE_RS232_PAK_LEGACY:
            acia.init(false);
            break;
        case MODE_RS232_PAK_TURBO:
            acia.init(true);
            break;
        case MODE_WORDPAK2:
            vga_driver.init();
            break;
        case MODE_COCOSDC:
            cocosdc.init();
            break;
        case MODE_WIMODEM:
            wimodem.init(true); // Always turbo mode for wimodem
            break;
        case MODE_ORCH90:
        case MODE_SPEECH_SOUND:
            // No specific init required
            break;
    }
}
