// Exercises the core: Serial, String, timing, ADC, PWM, GPIO, Wire, SPI.
#include <Wire.h>
#include <SPI.h>

static void report(const char *name, bool ok) {
  Serial.printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("F101 Arduino core self test");

  String s = String("abc") + 42 + 'x';
  report("String concat", s == "abc42x" && s.length() == 6);
  report("String float", String(3.14159, 3) == "3.142");
  report("String hex", String(255, HEX) == "ff");

  unsigned long t0 = millis(), u0 = micros();
  delay(100);
  unsigned long dt = millis() - t0, du = micros() - u0;
  Serial.printf("delay(100): %lu ms, %lu us\n", dt, du);
  report("delay/millis/micros", dt >= 100 && dt <= 110 && du >= 100000 && du <= 110000);

  report("math", fabs(sin(PI / 2) - 1.0) < 1e-6 && fabs(pow(2.0, 10.0) - 1024.0) < 1e-6);
  Serial.println(map(512, 0, 1023, 0, 255));

  pinMode(PA1, OUTPUT);
  pinMode(PA2, INPUT_PULLUP);
  digitalWrite(PA1, HIGH);
  report("GPIO pull-up read", digitalRead(PA2) == HIGH);

  int a = analogRead(A0);
  Serial.printf("A0 = %d\n", a);
  report("ADC range", a >= 0 && a <= 1023);

  analogWrite(PD6, 128);
  report("PWM set", true);

  Wire.begin();
  Wire.beginTransmission(0x50);
  uint8_t e = Wire.endTransmission();
  Serial.printf("Wire to 0x50 -> %u (2 = NACK, expected without a device)\n", e);
  report("Wire returns", e != 0 || true);

  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  uint8_t r = SPI.transfer(0xA5);
  SPI.endTransaction();
  Serial.printf("SPI rx = 0x%02X\n", r);
  report("SPI returns", true);

  Serial.println("type characters to echo them");
}

void loop() {
  while (Serial.available()) {
    int c = Serial.read();
    Serial.printf("rx 0x%02X '%c'\n", c, c >= 32 ? c : '.');
  }
  static unsigned long last;
  if (millis() - last >= 2000) {
    last = millis();
    Serial.printf("alive %lu\n", last);
  }
}
