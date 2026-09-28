/*
  PDM microphone level meter - quick smoke test.

  Prints the peak absolute sample value of every captured block as a text
  bar on the serial console. Tap or speak near the microphone and the bar
  should jump.

  Circuit:
  - Arduino Nano 33 BLE Sense or Arduino Nano RP2040 Connect board

  No host side tool is needed, just open the serial monitor at 115200.

  This example code is in the public domain.
*/

#include <PDM.h>

// default number of output channels
static const char channels = 1;
// default PCM output frequency
static const int frequency = 16000;
// Buffer to read samples into. For best performance, match its size to the
// buffer used internally by the PDM library (PDM_NUMBER_OF_SAMPLES).
short sampleBuffer[PDM_NUMBER_OF_SAMPLES];

void setup() {
	Serial.begin(115200);
	while (!Serial)
		;

	if (!PDM.begin(channels, frequency)) {
		Serial.println("Failed to start PDM!");
		while (1)
			;
	}
}

void loop() {
	int bytesAvailable = PDM.available();
	if (bytesAvailable <= 0) {
		return;
	}

	if (bytesAvailable > (int)sizeof(sampleBuffer)) {
		bytesAvailable = sizeof(sampleBuffer);
	}

	int bytesRead = PDM.read(sampleBuffer, bytesAvailable);
	int samples = bytesRead / 2;

	int peak = 0;
	for (int i = 0; i < samples; i++) {
		int value = sampleBuffer[i];
		if (value < 0) {
			value = -value;
		}
		if (value > peak) {
			peak = value;
		}
	}

	// 50 cell bar, full scale is 32767
	int bar = peak / 655;
	if (bar > 50) {
		bar = 50;
	}

	Serial.print('[');
	for (int i = 0; i < 50; i++) {
		Serial.print(i < bar ? '#' : ' ');
	}
	Serial.print("] ");
	Serial.println(peak);
}
