// Read GPADC channel A0, fade the PWM pin PD6 and echo what comes in on Serial.
const int pwmPin = PD6;
int level = 0, step = 5;

void setup() {
  Serial.begin(115200);
}

void loop() {
  int v = analogRead(A0);
  analogWrite(pwmPin, level);
  level += step;
  if (level <= 0 || level >= 255) step = -step;
  Serial.printf("A0=%d pwm=%d t=%lu ms\n", v, level, millis());
  while (Serial.available()) {
    Serial.print("rx: ");
    Serial.println((char)Serial.read());
  }
  delay(100);
}
