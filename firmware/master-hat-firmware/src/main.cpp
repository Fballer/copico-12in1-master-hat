#include <Arduino.h>
#include <EEPROM.h>
#include <I2S.h>
#include "hardware/pio.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "pico/platform.h"
#include "coco_bus.pio.h"
#include "../bios/xbios_rom.h"

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
#include "hat_config.h"

#define PIN_BTN_DUAL   19
#define PIN_STATUS_LED 17
#define CENTIPEDE_LED  25

#define I2S_BCK 10
#define I2S_DIN 11
#define I2S_LCK 12

PIO pio = pio0;
uint sm_addr, sm_data, sm_read;

uint8_t dac_left = 128;
uint8_t dac_right = 128;

I2S i2s(OUTPUT);

volatile EmulatorMode current_mode = MODE_BOOT_MENU;
HatConfig active_config;

BootMenu boot_menu;
Rtc rtc;

SerialPIO rs232_serial(13, 15, 256);

Pic7040 pic7040;
Acia6551 acia(&rs232_serial);
SpiStream spi_wimodem_stream;
Acia6551 wimodem(&spi_wimodem_stream);
V9958 v9958;
VgaDriver vga_driver(&v9958);
Cocosdc cocosdc;

uint32_t btn_press_start = 0;
bool btn_is_pressed = false;
bool btn_warning_active = false;
static bool flash_defaults_pending = true;
static bool hardware_init_done = false;
static volatile bool coco_bus_ready = false;
static volatile uint32_t rom_serve_count = 0;
static uint8_t coco_ram[128 * 1024];

static bool usb_host_connected() {
    // Avoid blocking Serial when no monitor is attached (CDC TX can stall).
    return Serial && Serial.dtr();
}

static void boot_log(const char* msg) {
    if (usb_host_connected()) {
        Serial.println(msg);
    }
}

static void stage_blink(uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        digitalWrite(CENTIPEDE_LED, HIGH);
        delay(60);
        digitalWrite(CENTIPEDE_LED, LOW);
        delay(60);
    }
    delay(200);
}

void update_orch90_audio();
void enter_boot_menu();
void apply_hat_config(const HatConfig& cfg);

#define PIN_RW      20
#define PIN_E_CLOCK 21
#define PIN_Q       22
#define PIN_CTS     8   // Centipede 32z CTS* (active LOW); shares J3 with hat VGA Green
#define PIN_SCS     9   // Centipede 32z SCS* (active LOW); shares J3 with hat VGA Blue
#define PIN_CART    27  // Centipede 32z CART* (open collector)
#define PIN_SLENB   28  // Centipede 32z SLENB* (open collector)
#define PIN_HALT    29  // Centipede 32z HALT* (open collector)
#define PIN_NMI     30  // Centipede 32z NMI* (open collector)

// J6 schmitt (pins 2-3, 4-5): E/Q inverted at GPIO. Set -DCENTIPEDE_J6_DIRECT=1 for direct E/Q.
#ifndef CENTIPEDE_J6_DIRECT
#define CENTIPEDE_INVERT_EQ 1
#else
#define CENTIPEDE_INVERT_EQ 0
#endif

static bool pio_bus_armed = false;
static bool pio_initialized = false;

static inline void stall_while_e_high() {
    while (gpio_get(PIN_E_CLOCK) == CENTIPEDE_INVERT_EQ) {
        tight_loop_contents();
    }
}

static inline void stall_while_e_low() {
    while (gpio_get(PIN_E_CLOCK) != CENTIPEDE_INVERT_EQ) {
        tight_loop_contents();
    }
}

static inline void stall_while_q_low() {
    while (gpio_get(PIN_Q) != CENTIPEDE_INVERT_EQ) {
        tight_loop_contents();
    }
}

static void disarm_pio_bus() {
    if (!pio_bus_armed) return;
    pio_sm_set_enabled(pio, sm_addr, false);
    pio_sm_set_enabled(pio, sm_data, false);
    pio_sm_set_enabled(pio, sm_read, false);
    gpio_set_dir_in_masked(0xFFu);
    pio_bus_armed = false;
}

static void rearm_pio_bus() {
    if (pio_bus_armed) return;
    pio_sm_set_enabled(pio, sm_read, true);
    pio_enable_sm_mask_in_sync(pio, (1u << sm_addr) | (1u << sm_data));
    pio_bus_armed = true;
}

