/*
 * Counts the events of the GPIO interrupts: pin 2 on CHANGE, pin 3 on RISING,
 * pin 4 on FALLING, and pin 5 attached then detached before any event.
 */

volatile int change2 = 0;
volatile int rising3 = 0;
volatile int falling4 = 0;
volatile int detached5 = 0;

void onChange2() {
	change2++;
}

void onRising3() {
	rising3++;
}

void onFalling4() {
	falling4++;
}

void onDetached5() {
	detached5++;
}

void setup() {
	Serial.begin(115200);

	pinMode(2, INPUT);
	pinMode(3, INPUT);
	pinMode(4, INPUT);
	pinMode(5, INPUT);

	attachInterrupt(digitalPinToInterrupt(2), onChange2, CHANGE);
	attachInterrupt(digitalPinToInterrupt(3), onRising3, RISING);
	attachInterrupt(digitalPinToInterrupt(4), onFalling4, FALLING);
	attachInterrupt(digitalPinToInterrupt(5), onDetached5, CHANGE);
	detachInterrupt(digitalPinToInterrupt(5));
}

void loop() {
	noInterrupts();
	int a = change2;
	int b = rising3;
	int c = falling4;
	int d = detached5;
	interrupts();

	Serial.print(a);
	Serial.print(" ");
	Serial.print(b);
	Serial.print(" ");
	Serial.print(c);
	Serial.print(" ");
	Serial.println(d);
	delay(200);
}
