// inslude the SPI library:
#include <Arduino.h>
#include <SPI.h>

#define NUM_BOARDS 2
#define BRIDGES_PER_CHIP 6
#define CHIPS_PER_BOARD 6
#define NCV_CHIPS NUM_BOARDS * CHIPS_PER_BOARD
#define TOTAL_BRIDGES NCV_CHIPS * BRIDGES_PER_CHIP

#define NUM_REGISTERS 3

#define HB_ACT_1_CTRL_ADDR 0b00000
#define HB_ACT_2_CTRL_ADDR 0b10000
#define HB_ACT_3_CTRL_ADDR 0b01000

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

uint8_t HB_REG_ADDRESSES[NUM_REGISTERS] = {HB_ACT_1_CTRL_ADDR, HB_ACT_2_CTRL_ADDR, HB_ACT_3_CTRL_ADDR};

int phase = 0;

// The high level one off state as seen graphically in processing
bool bstates[TOTAL_BRIDGES];

// Serial comm variables

serial_mode_t mode = MODE_NONE;
int serial_byte_count = 0;

// Pulse configuration variables

uint32_t tmpUpPulseLen = 0, tmpInterPulseLen = 0, tmpDownPulseLen = 0, tmpPauseLen = 0;
uint32_t upPulseLen = 500, interPulseLen = 500, downPulseLen = 500, pauseLen = 500;

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
	SPI.beginTransaction(SPISettings(5e6, LSBFIRST, SPI_MODE1));
	SPI.setClockDivider(SPI_CLOCK_DIV16);

	Serial.begin(115200);

	Serial.println("ready");
}

void loop() {
	// set(states, NCV_CHIPS, 0, 1, 0);
	// set(states, NCV_CHIPS, 1, 1, 0);
	// set(states, NCV_CHIPS, 2, 1, 0);
	// set(states, NCV_CHIPS, 3, 1, 0);
	// set(states, NCV_CHIPS, 6, 1, 0);
	// set(states, NCV_CHIPS, 8, 1, 0);
	// set(states, NCV_CHIPS, 18, 1, 0);

	// write(states,  NCV_CHIPS);
	// delay(500);

	// set(states, NCV_CHIPS, 0, 1, 1);
	// set(states, NCV_CHIPS, 1, 1, 1);
	// set(states, NCV_CHIPS, 2, 1, 1);
	// set(states, NCV_CHIPS, 3, 1, 1);
	// set(states, NCV_CHIPS, 6, 1, 1);
	// set(states, NCV_CHIPS, 8, 1, 1);
	// set(states, NCV_CHIPS, 18, 1, 1);

	// write(states, NCV_CHIPS);
	// delay(500);

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
					// We just latch as we go, there's not really risk to that
					mode = MODE_NONE;
					break;
				}

				int base_offset = serial_byte_count*BRIDGES_PER_CHIP;
				for (int i = 0; i < BRIDGES_PER_CHIP; i++) {
					bstates[base_offset+i] = (incomingByte & (1 << i)) > 0;
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

	if (cur_period >= 0 && cur_period < upPulseLen && phase != 1) {
		// pulse fwd
		for (int i = 0; i < TOTAL_BRIDGES; i++) set(states, NCV_CHIPS, i, bstates[i], false);
		write(states, NCV_CHIPS);
		phase = 1;
	} else if (cur_period >= upPulseLen && cur_period < upPulseLen+interPulseLen && phase != 2) {
		// idle
		for (int i = 0; i < TOTAL_BRIDGES; i++) set(states, NCV_CHIPS, i, false, false);
		write(states, NCV_CHIPS);
		phase = 2;
	} else if (cur_period >= upPulseLen+interPulseLen && cur_period < upPulseLen+interPulseLen+downPulseLen && phase != 3) {
		// pulse back
		for (int i = 0; i < TOTAL_BRIDGES; i++) set(states, NCV_CHIPS, i, bstates[i], true);
		write(states, NCV_CHIPS);
		phase = 3;
	} else if (cur_period >= upPulseLen+interPulseLen+downPulseLen && cur_period < period && phase != 0) {
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
	uint8_t state_index = position / BRIDGES_PER_CHIP;
	if (state_index >= num_states) return;

	uint8_t offset = position % BRIDGES_PER_CHIP;

	states[state_index].en &= ~(1 << offset);
	states[state_index].en |= (en << offset);

	states[state_index].dir &= ~(1 << offset);
	states[state_index].dir |= (dir << offset);
}

// Write a state array out over SPI
void write(const state_t* states, uint8_t num_states) {
	// assert the slave select, start SPI frame

	for(int i = 0; i < NUM_REGISTERS; i++) {
		//send TOTAL_BRIDGES write commands for one HB_ACT_CTRL_i register at a time

		//begin new SPI frame to transfer data for each chip's HB_ACT_CTRL_i reg
		digitalWrite(SS_PIN, LOW);

		for (int j = 0; j < NCV_CHIPS; j++) {
			uint8_t addr = 0b10000001;
			if (j == NCV_CHIPS-1) addr = 0b10000011;
			addr |= HB_REG_ADDRESSES[i] << 2;

			SPI.transfer(addr);
		}
		//set the states of each HB_ACT_CTRL_i register according to the states variable
		for (int j = 0; j < NCV_CHIPS; j++) {
			uint8_t dataByte = 0;

			//for two nibbles in this register: 
			for(int k = 0; k < 2; k++) {
				//if (j == 0) Serial.println((1 << (i*2+k)));
				if ((states[j].en & (1 << (i*2+k))) == 0) {
					dataByte &= ~(0b1111 << (k*4));
				} else {
					if ((states[j].dir & (1 << (i*2+k))) != 0) {
						dataByte |= 0b1001 << (k*4);
					} else {
						dataByte |= 0b0110 << (k*4);
					}
				}
			}

			SPI.transfer(dataByte);
		}

		digitalWrite(SS_PIN, HIGH);
	}	
}