static bool cart_rom_selected(uint16_t addr) {
    return (addr >= 0xC000 && addr <= 0xDFFF) || addr >= 0xFFFE;
}

static bool in_boot_menu() {
    return current_mode == MODE_BOOT_MENU;
}

static inline bool use_coco_ram(uint16_t abus) {
    return abus < 0x8000u;
}

static inline void init_open_collector_pin(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
    gpio_set_dir(pin, GPIO_IN);
    gpio_set_pulls(pin, false, false);
}

static volatile sio_hw_t* const volatile_sio = (volatile sio_hw_t*)sio_hw;

static inline bool vol_gpio_get(uint pin) {
    if (pin < 32u) {
        return (volatile_sio->gpio_in & (1u << pin)) != 0;
    }
    return (volatile_sio->gpio_hi_in & (1u << (pin - 32u))) != 0;
}

// Verbatim Bonobo centipede-watcher foreground body (Engine0 / SmallRam / DoCoco64k).
static void __not_in_flash_func(bonobo_sync_one_cycle)() {
    while (vol_gpio_get(PIN_E_CLOCK) == CENTIPEDE_INVERT_EQ) {
        tight_loop_contents();
    }

    const uint32_t signals = volatile_sio->gpio_in;
    const bool reading = (signals & (1u << PIN_RW)) != 0;
    const uint16_t abus = (uint16_t)(volatile_sio->gpio_hi_in & 0xFFFFu);
    uint8_t dbus = 0;

    constexpr uint32_t NEG_CTS = (1u << PIN_CTS);
    constexpr uint32_t NEG_SCS = (1u << PIN_SCS);
    constexpr uint32_t NEG_SELECTS = NEG_CTS | NEG_SCS;

    if ((signals & NEG_SELECTS) == NEG_SELECTS) {
        if (reading) {
            if (abus >= 0xFF00u) {
                IoReadFunc handler = io_read_table[abus & 0xFFu];
                if (handler) {
                    dbus = handler(abus);
                    gpio_set_dir_out_masked(0xFFu);
                    gpio_put_masked(0xFFu, dbus);
                }
            } else if (use_coco_ram(abus)) {
                dbus = coco_ram[abus];
                gpio_set_dir(PIN_SLENB, GPIO_OUT);
                busy_wait_at_least_cycles(12);
                gpio_set_dir_out_masked(0xFFu);
                gpio_put_masked(0xFFu, dbus);
            }

            while (vol_gpio_get(PIN_E_CLOCK) != CENTIPEDE_INVERT_EQ) {
                tight_loop_contents();
            }
            gpio_set_dir_in_masked(0xFFu);
            gpio_set_dir(PIN_SLENB, GPIO_IN);
        } else {
            while (vol_gpio_get(PIN_Q) != CENTIPEDE_INVERT_EQ) {
                tight_loop_contents();
            }
            dbus = (uint8_t)(volatile_sio->gpio_in & 0xFFu);
            coco_ram[abus] = dbus;
            if (abus >= 0xFF00u) {
                IoWriteFunc handler = io_write_table[abus & 0xFFu];
                if (handler) handler(abus, dbus);
            }
            while (vol_gpio_get(PIN_E_CLOCK) != CENTIPEDE_INVERT_EQ) {
                tight_loop_contents();
            }
        }
    } else if (reading) {
        if ((signals & NEG_CTS) == 0) {
            // Bonobo: disk11_rom[abus & 0x1FFF] — direct table, no function pointer.
            if (abus >= 0xD800u && abus <= 0xD881u && rom_read_handler) {
                dbus = rom_read_handler(abus);
            } else {
                dbus = copico_xbios_bin[abus & 0x1FFFu];
            }
        } else {
            dbus = coco_ram[abus];
            if ((signals & NEG_SCS) == 0 && abus >= 0xFF00u) {
                IoReadFunc handler = io_read_table[abus & 0xFFu];
                if (handler) dbus = handler(abus);
            }
        }

        gpio_set_dir_out_masked(0xFFu);
        gpio_put_masked(0xFFu, dbus);
        while (vol_gpio_get(PIN_E_CLOCK) != CENTIPEDE_INVERT_EQ) {
            tight_loop_contents();
        }
        gpio_set_dir_in_masked(0xFFu);
        rom_serve_count++;
    } else {
        while (vol_gpio_get(PIN_Q) != CENTIPEDE_INVERT_EQ) {
            tight_loop_contents();
        }
        dbus = (uint8_t)(volatile_sio->gpio_in & 0xFFu);
        coco_ram[abus] = dbus;

        if ((signals & NEG_SCS) == 0) {
            if (in_boot_menu() && abus >= 0xFF70u && abus <= 0xFF74u) {
                boot_menu.set_config(abus, dbus);
            } else if (in_boot_menu() && abus == 0xFF76u) {
                boot_menu.set_flash_command(dbus);
            } else if (abus == 0xFF7Fu) {
                if (in_boot_menu() && dbus == 0x55u) {
                    HatConfig cfg;
                    boot_menu.get_config(&cfg);
                    hat_config_save(cfg);
                    apply_hat_config(cfg);
                } else if (dbus == (uint8_t)MODE_BOOT_MENU) {
                    enter_boot_menu();
                }
            } else if (abus >= 0xFF00u) {
                IoWriteFunc handler = io_write_table[abus & 0xFFu];
                if (handler) handler(abus, dbus);
            }
        }

        while (vol_gpio_get(PIN_E_CLOCK) != CENTIPEDE_INVERT_EQ) {
            tight_loop_contents();
        }
    }
}

