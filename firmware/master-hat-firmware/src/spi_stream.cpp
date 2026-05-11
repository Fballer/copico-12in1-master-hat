#include "spi_stream.h"

// Hardware SPI pins for Coprocessor
#define PIN_SPI_MISO 23
#define PIN_SPI_MOSI 24
#define PIN_SPI_CS   26
#define PIN_SPI_SCK  28

SpiStream::SpiStream() {
    memset(&_tx_packet, 0, sizeof(_tx_packet));
    memset(&_rx_packet, 0, sizeof(_rx_packet));
}

void SpiStream::begin() {
    SPI.setRX(PIN_SPI_MISO);
    SPI.setTX(PIN_SPI_MOSI);
    SPI.setSCK(PIN_SPI_SCK);
    SPI.begin();
    
    pinMode(PIN_SPI_CS, OUTPUT);
    digitalWrite(PIN_SPI_CS, HIGH);
    
    // ESP32 SPI Slave max reliable speed is typically 10MHz or less
    SPI.beginTransaction(SPISettings(5000000, MSBFIRST, SPI_MODE0));
}

void SpiStream::tick() {
    uint32_t now = millis();
    if (now - _last_poll_time < POLL_INTERVAL_MS) {
        return;
    }
    _last_poll_time = now;

    // Prepare TX Packet
    _tx_packet.sync = SPI_SYNC_BYTE;
    _tx_packet.command = CMD_POLL;
    
    uint8_t tx_len = 0;
    while (tx_len < SPI_PAYLOAD_SIZE && _tx_head != _tx_tail) {
        _tx_packet.payload[tx_len++] = _tx_buffer[_tx_tail];
        _tx_tail = (_tx_tail + 1) % SPI_STREAM_BUFFER_SIZE;
    }
    _tx_packet.length = tx_len;
    
    if (tx_len > 0) {
        _tx_packet.command = CMD_TX_DATA;
    }
    
    _tx_packet.checksum = calc_checksum((const uint8_t*)&_tx_packet, sizeof(_tx_packet) - 1);

    // Perform full-duplex transfer
    digitalWrite(PIN_SPI_CS, LOW);
    SPI.transfer((uint8_t*)&_tx_packet, (uint8_t*)&_rx_packet, sizeof(SpiMasterPacket));
    digitalWrite(PIN_SPI_CS, HIGH);

    // Parse RX Packet
    if (_rx_packet.sync == SPI_SYNC_BYTE) {
        uint8_t expected_checksum = calc_checksum((const uint8_t*)&_rx_packet, sizeof(_rx_packet) - 1);
        if (expected_checksum == _rx_packet.checksum) {
            if (_rx_packet.status == STATUS_HAS_DATA && _rx_packet.length <= SPI_PAYLOAD_SIZE) {
                for (uint8_t i = 0; i < _rx_packet.length; i++) {
                    uint16_t next_head = (_rx_head + 1) % SPI_STREAM_BUFFER_SIZE;
                    if (next_head != _rx_tail) {
                        _rx_buffer[_rx_head] = _rx_packet.payload[i];
                        _rx_head = next_head;
                    }
                }
            }
        }
    }
}

int SpiStream::available() {
    if (_rx_head >= _rx_tail) {
        return _rx_head - _rx_tail;
    } else {
        return SPI_STREAM_BUFFER_SIZE - _rx_tail + _rx_head;
    }
}

int SpiStream::read() {
    if (_rx_head == _rx_tail) return -1;
    uint8_t val = _rx_buffer[_rx_tail];
    _rx_tail = (_rx_tail + 1) % SPI_STREAM_BUFFER_SIZE;
    return val;
}

int SpiStream::peek() {
    if (_rx_head == _rx_tail) return -1;
    return _rx_buffer[_rx_tail];
}

void SpiStream::flush() {
    // Optional: wait until tx buffer is empty
}

size_t SpiStream::write(uint8_t c) {
    uint16_t next_head = (_tx_head + 1) % SPI_STREAM_BUFFER_SIZE;
    if (next_head == _tx_tail) {
        return 0; // Buffer full
    }
    _tx_buffer[_tx_head] = c;
    _tx_head = next_head;
    return 1;
}

size_t SpiStream::write(const uint8_t *buffer, size_t size) {
    size_t written = 0;
    for (size_t i = 0; i < size; i++) {
        if (write(buffer[i]) == 1) written++;
        else break;
    }
    return written;
}
