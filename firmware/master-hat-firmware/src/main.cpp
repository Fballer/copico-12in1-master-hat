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

// ==========================================================
// Hardware Pin Definitions — Verified against PCB Netlist
// Centipede 32z Hat-Fuji-40C schematic, dated 2026-04-19
// ==========================================================

// J3 Header — System Control
#define PIN_BTN_DUAL   19  // J3 Pin 17: SDC_Button_G19
#define PIN_STATUS_LED 17  // J3 Pin 15: SDC_LED_G17
// Note: J3 Pin 18 = RESET line (hardware Reset_Pad)

// J3 Header — I2S DAC (PCM5102A: Orch-90 / Speech+Sound)
#define I2S_BCK 10  // J3 Pin 5:  Sound_BCK_G10  -> PCM5102A BCK
#define I2S_DIN 11  // J3 Pin 7:  Sound_DIN_G11  -> PCM5102A DIN
#define I2S_LCK 12  // J3 Pin 9:  Sound_LCK_G12  -> PCM5102A LCK

// J3 Header — RS-232 (HW-044 MAX3232 Module)
// TX=G13 and RX=G15 are handled by SerialPIO (see below)

// PIO Configuration
PIO pio = pio0;
uint sm_addr, sm_data, sm_read;

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
    uint offset_read = pio_add_program(pio, &coco_bus_read_program);

    sm_addr = pio_claim_unused_sm(pio, true);
    sm_data = pio_claim_unused_sm(pio, true);
    sm_read = pio_claim_unused_sm(pio, true);

    // 3. Configure SM_ADDR
    pio_sm_config c_addr = coco_sniffer_addr_program_get_default_config(offset_addr);
    sm_config_set_in_pins(&c_addr, 32); // A0 is GPIO 32
    pio_sm_init(pio, sm_addr, offset_addr, &c_addr);

    // 4. Configure SM_DATA
    pio_sm_config c_data = coco_sniffer_data_program_get_default_config(offset_data);
    sm_config_set_in_pins(&c_data, 0);  // D0 is GPIO 0
    pio_sm_init(pio, sm_data, offset_data, &c_data);

    // 5. Configure SM_READ (PIO-Accelerated Read Response)
    // Runs at full 150MHz (clkdiv=1) for minimum latency.
    // out_base=0 (D0-D7), set_base=0 (for pindirs), out_shift_right=true
    pio_sm_config c_read = coco_bus_read_program_get_default_config(offset_read);
    sm_config_set_out_pins(&c_read, 0, 8);   // Drive D0-D7 (GPIO 0-7)
    sm_config_set_set_pins(&c_read, 0, 8);   // Set pindirs for D0-D7
    sm_config_set_out_shift(&c_read, true, false, 8); // Shift right, no autopull
    sm_config_set_clkdiv(&c_read, 1.0f);     // Full 150MHz — minimum latency
    // Initialize GPIO 0-7 as PIO-controlled (but input until SM drives them)
    for (int i = 0; i < 8; i++) pio_gpio_init(pio, i);
    pio_sm_set_consecutive_pindirs(pio, sm_read, 0, 8, false); // Start as inputs
    pio_sm_init(pio, sm_read, offset_read, &c_read);
    pio_sm_set_enabled(pio, sm_read, true); // Start immediately — it idles on pull block

    // 6. Initialize GPIO for data bus (Pins 0-7 start as inputs)
    gpio_init_mask(0xFF);
    gpio_set_dir_in_masked(0xFF);

    // 7. Start address and data sniffer state machines synchronously
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

// ---------------------------------------------------------------
// BUS_RESPOND(byte): Fast PIO-based read response macro.
// Hands the data byte to the coco_bus_read PIO state machine,
// which drives GPIO 0-7, waits for E-clock fall, then tri-states.
// Replaces the old ~150ns gpio_set_dir_out_masked + gpio_put_masked
// sequence with a single ~7ns FIFO write.
// ---------------------------------------------------------------
#define BUS_RESPOND(byte) pio_sm_put_blocking(pio, sm_read, (uint32_t)(byte))

void loop1() {
    // Tight poll: only proceed if both FIFOs have data
    if (pio_sm_is_rx_fifo_empty(pio, sm_addr) || pio_sm_is_rx_fifo_empty(pio, sm_data)) return;

    uint32_t addr = pio_sm_get(pio, sm_addr);
    uint32_t data = pio_sm_get(pio, sm_data);
    bool is_read = gpio_get(PIN_RW);

        if (is_read) {
            // --- READ CYCLE ---
            // Use BUS_RESPOND() to hand data to the PIO SM — fast path.
            if (current_mode == MODE_BOOT_MENU && addr >= 0xC000 && addr <= 0xDFFF) {
                BUS_RESPOND(boot_menu.read_rom(addr));
            } else if (current_mode == MODE_COCOSDC && addr >= 0xC000 && addr <= 0xDFFF) {
                BUS_RESPOND(cocosdc.read_rom(addr));
            } else if (addr == 0xFF7F) {
                BUS_RESPOND((uint8_t)current_mode);
            } else if (current_mode == MODE_SPEECH_SOUND && addr == 0xFF7E) {
                BUS_RESPOND(pic7040.read_status());
            } else if ((current_mode == MODE_RS232_PAK_LEGACY || current_mode == MODE_RS232_PAK_TURBO)
                       && addr >= 0xFF68 && addr <= 0xFF6B) {
                uint8_t d = (addr == 0xFF68) ? acia.read_data() : acia.read_status();
                BUS_RESPOND(d);
            } else if (current_mode == MODE_WIMODEM && addr >= 0xFF68 && addr <= 0xFF6B) {
                uint8_t d = (addr == 0xFF68) ? wimodem.read_data() : wimodem.read_status();
                BUS_RESPOND(d);
            } else if (current_mode == MODE_WORDPAK2 && (addr == 0xFF78 || addr == 0xFF79)) {
                BUS_RESPOND(v9958.read_port(addr));
            } else if (current_mode == MODE_COCOSDC
                       && (addr == 0xFF40 || (addr >= 0xFF48 && addr <= 0xFF4B))) {
                BUS_RESPOND(cocosdc.read_register(addr));
            } else if (addr == 0xFF50) {
                BUS_RESPOND(rtc.read(addr));
            }
            // Note: if no handler matches, we do NOT call BUS_RESPOND.
            // The PIO SM stays idle (pull block) and the CoCo reads the
            // open bus value naturally from the physical data bus pull-ups.
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