static void init_centipede_control_pins() {
    // Bonobo centipede-watcher: release open-collector cart control lines.
    gpio_init(PIN_CART);
    gpio_set_dir(PIN_CART, GPIO_OUT);
    gpio_put(PIN_CART, 1);
    init_open_collector_pin(PIN_SLENB);
    init_open_collector_pin(PIN_HALT);
    init_open_collector_pin(PIN_NMI);
}

static void init_coco_bus_gpio() {
    // Bonobo InitializePins: tri-state the entire cart-side GPIO bank first.
    for (uint i = 0; i <= 22; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_set_pulls(i, false, false);
    }
    for (uint i = 32; i <= 47; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
        gpio_set_pulls(i, false, false);
    }
    init_centipede_control_pins();
}

static void init_hat_gpio() {
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);
    pinMode(PIN_BTN_DUAL, INPUT_PULLUP);

    // Centipede 32z cartridge selects — must stay inputs in X-BIOS mode (not VGA outputs).
    gpio_init(PIN_CTS);
    gpio_init(PIN_SCS);
    gpio_set_dir(PIN_CTS, GPIO_IN);
    gpio_set_dir(PIN_SCS, GPIO_IN);
    gpio_set_pulls(PIN_CTS, false, false);
    gpio_set_pulls(PIN_SCS, false, false);
}

void enter_boot_menu() {
    if (current_mode != MODE_BOOT_MENU) {
        boot_log("[Boot] Entering CoPico X-BIOS menu");
        esp32.send_command(CMD_RESET, nullptr, 0);
    }
    current_mode = MODE_BOOT_MENU;
    rebuild_io_tables_boot_menu();
    boot_menu.init();
    boot_log(rom_read_handler ? "[Boot] X-BIOS ROM handler armed"
                                : "[Boot] ERROR: ROM handler missing");
}

static void init_coco_bus_pio() {
    if (pio_initialized) return;
    init_coco_bus_gpio();

    uint offset_addr = pio_add_program(pio, &coco_sniffer_addr_program);
    uint offset_data = pio_add_program(pio, &coco_sniffer_data_program);
    uint offset_read = pio_add_program(pio, &coco_bus_read_program);

    sm_addr = pio_claim_unused_sm(pio, true);
    sm_data = pio_claim_unused_sm(pio, true);
    sm_read = pio_claim_unused_sm(pio, true);

    pio_sm_config c_addr = coco_sniffer_addr_program_get_default_config(offset_addr);
    sm_config_set_in_pins(&c_addr, 32);
    pio_sm_init(pio, sm_addr, offset_addr, &c_addr);

    pio_sm_config c_data = coco_sniffer_data_program_get_default_config(offset_data);
    sm_config_set_in_pins(&c_data, 0);
    pio_sm_init(pio, sm_data, offset_data, &c_data);

    pio_sm_config c_read = coco_bus_read_program_get_default_config(offset_read);
    sm_config_set_out_pins(&c_read, 0, 8);
    sm_config_set_set_pins(&c_read, 0, 8);
    sm_config_set_out_shift(&c_read, true, false, 8);
    sm_config_set_clkdiv(&c_read, 1.0f);
    for (int i = 0; i < 8; i++) pio_gpio_init(pio, i);
    for (int i = 32; i < 48; i++) pio_gpio_init(pio, i);
    pio_sm_set_consecutive_pindirs(pio, sm_read, 0, 8, false);
    pio_sm_init(pio, sm_read, offset_read, &c_read);
    pio_sm_set_enabled(pio, sm_read, true);

    pio_enable_sm_mask_in_sync(pio, (1u << sm_addr) | (1u << sm_data));
    pio_initialized = true;
    pio_bus_armed = true;
}

