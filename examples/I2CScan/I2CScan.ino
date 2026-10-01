#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Serial.println("I2C scan on Wire (SCL=PE0, SDA=PE1)");
}

void loop() {
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("device at 0x");
      Serial.println(addr, HEX);
      found++;
    }
  }
  Serial.println(found ? "done" : "no devices");
  delay(3000);
}
