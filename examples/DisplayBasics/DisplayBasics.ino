#include <Display.h>

void setup() {
  Serial.begin(115200);
  if (!Display.begin()) {
    Serial.println("display not ready");
    return;
  }
  Serial.printf("display %dx%d\n", Display.width(), Display.height());

  Display.fillScreen(Display.color(0, 0, 64));
  Display.fillRect(20, 20, 200, 120, Display.color(255, 0, 0));
  Display.fillCircle(400, 100, 60, Display.color(0, 255, 0));
  Display.drawLine(0, 0, Display.width() - 1, Display.height() - 1, Display.color(255, 255, 255));
  Display.drawRect(5, 5, Display.width() - 10, Display.height() - 10, Display.color(255, 255, 0));
  Display.fillTriangle(600, 40, 700, 160, 520, 160, Display.color(255, 128, 0));
  Display.setTextSize(3);
  Display.setTextColor(Display.color(255, 255, 255));
  Display.setCursor(20, 200);
  Display.println("F101 Arduino Display");
  Display.setTextSize(2);
  Display.printf("%dx%d  millis=%lu\n", Display.width(), Display.height(), millis());
  Display.show();
  Serial.println("drawn");
}

void loop() {
  static int level = 255, step = -5;
  Display.setBrightness(level);
  level += step;
  if (level <= 20 || level >= 255) step = -step;
  delay(30);
}
