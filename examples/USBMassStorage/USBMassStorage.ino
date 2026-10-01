// Plug a USB flash stick into the Type-A port: prints its identity and
// capacity, reads the MBR, then lists the root directory of its FAT volume.
#include <USBHost.h>
#include <SD.h>

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(USBHost.begin() ? "USB host started" : "USB host failed");
  Serial.println("waiting for a drive...");
  while (!USBHostMSC.begin(1000)) {
    Serial.println("...");
  }
  USBHostDeviceInfo i;
  USBHostMSC.info(i);
  Serial.printf("drive %04x:%04x %s speed, %lu blocks of %lu bytes (%lu MiB)\n", i.vid, i.pid, i.speedName(),
                (unsigned long)USBHostMSC.blockCount(), (unsigned long)USBHostMSC.blockSize(),
                (unsigned long)(USBHostMSC.capacity() >> 20));

  static uint8_t sector[512];
  bool ok = USBHostMSC.readBlocks(0, sector, 1);
  Serial.printf("sector 0: %s, signature %02x%02x\n", ok ? "ok" : "read error", sector[510], sector[511]);

  if (!USBHostMSC.mount()) {
    Serial.println("no FAT volume");
    return;
  }
  Serial.printf("volume %lu MiB, free %lu MiB\n", (unsigned long)(USBDrive.totalBytes() >> 20),
                (unsigned long)(USBDrive.freeBytes() >> 20));
  File root = USBDrive.open("/");
  for (File e = root.openNextFile(); e; e = root.openNextFile()) {
    Serial.printf("  %s%s %lu\n", e.name(), e.isDirectory() ? "/" : "", (unsigned long)e.size());
    e.close();
  }
}

void loop() {
}
