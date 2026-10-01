// A USB CDC serial port on the OTG connector: open it on the PC and type,
// everything is echoed back (and logged on the UART console).
#include <USBDevice.h>

void setup() {
  Serial.begin(115200);
  USBSerial.setStrings("F101 Arduino", "F101 USB Serial", "F101-0001");
  Serial.println(USBSerial.begin() ? "USB device started" : "USB device failed");
}

void loop() {
  static bool was = false;
  if (USBSerial.connected() != was) {
    was = USBSerial.connected();
    Serial.println(was ? "PC opened the port" : "PC closed the port");
    if (was) USBSerial.println("hello from the F101");
  }
  while (USBSerial.available()) {
    int c = USBSerial.read();
    USBSerial.write(c);
    Serial.printf("usb rx 0x%02X\n", c);
  }
}
