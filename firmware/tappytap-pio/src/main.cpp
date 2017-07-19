// inslude the SPI library:
#include <Arduino.h>
#include <SPI.h>

// Struct representing the state of one NCV7718 chip which contains 3 hbridges:
//  * bits 0,1,2 of en represent if hbridges 0,1,2 are enabled
//  * bits 0,1,2 of dir represent the direction of hbridges 0,1,2
// So for example if en = 0b011 and dir = 0b010 then motors 0 and 1 are enabled
// with motor 0 and 1 going in opposite directions
typedef struct {
	uint8_t en;
	uint8_t dir;
} state_t;

void set(state_t*, uint8_t, uint8_t, bool, bool);
void write(const state_t*, uint8_t);
void copyTriple(uint8_t*, uint8_t, uint8_t);

void displayByte(state_t*, uint8_t, uint8_t);

// Slave select PIN for SPI (attached to all the NCV7718 chips) (active low)
#define SS_PIN 10
// Enable PIN for all the NCV7718 chips (active high)
#define NCV_EN_PIN 9

// There are three NCV7718 chips on each board, hence we have three state_t structs
// We init them off by setting en = 0 and dir = 0 for each
state_t states[3] = {
	{0, 0},
	{0, 0},
	{0, 0},
};

// Counter of received bytes
uint8_t daisy_counter = -1;

void setup() {
	// Set relevant pin modes
	pinMode(SS_PIN, OUTPUT);
	pinMode(NCV_EN_PIN, OUTPUT);

	digitalWrite(NCV_EN_PIN, HIGH); // enabled (active high)
	digitalWrite(SS_PIN, HIGH); // not selected (active low)

	// Configure SPI
	SPI.begin();
	SPI.setDataMode(SPI_MODE1);

	// I believe this is the fastest safe UART speed for the built in 8MHz clock, see here http://wormfood.net/avrbaudcalc.php
	Serial.begin(38400);
}

// helper function to copy three bits at a give bit offset from the source into the lower 3 bits of the destination
void copyTriple(uint8_t* dest, uint8_t source, uint8_t source_offset) {
	*dest = (source & (0b111 << source_offset)) >> source_offset;
}

void loop() {
	// Serial protocol:
	//
	// 3 bytes are sent for each daisy chained tappy tap board
	// containing the following bits
	// byte 1
	//   [mark][x][en6][en5][en4][en3][en2][en1]
	// byte 2
	//   [mark][x][dir3][dir2][dir1][en9][en8][en7]
	// byte 3
	//   [mark][x][dir9][dir8][dir7][dir6][dir5][dir4]
	//
	// [mark] bit indicated the very first byte of a new command. 
	// so it should be set to 1 for the first byte of an update and zero
	// the rest of the time
	// [x] is an unused bit
	// [en] are the enabled bits for all 9 hbridges
	// [dir] are the direction bits for all 9 hbridges
	//
	// So for example to enabled to the first hbridge on the first tappy tap
	// board and the last hbridge on the second (daisy chained) tappy tap board
	// you would send:
	// 0x81 (contains mark + en1 for first board) 0x80 (dir1=1 for first board) 0x00 (nothing set for dir4-9)
	// 0x00 (no mark + nothing set for en1-6) 0x40 (set en9) + 0x20 (set dir9=1)

	if (Serial.available() > 0) {
		// Read uart 
		uint8_t incomingByte = Serial.read();

		// We use the MSB as a marker for the start of a new command, reset the counter then
		if ((incomingByte & 0x80) != 0) daisy_counter = 0;

		if (daisy_counter < 0) return;

		// TODO(sparky): this is where we could add code that would use the 0th daisy_counter byte to have a latch count down to avoid flicker

		if (daisy_counter == 0) {
			// load the data from the first byte into our configuration (e.g. en1-6)
			copyTriple(&states[0].en, incomingByte, 0);
			copyTriple(&states[1].en, incomingByte, 3);
		} else if (daisy_counter == 1) {
			// load the data from the second byte into our configuration (e.g. en7-9 and dir1-3)
			copyTriple(&states[2].en, incomingByte, 0);
			copyTriple(&states[0].dir, incomingByte, 3);
		} else if(daisy_counter == 2) {
			// load the data from the second byte into our configuration (e.g. dir4-9)
			copyTriple(&states[1].dir, incomingByte, 0);
			copyTriple(&states[2].dir, incomingByte, 3);

			// Now we have all the data so we write it out over SPI to the NCV7718
			write(states, 3);
		} else if(daisy_counter == 3) {
			// The next byte after the three for this chip will be daisy chained to the next board and will be the "first" byte for 
			// that board. As such it need to have a mark bit set on it. So we add that bit and send it
			Serial.write(0x80 | incomingByte);
		} else {
			// Write the remaining received bytes forward through the daisy chain
			Serial.write(incomingByte);
		}

		// Increment our byte counter
		daisy_counter++;
	}
}

// Debugging function that displays a uint8_t as an LED pattern
void displayByte(state_t* states, uint8_t num_states, uint8_t byte) {
	for (int i = 0; i < 8; i++) {
		bool enabled = (byte & (1 << i)) != 0;
		set(states, num_states, i, enabled, enabled);
	}
	set(states, num_states, 8, false, false);
	write(states, num_states);
}

// Helper function if you want to set the en and dir for a particular hbridge manually
// e.g set(states, num_states, 3, 1, 1); would set the 3rd hbridge to en=1 dir=1
void set(state_t* states, uint8_t num_states, uint8_t position, bool en, bool dir) {
	uint8_t state_index = position / 3;
	if (state_index >= num_states) return;

	uint8_t offset = position % 3;

	states[state_index].en &= ~(1 << offset);
	states[state_index].en |= (en << offset);

	states[state_index].dir &= ~(1 << offset);
	states[state_index].dir |= (dir << offset);
}

// Write a state array out over SPI
void write(const state_t* states, uint8_t num_states) {
	// assert the slave select
	digitalWrite(SS_PIN, LOW);

	// For each NCV7718
	for (int i = num_states-1; i >= 0; i--) {
		// tmp variable to compute the actual SPI data to send. See datasheet for protocol details
		uint8_t ncvEn = 0;
		uint8_t ncvCC = 0;

		for (int j = 0; j < 3; j++) {
			// We bit shift our state struct into the target format
			if ((states[i].en & (1 << j)) != 0) {
				ncvEn |= (1 << j*2) | (1 << (j*2 +1));
			}

			if ((states[i].dir & (1 << j)) != 0) {
				ncvCC |= (1 << j*2) | (0 << (j*2 +1));
			} else {
				ncvCC |= (0 << j*2) | (1 << (j*2 +1));
			}
		}

		// Send 16 bits over SPI as per NCV7718 docs (we're ignoring the extra features and return values for now)
		SPI.transfer(0 << 7 | 0 << 6 | 0 << 5 | (ncvEn >> 1) & 0x1F);
		SPI.transfer((ncvEn & 0x01) << 7 | (ncvCC & 0x3F) << 1 | 0);
	}

	// deassert the slave select
	digitalWrite(SS_PIN, HIGH);
}