#ifdef PWM_DIAG
#include <Arduino.h>

const int motorPins[4] = {D1, D2, D5, D4}; // M1, M2, M3 (moved to D5), M4

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("PWM diagnostic starting");
  for (int i = 0; i < 4; ++i) pinMode(motorPins[i], OUTPUT);
  delay(500);
}

void loop() {
  for (int i = 0; i < 4; ++i) {
    Serial.printf("Testing motor %d on pin %d\n", i + 1, motorPins[i]);
    for (int v = 0; v <= 255; v += 51) {
      analogWrite(motorPins[i], v);
      Serial.printf("  PWM %d\n", v);
      delay(500);
    }
    analogWrite(motorPins[i], 0);
    delay(500);
  }

  Serial.println("Holding 200 on all pins for observation");
  for (int i = 0; i < 4; ++i) analogWrite(motorPins[i], 200);
  while (true) delay(1000);
}
#endif
