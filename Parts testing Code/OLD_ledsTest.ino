#include <Arduino.h>

const int LED = 4;

void setup() {
  ledcSetup(0, 5000, 8);  // PWM channel 0, 5kHz, 8-bit resolution
  ledcAttachPin(LED, 0);  // attach to GPIO4
}

void loop() {
  for (int i = 0; i <= 255; i++) {
    ledcWrite(0, i);
    delay(5);
  }
  for (int i = 255; i >= 0; i--) {
    ledcWrite(0, i);
    delay(5);
  }
}
