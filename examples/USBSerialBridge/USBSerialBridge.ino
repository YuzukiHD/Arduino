// A CDC ACM device or a USB serial adapter (CH340/CH343) on the Type-A port is
// bridged to the console: what you type on Serial goes to it and back.
#include <USBHost.h>

void setup() {
  Serial.begin(115200);
  delay(200);
  USBHost.begin();
  Serial.println("plug in a USB serial device");
}

void loop() {
  static bool open = false;
  if (!open) {
    if (USBHostSerial.present() && USBHostSerial.begin(115200)) {
      USBHostDeviceInfo i;
      USBHostSerial.info(i);
      Serial.printf("serial device %04x:%04x open\n", i.vid, i.pid);
      USBHostSerial.println("hello from the F101");
      open = true;
    }
    delay(200);
    return;
  }
  if (!USBHostSerial.present()) {
    Serial.println("device gone");
    USBHostSerial.end();
    open = false;
    return;
  }
  while (Serial.available()) USBHostSerial.write(Serial.read());
  while (USBHostSerial.available()) Serial.write(USBHostSerial.read());
}
