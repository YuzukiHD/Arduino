// Serial ports and their pins are chosen in the sketch: no devicetree needed.
// Serial1 (UART5) is moved from its default PE5/PE4 to explicit pins, then
// everything received on Serial1 is forwarded to Serial. Short PE4 to PE5 for a loopback.
void setup() {
  Serial.begin(115200);                       // console UART3 on PE9 (RX) / PE8 (TX)
  Serial.println("Serial1 on UART5");
  if (!Serial1.setPins(PE5, PE4)) Serial.println("pins not routable");
  Serial1.begin(9600, SERIAL_8N1);
  Serial.printf("Serial1: RX=%d TX=%d (UART%d)\n", Serial1.rxPin(), Serial1.txPin(), Serial1.uartNumber());
}

void loop() {
  Serial1.print("ping ");
  Serial1.println(millis());
  delay(500);
  while (Serial1.available()) Serial.write(Serial1.read());
}
