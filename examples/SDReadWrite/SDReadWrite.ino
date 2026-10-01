// Write a text file and a binary file, read them back and compare.
#include <SD.h>

static void report(const char *name, bool ok) {
  Serial.printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  if (!SD.begin()) {
    Serial.println("SD.begin() failed");
    return;
  }

  SD.mkdir("/arduino/sub");
  report("mkdir", SD.exists("/arduino/sub"));

  File f = SD.open("/arduino/hello.txt", O_WRITE | O_CREAT | O_TRUNC);
  f.println("hello from the F101");
  f.print(42);
  f.close();

  f = SD.open("/arduino/hello.txt");
  String s = f.readString();
  Serial.println(s);
  report("text readback", s.startsWith("hello from the F101") && s.endsWith("42") && f.size() == s.length());
  f.close();

  uint8_t buf[512], chk[512];
  for (int i = 0; i < 512; i++) buf[i] = i * 7;
  f = SD.open("/arduino/data.bin", O_WRITE | O_CREAT | O_TRUNC);
  for (int i = 0; i < 64; i++) f.write(buf, sizeof(buf));
  f.close();
  f = SD.open("/arduino/data.bin");
  bool ok = f.size() == 64 * 512;
  f.seek(512 * 10);
  ok = ok && f.position() == 512 * 10 && f.read(chk, 512) == 512 && memcmp(chk, buf, 512) == 0;
  f.close();
  report("binary seek/read", ok);

  report("rename", SD.rename("/arduino/data.bin", "/arduino/data2.bin") && SD.exists("/arduino/data2.bin"));
  report("remove", SD.remove("/arduino/data2.bin") && !SD.exists("/arduino/data2.bin"));
}

void loop() {
}
