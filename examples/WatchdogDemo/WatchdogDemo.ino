// Feeds the watchdog for 10 s, then stops feeding it: the board resets after the timeout
// (the boot messages appear again on the console).
#include <Watchdog.h>

unsigned long started;

void setup() {
  Serial.begin(115200);
  Serial.println("Watchdog demo, boot");
  Serial.printf("begin(3000): %d, period %lu ms\n", Watchdog.begin(3000),
                (unsigned long)Watchdog.timeoutMs());
  started = millis();
}

void loop() {
  if (millis() - started < 10000) {
    Watchdog.reset();
    Serial.printf("fed at %lu ms\n", millis() - started);
  } else {
    Serial.println("not feeding any more, reset in <= 3 s");
  }
  delay(1000);
}
