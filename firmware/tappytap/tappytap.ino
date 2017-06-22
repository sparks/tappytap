// inslude the SPI library:
#include <SPI.h>

typedef struct {
	uint8_t en;
	uint8_t dir;
} state_t;

void set(state_t*, uint8_t, uint8_t, bool, bool);
void write(const state_t*, uint8_t);
void copyTriple(uint8_t*, uint8_t, uint8_t);

void displayByte(state_t*, uint8_t, uint8_t);

#define SS_PIN 10
#define NCV_EN_PIN 9

state_t states[3] = {
	{0, 0},
	{0, 0},
	{0, 0},
};

uint8_t daisy_counter = -1;

void setup() {
	pinMode(SS_PIN, OUTPUT);
	pinMode(NCV_EN_PIN, OUTPUT);

	digitalWrite(NCV_EN_PIN, HIGH);
	digitalWrite(SS_PIN, HIGH);

	SPI.begin();
	SPI.setDataMode(SPI_MODE1);

	Serial.begin(38400);
}

void copyTriple(uint8_t* dest, uint8_t source, uint8_t source_offset) {
	*dest = (source & (0b111 << source_offset)) >> source_offset;
}

void loop() {
	if (Serial.available() > 0) {
		uint8_t incomingByte = Serial.read();

		if ((incomingByte & 0x80) != 0) daisy_counter = 0;

		if (daisy_counter < 0) return;

		if (daisy_counter == 0) {
			copyTriple(&states[0].en, incomingByte, 0);
			copyTriple(&states[1].en, incomingByte, 3);
		} else if (daisy_counter == 1) {
			copyTriple(&states[2].en, incomingByte, 0);
			copyTriple(&states[0].dir, incomingByte, 3);
		} else if(daisy_counter == 2) {
			copyTriple(&states[1].dir, incomingByte, 0);
			copyTriple(&states[2].dir, incomingByte, 3);

			write(states, 3);
		} else if(daisy_counter == 3) {
			Serial.write(0x80 | incomingByte);
		} else {
			Serial.write(incomingByte);
		}

		daisy_counter++;
	}
}

void displayByte(state_t* states, uint8_t num_states, uint8_t byte) {
	for (int i = 0; i < 8; i++) {
		bool enabled = (byte & (1 << i)) != 0;
		set(states, num_states, i, enabled, enabled);
	}
	set(states, num_states, 8, false, false);
	write(states, num_states);
}

void set(state_t* states, uint8_t num_states, uint8_t position, bool en, bool dir) {
	uint8_t state_index = position / 3;
	if (state_index >= num_states) return;

	uint8_t offset = position % 3;

	states[state_index].en &= ~(1 << offset);
	states[state_index].en |= (en << offset);

	states[state_index].dir &= ~(1 << offset);
	states[state_index].dir |= (dir << offset);
}

void write(const state_t* states, uint8_t num_states) {
	digitalWrite(SS_PIN, LOW);

	for (int i = num_states-1; i >= 0; i--) {
		uint8_t ncvEn = 0;
		uint8_t ncvCC = 0;

		for (int j = 0; j < 3; j++) {
			if ((states[i].en & (1 << j)) != 0) {
				ncvEn |= (1 << j*2) | (1 << (j*2 +1));
			}

			if ((states[i].dir & (1 << j)) != 0) {
				ncvCC |= (1 << j*2) | (0 << (j*2 +1));
			} else {
				ncvCC |= (0 << j*2) | (1 << (j*2 +1));
			}
		}

		SPI.transfer(0 << 7 | 0 << 6 | 0 << 5 | (ncvEn >> 1) & 0x1F);
		SPI.transfer((ncvEn & 0x01) << 7 | (ncvCC & 0x3F) << 1 | 0);
	}

	digitalWrite(SS_PIN, HIGH);
}