void apply_hat_config(const HatConfig& cfg) {
    HatConfig c = cfg;
    hat_config_apply_constraints(c);

    if (current_mode != MODE_BOOT_MENU) {
        esp32.send_command(CMD_RESET, nullptr, 0);
    }

    active_config = c;
    current_mode = MODE_HAT_ACTIVE;

    rebuild_io_tables_config(c);
    hat_config_log(c, active_layers);

    if (active_layers.internal_rom) {
        Serial.println("[Config] Internal ROM — hat tri-stated");
        return;
    }

    i2s.setBCLK(I2S_BCK);
    i2s.setDATA(I2S_DIN);
    i2s.setBitsPerSample(16);
    i2s.begin(44100);
    spi_wimodem_stream.begin();
    rtc.init();

    if (active_layers.cocosdc) cocosdc.init();
    if (active_layers.wordpak) vga_driver.init();
    if (active_layers.rs232) acia.init(false);
    if (active_layers.wimodem) wimodem.init(true);
    if (active_layers.fujinet) {
        if (!flash_rom.load_rom_to_buffer(SLOT_FUJINET,
                                          io_dispatch_fujinet_rom_buffer(),
                                          nullptr, nullptr)) {
            Serial.println("[Config] FujiNet ROM load failed");
        }
    }
}

static void init_hat_hardware() {
    init_hat_gpio();
    Serial.begin(115200);
    boot_log("[Boot] CoPico RP2350 starting (RP2350B / Centipede 32z)");
    boot_log("[Boot] init: EEPROM");
    EEPROM.begin(512);
    stage_blink(5);
    boot_log("[Boot] X-BIOS ready — slow blink = alive");
    hardware_init_done = true;
}

// Earliest hook Arduino calls on core0 — tri-state cart GPIO before USB/Serial init.
void initVariant() {
    init_coco_bus_gpio();
}

void setup() {
    // CoCo bus is already live on core1 (setup1). Core0 only handles UI / hat init.
    pinMode(CENTIPEDE_LED, OUTPUT);
    digitalWrite(CENTIPEDE_LED, LOW);

#ifdef PICO_DEFAULT_LED_PIN
    pinMode(PICO_DEFAULT_LED_PIN, OUTPUT);
#endif
}

void loop() {
    if (!hardware_init_done) {
        init_hat_hardware();
        stage_blink(6);
        return;
    }

    if (flash_defaults_pending) {
        // X-BIOS serves from embedded ROM; skip heavy flash writes at boot.
        if (flash_rom.is_slot_empty(SLOT_COCOSDC)) {
            flash_rom.status_flags |= FLASH_STATUS_SDC_MISSING;
        }
        flash_defaults_pending = false;
    }

    if (digitalRead(PIN_BTN_DUAL) == LOW) {
        if (!btn_is_pressed) {
            btn_is_pressed = true;
            btn_press_start = millis();
            btn_warning_active = false;
        } else {
            uint32_t hold_time = millis() - btn_press_start;

            if (hold_time >= 5000) {
                digitalWrite(PIN_STATUS_LED, LOW);
                btn_is_pressed = false;
                Serial.println("System Reset Triggered -> Returning to Boot Menu");
                enter_boot_menu();
            } else if (hold_time >= 3000) {
                digitalWrite(PIN_STATUS_LED, (millis() / 100) % 2);
                btn_warning_active = true;
            }
        }
    } else {
        if (btn_is_pressed) {
            uint32_t hold_time = millis() - btn_press_start;
            btn_is_pressed = false;

            if (btn_warning_active) {
                digitalWrite(PIN_STATUS_LED, LOW);
                btn_warning_active = false;
            } else if (hold_time < 1000 && active_layers.cocosdc) {
                Serial.println("Disk Swap Triggered");
                esp32.sdc_swap();
            }
        }
    }

    if (in_boot_menu()) {
        boot_menu.service_flash_command();
        HatConfig pending;
        if (boot_menu.consume_pending_config_apply(&pending)) {
            hat_config_save(pending);
            apply_hat_config(pending);
        }

        digitalWrite(PIN_STATUS_LED, (millis() / 500) % 2);
#ifdef PICO_DEFAULT_LED_PIN
        digitalWrite(PICO_DEFAULT_LED_PIN, (millis() / 500) % 2);
#endif
        digitalWrite(CENTIPEDE_LED, (millis() / 500) % 2);
        delay(10);
        return;
    }

    if (active_layers.speech) {
        pic7040.tick();
        int16_t sample_l = 0, sample_r = 0;
        pic7040.generate_audio(&sample_l, &sample_r);
        i2s.write(sample_l);
        i2s.write(sample_r);
    }

    if (active_layers.wordpak) {
        vga_driver.tick();
    }

    if (active_layers.cocosdc) {
        cocosdc.tick();
    }

    if (active_layers.wimodem) {
        spi_wimodem_stream.tick();
    }

    if (!active_layers.speech && !active_layers.wordpak &&
        !active_layers.cocosdc && !active_layers.wimodem) {
        delay(1);
    }
}

