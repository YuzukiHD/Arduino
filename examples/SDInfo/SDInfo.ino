#include <SD.h>

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("SD card info");
  if (!SD.begin()) {
    Serial.println("SD.begin() failed: no card or no FAT volume");
    return;
  }
  Serial.print("card size: ");
  Serial.print((uint32_t)(SD.cardSize() >> 20));
  Serial.println(" MiB");
  Serial.print("volume: ");
  Serial.print((uint32_t)(SD.totalBytes() >> 20));
  Serial.print(" MiB, free ");
  Serial.print((uint32_t)(SD.freeBytes() >> 20));
  Serial.println(" MiB");
}

void loop() {
}
