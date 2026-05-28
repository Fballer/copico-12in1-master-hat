// Minimal USB-serial test — no globals, no CoCo bus, no hat drivers.
#include <Arduino.h>

#define CENTIPEDE_LED 25

void setup() {
    pinMode(CENTIPEDE_LED, OUTPUT);
    for (int i = 0; i < 6; i++) {
        digitalWrite(CENTIPEDE_LED, HIGH);
        delay(100);
        digitalWrite(CENTIPEDE_LED, LOW);
        delay(100);
    }

    Serial.begin(115200);
    delay(1000);
    Serial.println("[SMOKE] CoPico RP2350B USB serial OK");
    Serial.flush();
}

void loop() {
    static uint32_t last = 0;
    digitalWrite(CENTIPEDE_LED, (millis() / 500) % 2);
    if (millis() - last >= 1000) {
        Serial.println("[SMOKE] tick");
        Serial.flush();
        last = millis();
    }
}

void setup1() {}

void loop1() {}
