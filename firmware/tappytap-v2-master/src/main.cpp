// inslude the SPI library:
#include <Arduino.h>
#include <SPI.h>

#define NUM_BOARDS 10

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

void displayBytes(state_t*, uint8_t, uint8_t*, uint8_t);

// Slave select PIN for SPI (attached to all the NCV7718 chips) (active low)
#define SS_PIN 10
// Enable PIN for all the NCV7718 chips (active high)
#define NCV_EN_PIN 9

// There are three NCV7718 chips on each board, hence we have three state_t structs
// We init them off by setting en = 0 and dir = 0 for each
state_t states[NUM_BOARDS];

// Counter of received bytes
uint8_t daisy_counter = -1;

void setup() {
	// Clear the states array
	for (uint8_t i; i < NUM_BOARDS; i++) {
		states[i] = {0, 0};
	}

	// Set relevant pin modes
	pinMode(SS_PIN, OUTPUT);
	pinMode(NCV_EN_PIN, OUTPUT);

	digitalWrite(NCV_EN_PIN, HIGH); // enabled (active high)
	digitalWrite(SS_PIN, HIGH); // not selected (active low)

	// Configure SPI
	SPI.begin();
	SPI.setDataMode(SPI_MODE1);

	Serial.begin(115200);
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
	//   [mark2][mark1][en6][en5][en4][en3][en2][en1]
	// byte 2
	//   [mark2][mark1][dir3][dir2][dir1][en9][en8][en7]
	// byte 3
	//   [mark2][mark1][dir9][dir8][dir7][dir6][dir5][dir4]
	//
	// The first byte will always be of the form
	//   [1][0][en6][en5][en4][en3][en2][en1]
	// To indicate with [mark2] that it is the first byte
	//
	// The last byte (byte 3n+1) will always be of the form:
	//   [0][1][x][x][x][x][x][x]
	// where the [x] bits don't matter. To indicate with the [mark1] 
	// bit that the chips should latch
	//
	// [mark2] bit indicated the very first byte of a new command. 
	// so it should be set to 1 for the first byte of an update and zero
	// the rest of the time
	// [mark1] bit indicates a latch command, this actually transmits all the previously sent configurations
	// to the NCV boards. This will always be the last command in a sequence. Currently if this bit is
	// set, all other bits are ignored for the byte
	// [en] are the enabled bits for all 9 hbridges
	// [dir] are the direction bits for all 9 hbridges
	//
	// So for example to enabled to the first hbridge on the first tappy tap
	// board and the last hbridge on the second (daisy chained) tappy tap board
	// you would send:
	// 0x81 (contains mark + en1 for first board) 0x80 (dir1=1 for first board) 0x00 (nothing set for dir4-9)
	// 0x00 (no mark + nothing set for en1-6) 0x40 (set en9) + 0x20 (set dir9=1)
	// 0x40 (latch)

	if (Serial.available() > 0) {
		// Read uart 
		uint8_t incomingByte = Serial.read();

		// We use use the 7th bit [mark1] as a latch command
		if ((incomingByte & 0x40) != 0) {
			write(states, NUM_BOARDS);
			
			return;
		}

		// We use the MSB [mark2] as a marker for the start of a new command, reset the counter then
		if ((incomingByte & 0x80) != 0) daisy_counter = 0;
		if (daisy_counter < 0) return;

		uint8_t board_num = daisy_counter / 3;
		uint8_t sequence_num = daisy_counter % 3;

		if (board_num >= NUM_BOARDS) {
			Serial.println("Error: too many bytes transfered. You need to increase the NUM_BOARDS #define");
			return; // Out of bounds you've sent too much data
		}

		if (sequence_num == 0) {
			// load the data from the first byte into our configuration (e.g. en1-6)
			copyTriple(&states[board_num].en, incomingByte, 0);
			copyTriple(&states[board_num+1].en, incomingByte, 3);
		} else if (sequence_num == 1) {
			// load the data from the second byte into our configuration (e.g. en7-9 and dir1-3)
			copyTriple(&states[board_num+2].en, incomingByte, 0);
			copyTriple(&states[board_num].dir, incomingByte, 3);
		} else if(sequence_num == 2) {
			// load the data from the second byte into our configuration (e.g. dir4-9)
			copyTriple(&states[board_num+1].dir, incomingByte, 0);
			copyTriple(&states[board_num+2].dir, incomingByte, 3);
		}
	}
}

// Debugging function that displays a uint8_t as an LED pattern
void displayBytes(state_t* states, uint8_t num_states, uint8_t* bytes, uint8_t num_bytes) {
	// Clear all outputs
	for (uint8_t i = 0; i < num_states; i++) {
		for (uint8_t j = 0; j < 3; j++) { // There are 3 hbridges in each state
			set(states, num_states, i*3 + j, false, false); // That means that the position is 3 * state so far + offset in current states
		}
	}

	for (uint8_t i = 0; i < num_bytes; i++) { // for each byte
		for (uint8_t j = 0; j < 8; j++) { // for each bit
			bool enabled = (bytes[i] & (1 << j)) != 0;
			set(states, num_states, i*8 + j, enabled, enabled);
		}
	}

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