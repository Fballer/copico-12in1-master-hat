#pragma once
#include <Arduino.h>

// A fake HardwareSerial that bridges Zimodem to our SPI FIFOs
class ModemSerial : public Stream {
public:
    ModemSerial() {}

    // Dummy methods to satisfy Zimodem's HardwareSerial expectations
    void begin(unsigned long baud, uint32_t config=SERIAL_8N1, int8_t rxPin=-1, int8_t txPin=-1, bool invert=false, unsigned long timeout_ms = 20000UL) {}
    void end() {}
    void setRxBufferSize(size_t) {}
    void setDebugOutput(bool) {}
    uint32_t baudRate() { return 115200; }
    void updateBaudRate(uint32_t) {}
    void setAutoBaud(bool) {}
    
    // Stream implementation linked to FIFOs in main_esp32.cpp
    int available() override;
    int availableForWrite();
    int read() override;
    int peek() override;
    void flush() override;
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
};
