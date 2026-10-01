#include <SD.h>

static void list(File dir, int depth) {
  while (true) {
    File e = dir.openNextFile();
    if (!e) break;
    for (int i = 0; i < depth; i++) Serial.print("  ");
    Serial.print(e.name());
    if (e.isDirectory()) {
      Serial.println("/");
      list(e, depth + 1);
    } else {
      Serial.print("  ");
      Serial.println(e.size());
    }
    e.close();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  if (!SD.begin()) {
    Serial.println("SD.begin() failed");
    return;
  }
  File root = SD.open("/");
  list(root, 0);
  root.close();
}

void loop() {
}
