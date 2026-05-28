// Minimal CoCo ROM serve test — GPIO bus loop only, no LED/Serial/EEPROM/hat code.
#include <Arduino.h>
#include "hardware/gpio.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "pico/platform.h"
#include "pico/time.h"

// Bonobo Disk BASIC ROM (proven working on your hardware in Test A).
typedef unsigned char byte;
extern byte disk11_rom[8192];
#define ROM_BYTE(abus) disk11_rom[(abus) & 0x1FFFu]

#define PIN_RW      20
#define PIN_E_CLOCK 21
#define PIN_Q       22
#define PIN_CTS     8
#define PIN_SCS     9
#define PIN_CART    27
#define PIN_SLENB   28
#define PIN_HALT    29
#define PIN_NMI     30

#ifndef CENTIPEDE_J6_DIRECT
#define CENTIPEDE_INVERT_EQ 1
#else
#define CENTIPEDE_INVERT_EQ 0
#endif

static volatile sio_hw_t* const volatile_sio = (volatile sio_hw_t*)sio_hw;
static uint8_t coco_ram[128 * 1024];

static inline bool vol_gpio_get(uint pin) {
    if (pin < 32u) {
        return (volatile_sio->gpio_in & (1u << pin)) != 0;
    }
    return (volatile_sio->gpio_hi_in & (1u << (pin - 32u))) != 0;
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

static void init_coco_bus_gpio() {
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
    gpio_init(PIN_CART);
    gpio_set_dir(PIN_CART, GPIO_OUT);
    gpio_put(PIN_CART, 1);
    init_open_collector_pin(PIN_SLENB);
    init_open_collector_pin(PIN_HALT);
    init_open_collector_pin(PIN_NMI);
}

static void __not_in_flash_func(serve_one_cycle)() {
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
            if (use_coco_ram(abus)) {
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
            while (vol_gpio_get(PIN_E_CLOCK) != CENTIPEDE_INVERT_EQ) {
                tight_loop_contents();
            }
        }
    } else if (reading) {
        if ((signals & NEG_CTS) == 0) {
            dbus = ROM_BYTE(abus);
        } else {
            dbus = coco_ram[abus];
        }
        gpio_set_dir_out_masked(0xFFu);
        gpio_put_masked(0xFFu, dbus);
        while (vol_gpio_get(PIN_E_CLOCK) != CENTIPEDE_INVERT_EQ) {
            tight_loop_contents();
        }
        gpio_set_dir_in_masked(0xFFu);
    } else {
        while (vol_gpio_get(PIN_Q) != CENTIPEDE_INVERT_EQ) {
            tight_loop_contents();
        }
        dbus = (uint8_t)(volatile_sio->gpio_in & 0xFFu);
        coco_ram[abus] = dbus;
        while (vol_gpio_get(PIN_E_CLOCK) != CENTIPEDE_INVERT_EQ) {
            tight_loop_contents();
        }
    }
}

void initVariant() {
    init_coco_bus_gpio();
}

void setup1() {
    init_coco_bus_gpio();
    save_and_disable_interrupts();
    for (;;) {
        serve_one_cycle();
    }
}

void setup() {
    tight_loop_contents();
}

void loop() {
    tight_loop_contents();
}
