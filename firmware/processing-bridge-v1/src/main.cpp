// inslude the SPI library:
#include <Arduino.h>
#include <SPI.h>

#define NUM_BOARDS 1
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

int phase = 0;

// The high level one off state as seen graphically in processing
bool bstates[TOTAL_BRIDGES];

// Serial comm variables

serial_mode_t mode = MODE_NONE;
int serial_byte_count = 0;

// Pulse configuration variables

uint16_t tmpUpPulseLen = 0, tmpInterPulseLen = 0, tmpDownPulseLen = 0, tmpPauseLen = 0;
uint16_t upPulseLen = 1000, interPulseLen = 0, downPulseLen = 1000, pauseLen = 0;

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
	SPI.beginTransaction(SPISettings(5e6, MSBFIRST, SPI_MODE1));

	Serial.begin(115200);
}

void loop() {
	if (Serial.available() > 0) {
		// Read uart 
		uint8_t incomingByte = Serial.read();
		
		if (SERIAL_DEBUG) {
			Serial.print("serial_byte_count: ");
			Serial.println(serial_byte_count);
		}

		switch(mode) {
			case MODE_CONF: {
				if (SERIAL_DEBUG) Serial.println("Conf started");

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
						tmpInterPulseLen |= incomingByte;
						break;
					}
					case 3: {
						tmpInterPulseLen |= incomingByte << 8;
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
						interPulseLen = tmpInterPulseLen;
						downPulseLen = tmpDownPulseLen;
						pauseLen = tmpPauseLen;
						if (SERIAL_DEBUG) {
							Serial.println("Conf done");
							Serial.print("  >upPulseLen: ");
							Serial.println(upPulseLen);
							Serial.print("  >interPulseLen: ");
							Serial.println(interPulseLen);
							Serial.print("  >downPulseLen: ");
							Serial.println(downPulseLen);
							Serial.print("  >pauseLen: ");
							Serial.println(pauseLen);
							Serial.println();
						}

						mode = MODE_NONE;
						break;
					}
				}

				serial_byte_count++;
				break;
			}

			case MODE_STATE: {
				if (SERIAL_DEBUG) Serial.println("State started");
				if (incomingByte == 0x82) {
					if (SERIAL_DEBUG) {
						if (SERIAL_DEBUG) {
							Serial.println("State done");
							for (int i = 0; i < TOTAL_BRIDGES; i++) {
								if (i % 3 == 0) Serial.print("  >");
								Serial.print(bstates[(i / BRIDGE_PER_BOARD) * BRIDGE_PER_BOARD + (i % BRIDGE_PER_BOARD) / 3 + (i%3)*3]);
								Serial.print(" ");
								if (i % 3 == 2) Serial.println();
								if (i % 9 == 8) Serial.println();
							}
						}
					}

					// We just latch as we go, there's not really risk to that
					mode = MODE_NONE;
					break;
				}

				int base_offset = serial_byte_count/2*BRIDGE_PER_BOARD;

				if (serial_byte_count % 2 == 0) {
					for (int i = 0; i < 7; i++) {
						bstates[base_offset+i] = (incomingByte & (1 << i)) > 0;
					}
				} else {
					for (int i = 0; i < 2; i++) {
						bstates[base_offset+7+i] = (incomingByte & (1 << i)) > 0;
					}
				}
				serial_byte_count++;
				break;
			}
			default:
			case MODE_NONE: {
				if (SERIAL_DEBUG) Serial.println("None start");
				serial_byte_count = 0;
				switch(incomingByte) {
					case 0x80: {
						if (SERIAL_DEBUG) Serial.println("  >Conf");
						mode = MODE_CONF;

						tmpUpPulseLen = 0;
						tmpInterPulseLen = 0;
						tmpDownPulseLen = 0;
						tmpPauseLen = 0;
						break;
					}
					case 0x81: {
						if (SERIAL_DEBUG) Serial.println("  >State");
						mode = MODE_STATE;
						break;
					}
					default: {
						if (SERIAL_DEBUG) Serial.println("  >?");
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
	unsigned long cur_time = micros()/10;
	unsigned long period = upPulseLen+interPulseLen+downPulseLen+pauseLen;
	unsigned long cur_period = cur_time % period;

	if (cur_period < upPulseLen && (phase == 0 || phase == 3)) {
		// pulse fwd
		for (int i = 0; i < TOTAL_BRIDGES; i++) set(states, NCV_CHIPS, i, bstates[i], false);
		if (upPulseLen > MIN_PHASE_TIME) write(states, NCV_CHIPS);
		phase = 1;
	} else if (cur_period >= upPulseLen && phase <= 1) {
		// idle
		for (int i = 0; i < TOTAL_BRIDGES; i++) set(states, NCV_CHIPS, i, false, false);
		write(states, NCV_CHIPS);
		phase = 2;
	} else if (cur_period >= upPulseLen+interPulseLen && phase <= 2) {
		// pulse back
		for (int i = 0; i < TOTAL_BRIDGES; i++) set(states, NCV_CHIPS, i, bstates[i], true);
		if (downPulseLen > MIN_PHASE_TIME) write(states, NCV_CHIPS);
		phase = 3;
	} else if (cur_period >= upPulseLen+interPulseLen+downPulseLen && phase <= 3) {
		// idle
		for (int i = 0; i < TOTAL_BRIDGES; i++) set(states, NCV_CHIPS, i, false, false);
		write(states, NCV_CHIPS);
		phase = 0;
	}

	phase = phase % 4;
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
		SPI.transfer(0 << 7 | 0 << 6 | 0 << 5 | ((ncvEn >> 1) & 0x1F));
		SPI.transfer(((ncvEn & 0x01) << 7) | ((ncvCC & 0x3F) << 1) | 0);
	}

	// deassert the slave select
	digitalWrite(SS_PIN, HIGH);
}