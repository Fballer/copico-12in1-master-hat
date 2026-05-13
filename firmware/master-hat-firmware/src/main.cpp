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
#include "io_dispatch.h"
#include "flash_rom_manager.h"

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

// PIN_RW and PIN_E_CLOCK are on the CoCo bus headers (directly read by PIO/GPIO)
#define PIN_RW      20  // Active LOW = write cycle
#define PIN_E_CLOCK 21  // E-clock from the CoCo

void setup() {
    Serial.begin(115200);
#ifdef PICO_DEFAULT_LED_PIN
    pinMode(PICO_DEFAULT_LED_PIN, OUTPUT);
#endif

    pinMode(PIN_BTN_DUAL, INPUT_PULLUP);
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);
    delay(50); // Debounce settle
    
    // ================================================================
    // Flash ROM Bank Init — Must happen before mode selection.
    // Installs factory ROMs (Chameleon, FujiNet, RS-232, Disk BASIC)
    // into flash on first boot. SDC-DOS is handled separately via SD.
    // ================================================================
    flash_rom.init_factory_defaults();

    EEPROM.begin(512);
    uint8_t saved_mode = EEPROM.read(0);
    if (saved_mode >= MODE_MAX) saved_mode = (uint8_t)MODE_BOOT_MENU;

    // G19 held at power-on → force CoPico X-BIOS (Slot 0)
    // This is the user's "panic button" to recover from any misconfiguration.
    if (digitalRead(PIN_BTN_DUAL) == LOW) {
        current_mode = MODE_BOOT_MENU;
        Serial.println("[Boot] G19 held → forcing CoPico X-BIOS Menu");
    } else if (flash_rom.status_flags & FLASH_STATUS_SDC_MISSING &&
               saved_mode == MODE_COCOSDC) {
        // SDC-DOS is not in flash and the user wants CoCoSDC mode.
        // Boot to the CoPico X-BIOS Menu so it can display the setup message.
        current_mode = MODE_BOOT_MENU;
        Serial.println("[Boot] SDC-DOS missing → forcing CoPico X-BIOS for setup alert");
    } else {
        current_mode = (EmulatorMode)saved_mode;
        Serial.print("[Boot] Restored mode: ");
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
// ---------------------------------------------------------------
#define BUS_RESPOND(byte) pio_sm_put_blocking(pio, sm_read, (uint32_t)(byte))

// ================================================================
// loop1(): Core 1 — Hybrid Fast-Map Bus Handler
// ================================================================
// TIMING CONTEXT (why this code is structured the way it is):
//
// The CoCo's 6809/6309 CPU reads data from the bus on the
// falling edge of the E-clock. We must drive valid data onto
// GPIO 0-7 BEFORE that edge arrives. Our timing budget:
//
//   Stock CoCo (1.0 MHz):  ~500 ns  — easy
//   Turbo       (1.79 MHz): ~279 ns  — comfortable
//   GIME-X      (2.86 MHz): ~175 ns  — our design target
//
// ARCHITECTURE:
//   1. ROM Shadow ($C000-$DFFF): Checked via a single function
//      pointer (rom_read_handler). One comparison + indirect call.
//      Cost: ~20 ns.
//
//   2. I/O Space ($FF00-$FFFF): Indexed lookup into a 256-entry
//      function pointer table (io_read_table / io_write_table).
//      Eliminates the old if/else chain entirely.
//      Cost: ~7 ns (array index + null check + indirect call).
//
//   3. Special writes ($FF70-$FF73 boot config, $FF7F mode switch):
//      Handled inline before the table lookup because they trigger
//      system-level actions (EEPROM write, mode switch) that must
//      not be deferred to a handler function.
//
// ESTIMATED TOTAL LATENCY: ~120 ns worst-case
// MARGIN AT 2.86 MHz:      ~55 ns (comfortable)
// ================================================================

void loop1() {
    // Tight poll: bail immediately if no bus cycle to process
    if (pio_sm_is_rx_fifo_empty(pio, sm_addr) || pio_sm_is_rx_fifo_empty(pio, sm_data)) return;

    uint32_t addr = pio_sm_get(pio, sm_addr);
    uint32_t data = pio_sm_get(pio, sm_data);
    bool is_read = gpio_get(PIN_RW);

    if (is_read) {
        // --- READ CYCLE (fast path) ---

        // Path A: ROM shadow ($C000-$DFFF) — single pointer check
        if (addr >= 0xC000 && addr <= 0xDFFF) {
            if (rom_read_handler) {
                BUS_RESPOND(rom_read_handler(addr));
            }
            return;
        }

        // Path B: I/O space ($FF00-$FFFF) — table lookup
        if (addr >= 0xFF00) {
            IoReadFunc handler = io_read_table[addr & 0xFF];
            if (handler) {
                BUS_RESPOND(handler(addr));
            }
            // If handler is NULL, no device here — open bus.
        }
    } else {
        // --- WRITE CYCLE ---

        // Special: Boot Menu config registers (system-level, inline)
        if (current_mode == MODE_BOOT_MENU && addr >= 0xFF70 && addr <= 0xFF73) {
            boot_menu.set_config(addr, (uint8_t)data);
            return;
        }

        // Special: Mode switch register (system-level, inline)
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
        }

        // Normal I/O write — table lookup
        if (addr >= 0xFF00) {
            IoWriteFunc handler = io_write_table[addr & 0xFF];
            if (handler) {
                handler(addr, (uint8_t)data);
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
    
    // Rebuild the I/O dispatch tables for the new mode.
    // This remaps the 256-entry function pointer tables so loop1()
    // routes bus reads/writes to the correct emulator handlers.
    rebuild_io_tables(new_mode);
    
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
        case MODE_FUJINET:
            // Load FujiNet BIOS (Slot 2) from flash into shadow RAM.
            // The io_dispatch read handler will serve it to the CoCo bus.
            flash_rom.load_rom_to_buffer(SLOT_FUJINET,
                                         io_dispatch_get_shadow_buffer(),
                                         nullptr, nullptr);
            break;
        case MODE_WIMODEM:
            wimodem.init(true); // Always turbo mode for wimodem
            break;
        case MODE_INTERNAL_ROM:
            // Hat goes fully silent — all data bus pins go tri-state.
            // The CoCo boots using its own internal Color BASIC ROMs.
            // io_dispatch handles this by simply not asserting /OE.
            Serial.println("[Mode] Internal ROM — Hat tri-stated");
            break;
        case MODE_ORCH90:
        case MODE_SPEECH_SOUND:
            // No specific init required
            break;
    }
}
