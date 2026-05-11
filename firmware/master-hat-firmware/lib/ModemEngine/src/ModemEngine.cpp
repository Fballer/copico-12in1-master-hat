#include "ModemEngine.h"

// External FIFOs defined in main_esp32.cpp
#define ESP_FIFO_SIZE 1024
extern uint8_t modem_rx_fifo[ESP_FIFO_SIZE];
extern volatile uint16_t modem_rx_head;
extern volatile uint16_t modem_rx_tail;

extern uint8_t modem_tx_fifo[ESP_FIFO_SIZE];
extern volatile uint16_t modem_tx_head;
extern volatile uint16_t modem_tx_tail;

ModemSerial HWSerial; // Global instance for Zimodem

int ModemSerial::available() {
    if (modem_rx_head >= modem_rx_tail) {
        return modem_rx_head - modem_rx_tail;
    } else {
        return ESP_FIFO_SIZE - modem_rx_tail + modem_rx_head;
    }
}

int ModemSerial::availableForWrite() {
    uint16_t used;
    if (modem_tx_head >= modem_tx_tail) {
        used = modem_tx_head - modem_tx_tail;
    } else {
        used = ESP_FIFO_SIZE - modem_tx_tail + modem_tx_head;
    }
    return ESP_FIFO_SIZE - 1 - used;
}

int ModemSerial::read() {
    if (modem_rx_head == modem_rx_tail) return -1;
    uint8_t val = modem_rx_fifo[modem_rx_tail];
    modem_rx_tail = (modem_rx_tail + 1) % ESP_FIFO_SIZE;
    return val;
}

int ModemSerial::peek() {
    if (modem_rx_head == modem_rx_tail) return -1;
    return modem_rx_fifo[modem_rx_tail];
}

void ModemSerial::flush() {
    // We could block here until TX fifo is empty, but for Zimodem async is better
}

size_t ModemSerial::write(uint8_t c) {
    uint16_t next_head = (modem_tx_head + 1) % ESP_FIFO_SIZE;
    if (next_head == modem_tx_tail) {
        return 0; // Buffer full
    }
    modem_tx_fifo[modem_tx_head] = c;
    modem_tx_head = next_head;
    return 1;
}

size_t ModemSerial::write(const uint8_t *buffer, size_t size) {
    size_t written = 0;
    for (size_t i = 0; i < size; i++) {
        if (write(buffer[i]) == 1) written++;
        else break;
    }
    return written;
}