void setup1() {
    // First instruction on core1: serve the CoCo bus (do not wait for core0 setup()).
    init_coco_bus_gpio();
    enter_boot_menu();
    coco_bus_ready = true;
    multicore_lockout_victim_init();

    uint32_t ints = save_and_disable_interrupts();
    while (in_boot_menu()) {
        bonobo_sync_one_cycle();
    }
    restore_interrupts(ints);
}

#define BUS_RESPOND(byte) pio_sm_put_blocking(pio, sm_read, (uint32_t)(byte))

void loop1() {
    if (in_boot_menu()) return;

    if (!pio_initialized) {
        init_coco_bus_pio();
    }
    rearm_pio_bus();

    if (pio_sm_is_rx_fifo_empty(pio, sm_addr)) return;

    uint32_t addr = pio_sm_get(pio, sm_addr);
    bool is_read = gpio_get(PIN_RW);

    if (is_read) {
        if (!pio_sm_is_rx_fifo_empty(pio, sm_data)) {
            (void)pio_sm_get(pio, sm_data);
        }
    } else {
        if (pio_sm_is_rx_fifo_empty(pio, sm_data)) return;
    }

    uint32_t data = is_read ? 0 : pio_sm_get(pio, sm_data);
    bool cts_active = (gpio_get(PIN_CTS) == 0);
    bool scs_active = (gpio_get(PIN_SCS) == 0);

    if (is_read) {
        if (cart_rom_selected((uint16_t)addr) && rom_read_handler && cts_active) {
            BUS_RESPOND(rom_read_handler((uint16_t)addr));
            rom_serve_count++;
            return;
        }

        if (addr >= 0xFF00) {
            if (!scs_active) return;
            IoReadFunc handler = io_read_table[addr & 0xFF];
            if (handler) {
                BUS_RESPOND(handler(addr));
            }
        }
    } else {
        if (in_boot_menu() && scs_active && addr >= 0xFF70 && addr <= 0xFF74) {
            boot_menu.set_config(addr, (uint8_t)data);
            return;
        }

        if (in_boot_menu() && scs_active && addr == 0xFF76) {
            boot_menu.set_flash_command((uint8_t)data);
            return;
        }

        if (scs_active && addr == 0xFF7F) {
            if (in_boot_menu() && data == 0x55) {
                HatConfig cfg;
                boot_menu.get_config(&cfg);
                hat_config_save(cfg);
                apply_hat_config(cfg);
            } else if (data == (uint8_t)MODE_BOOT_MENU) {
                enter_boot_menu();
            }
            return;
        }

        if (addr >= 0xFF00) {
            if (!scs_active) return;
            IoWriteFunc handler = io_write_table[addr & 0xFF];
            if (handler) {
                handler(addr, (uint8_t)data);
            }
        }
    }
}

void update_orch90_audio() {
    int16_t sample_l = ((int16_t)dac_left - 128) << 8;
    int16_t sample_r = ((int16_t)dac_right - 128) << 8;
    i2s.write(sample_l);
    i2s.write(sample_r);
}
