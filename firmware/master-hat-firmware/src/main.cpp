#include <Arduino.h>
#include <I2S.h>
#include "hardware/pio.h"
#include "coco_bus.pio.h"

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

// Audio state
uint8_t dac_left = 128;
uint8_t dac_right = 128;

// Initialize I2S instance for audio output
I2S i2s(OUTPUT);

// Function prototype
void update_orch90_audio();

void setup() {
    Serial.begin(115200);
    
    // 1. Initialize I2S DAC (PCM5102A)
    i2s.setBCLK(I2S_BCK);
    i2s.setDATA(I2S_DIN);
    i2s.setBitsPerSample(16); // Orch-90 is 8-bit, but I2S usually wants 16 or 32-bit frames
    i2s.begin(44100);

    // 2. Load PIO Programs
    uint offset_addr = pio_add_program(pio, &coco_sniffer_addr_program);
    uint offset_data = pio_add_program(pio, &coco_sniffer_data_program);

    sm_addr = pio_claim_unused_sm(pio, true);
    sm_data = pio_claim_unused_sm(pio, true);

    // 3. Configure SM_ADDR
    pio_sm_config c_addr = coco_sniffer_addr_program_get_default_config(offset_addr);
    sm_config_set_in_pins(&c_addr, 32); // A0 is GPIO 32
    sm_config_set_jmp_pin(&c_addr, PIN_RW);
    pio_sm_init(pio, sm_addr, offset_addr, &c_addr);

    // 4. Configure SM_DATA
    pio_sm_config c_data = coco_sniffer_data_program_get_default_config(offset_data);
    sm_config_set_in_pins(&c_data, 0);  // D0 is GPIO 0
    sm_config_set_jmp_pin(&c_data, PIN_RW);
    pio_sm_init(pio, sm_data, offset_data, &c_data);

    // 5. Start both state machines synchronously
    pio_enable_sm_mask_in_sync(pio, (1u << sm_addr) | (1u << sm_data));
}

void loop() {
    // Core 0 handles system tasks / debugging
    delay(10);
}

// Core 1 loop - Dedicated to processing the CoCo bus
void setup1() {
    // Core 1 setup (if any needed in the future)
}

void loop1() {
    // Poll the FIFOs tightly
    if (!pio_sm_is_rx_fifo_empty(pio, sm_addr) && !pio_sm_is_rx_fifo_empty(pio, sm_data)) {
        uint32_t addr = pio_sm_get(pio, sm_addr);
        uint32_t data = pio_sm_get(pio, sm_data);

        // UNIVERSAL BUS MANAGER: Route to the correct module
        switch (addr) {
            // --- Phase 1: Orchestra-90 ---
            case 0xFF7A: // Left Channel DAC
                dac_left = (uint8_t)data;
                update_orch90_audio();
                break;
            case 0xFF7B: // Right Channel DAC
                dac_right = (uint8_t)data;
                update_orch90_audio();
                break;
                
            // --- Future Modules (Examples) ---
            // case 0xFF68: acia_tx(data); break;
            // case 0xFF40: cocosdc_write(data); break;
        }
    }
}

void update_orch90_audio() {
    // Convert 8-bit unsigned/signed to 16-bit I2S frame
    int16_t sample_l = ((int16_t)dac_left - 128) << 8;
    int16_t sample_r = ((int16_t)dac_right - 128) << 8;
    
    // Push to I2S DAC (this is non-blocking if using DMA behind the scenes)
    i2s.write(sample_l);
    i2s.write(sample_r);
}
