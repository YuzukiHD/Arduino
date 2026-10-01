// Blink an LED on LED_BUILTIN (PA0 on the EVB variant) and print to Serial.
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.println("F101 Arduino: Blink");
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("on");
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("off");
  delay(500);
}
