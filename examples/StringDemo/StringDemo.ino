void setup() {
  Serial.begin(115200);
  String s = "Hello";
  s += ", ";
  s += String("Arduino") + " on " + "Zephyr";
  Serial.println(s);
  Serial.println(s.length());
  s.toUpperCase();
  Serial.println(s);
  Serial.println(String(3.14159, 3));
  Serial.println(String(255, HEX));
  Serial.println(s.indexOf("ZEPHYR"));
}

void loop() {
  static unsigned long n;
  Serial.println(n++);
  delay(1000);
}
