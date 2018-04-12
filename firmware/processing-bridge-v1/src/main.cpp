// inslude the SPI library:
#include <Arduino.h>
#include <SPI.h>

#define NUM_BOARDS 10
#define NCV_CHIPS NUM_BOARDS*3
#define BRIDGE_PER_BOARD 9
#define TOTAL_BRIDGES NUM_BOARDS*9

#define SERIAL_DEBUG false

// Slave select PIN for SPI (attached to all the NCV7718 chips) (active low)
#define SS_PIN 10
// Enable PIN for all the NCV7718 chips (active high)
#define NCV_EN_PIN 9

// Struct representing the state of one NCV7718 chip which contains 3 hbridges:
//  * bits 0,1,2 of en represent if hbridges 0,1,2 are enabled
//  * bits 0,1,2 of dir represent the direction of hbridges 0,1,2
// So for example if en = 0b011 and dir = 0b010 then motors 0 and 1 are enabled
// with motor 0 and 1 going in opposite directions
typedef struct {
	uint8_t en;
	uint8_t dir;
} state_t;

// Struct representing the current mode of serial communication
typedef enum _serial_mode_t {
	MODE_NONE,
	MODE_STATE,
	MODE_CONF
} serial_mode_t;

void set(state_t*, uint8_t, uint8_t, bool, bool);
void write(const state_t*, uint8_t);
void drive(const bool*);

// There are three NCV7718 chips on each board, hence we have three state_t structs
// We init them off by setting en = 0 and dir = 0 for each
state_t states[NCV_CHIPS];

// The high level one off state as seen graphically in processing
bool bstates[TOTAL_BRIDGES];

// Serial comm variables

serial_mode_t mode = MODE_NONE;
int serial_byte_count = 0;

// Pulse configuration variables

uint16_t tmpUpPulseLen, tmpInterPulseDelay, tmpDownPulseLen, tmpPauseLen = 0;
uint16_t upPulseLen, interPulseDelay, downPulseLen, pauseLen = 0;

void setup() {
	// Clear the states array
	for (int i = 0; i < NCV_CHIPS; i++) {
		states[i] = {0, 0};
	}

	for (int i = 0; i < TOTAL_BRIDGES; i++) {
		bstates[i] = false;
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

void loop() {
	if (Serial.available() > 0) {
		// Read uart 
		uint8_t incomingByte = Serial.read();
		
		if (SERIAL_DEBUG) Serial.println(serial_byte_count);

		switch(mode) {
			case MODE_CONF: {
				if (SERIAL_DEBUG) Serial.println("C");
				switch(serial_byte_count) {
					case 0: {
						tmpUpPulseLen |= incomingByte;
						break;
					}
					case 1: {
						tmpUpPulseLen |= incomingByte << 8;
						break;
					}
					case 2: {
						tmpInterPulseDelay |= incomingByte;
						break;
					}
					case 3: {
						tmpInterPulseDelay |= incomingByte << 8;
						break;
					}
					case 4: {
						tmpDownPulseLen |= incomingByte;
						break;
					}
					case 5: {
						tmpDownPulseLen |= incomingByte << 8;
						break;
					}
					case 6: {
						tmpPauseLen |= incomingByte;
						break;
					}
					case 7: {
						tmpPauseLen |= incomingByte << 8;
						// falls through to default now
					}
					default: {
						// latches
						upPulseLen = tmpUpPulseLen;
						interPulseDelay = tmpInterPulseDelay;
						downPulseLen = tmpDownPulseLen;
						pauseLen = tmpPauseLen;

						mode = MODE_NONE;
						break;
					}
				}

				serial_byte_count++;
				break;
			}
			case MODE_STATE: {
				if (SERIAL_DEBUG) Serial.println("S");
				if (incomingByte == 0x82) {
					// We just latch as we go, there's not really risk to that
					mode = MODE_NONE;
				}

				int base_offset = serial_byte_count/2*BRIDGE_PER_BOARD;

				if (serial_byte_count % 2 == 0) {
					for (int i = 0; i < 7; i++) {
						bstates[base_offset+i] = incomingByte & (1 << i) > 0;
					}
				} else {
					for (int i = 0; i < 2; i++) {
						bstates[base_offset+7+i] = incomingByte & (1 << i) > 0;
					}
				}
				serial_byte_count++;
				break;
			}
			default:
			case MODE_NONE: {
				if (SERIAL_DEBUG) Serial.println("N");
				serial_byte_count = 0;
				switch(incomingByte) {
					case 0x80: {
						mode = MODE_CONF;

						tmpUpPulseLen = 0;
						tmpInterPulseDelay = 0;
						tmpDownPulseLen = 0;
						tmpPauseLen = 0;
						break;
					}
					case 0x81: {
						mode = MODE_STATE;
						break;
					}
					default: {
						mode = MODE_NONE;
						break;
					}
				}
				break;
			}
		}		
	}

	drive(bstates);
}

void drive(const bool* bstates) {
	unsigned long cur_time = millis();
	unsigned long period = upPulseLen+interPulseDelay+downPulseLen+pauseLen;
	unsigned long cur_period = cur_time % period;

	for (int i = 0; i < TOTAL_BRIDGES; i++) {
		if (cur_period < upPulseLen) {
			// pulse fwd
			set(states, NCV_CHIPS, i, bstates[i], false);
		} else if (cur_period < upPulseLen+interPulseDelay) {
			// idle
			set(states, NCV_CHIPS, i, false, false);
		} else if (cur_period < upPulseLen+interPulseDelay+downPulseLen) {
			// pulse back
			set(states, NCV_CHIPS, i, bstates[i], true);
		} else {
			// idle
			set(states, NCV_CHIPS, i, false, false);
		}
	}
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