import processing.serial.*;

float borderPct = 0.2;

boolean debugSerial = true;

int tapDimX = 3;
int tapDimY = 3;

TapConf tapConf;

boolean[][] states = new boolean[tapDimX][tapDimY];

Serial arduinoMaster;

boolean showConf = false;

public void setup() {
	size(500, 500);

	println(Serial.list());

	arduinoMaster = new Serial(this, Serial.list()[0], 115200);
	tapConf = new TapConf(arduinoMaster, 5, 5, 5, 5);

	tapConf.sendConf();
}

public void draw() {
	background(0);

	for (int i = 0; i < tapDimX; i++) {
		for (int j = 0; j < tapDimY; j++) {
			strokeWeight(4);
			fill(states[i][j] ? 200 : 0);
			stroke(255);
			rect((i+borderPct) * width / tapDimX, (j+borderPct) * height / tapDimY, (width / tapDimX) * (1.0 - borderPct * 2), (height / tapDimY) * (1.0 - borderPct * 2));
		}
	}

	if (showConf) {

		fill(100);
		noStroke();
		rect(15, 5, 130, 70);

		fill(255);
		stroke(255);

		int spacing = 15;

		text(String.format("upPulseLen : %d", tapConf.upPulseLen), 20, 20+spacing*0);
		text(String.format("interPulseDelay : %d", tapConf.interPulseDelay), 20, 20+spacing*1);
		text(String.format("downPulseLen : %d", tapConf.downPulseLen), 20, 20+spacing*2);
		text(String.format("pauseLen : %d", tapConf.pauseLen), 20, 20+spacing*3);
	}
}

// keypress

public void keyPressed() {
	int incr = Character.isUpperCase(key) ? 10 : 1;

	switch (Character.toLowerCase(key)) {
		case 'q': {
			tapConf.upPulseLen += incr;
			tapConf.sendConf();
			break;
		}
		case 'a': {
			tapConf.upPulseLen -= incr;
			tapConf.sendConf();
			break;
		}

		case 'w': {
			tapConf.interPulseDelay += incr;
			tapConf.sendConf();
			break;
		}
		case 's': {
			tapConf.interPulseDelay -= incr;
			tapConf.sendConf();
			break;
		}

		case 'e': {
			tapConf.downPulseLen += incr;
			tapConf.sendConf();
			break;
		}
		case 'd': {
			tapConf.downPulseLen -= incr;
			tapConf.sendConf();
			break;
		}

		case 'r': {
			tapConf.pauseLen += incr;
			tapConf.sendConf();
			break;
		}
		case 'f': {
			tapConf.pauseLen -= incr;
			tapConf.sendConf();
			break;
		}

		case 'z': {
			showConf = !showConf;
		}
	}
}

// Mouse

public void mouseDragged() {
	if (isInterstitial()) return;
	setState(mouseXToX(mouseX), mouseYToY(mouseY), true);
}

public void mousePressed() {
	if (isInterstitial()) return;
	setState(mouseXToX(mouseX), mouseYToY(mouseY), true);
}

public void mouseReleased() {
	setAllStates(false);
}

public boolean isInterstitial() {
	float x = (mouseX / ((float)width / tapDimX)) % 1;
	float y = (mouseY / ((float)height / tapDimY)) % 1;

	return (x < borderPct || y < borderPct || x > 1 - borderPct || y > 1 - borderPct);
}

public int mouseXToX(int mouseX) {
	return constrain(floor(mouseX / (width / tapDimX)), 0, tapDimX - 1);
}

public int mouseYToY(int mouseY) {
	return constrain(floor(mouseY / (height / tapDimY)), 0, tapDimY - 1);
}

// States

public void setState(int x, int y, boolean state) {
	boolean changed = states[x][y] != state;
	states[x][y] = state;
	if (changed) pushStates();
}

public void setAllStates(boolean state) {
	boolean changed = false;
	for (int i = 0; i < tapDimX; i++) {
		for (int j = 0; j < tapDimY; j++) {
			if (states[i][j] != state) changed = true;
			states[i][j] = state;
		}
	}
	if (changed) pushStates();
}

public void pushStates() {
	writeArduinoMaster(0x81);
	for (int i = 0; i < tapDimX * tapDimY / 9; i++) {
		byte[] out = new byte[2];
		for (int j = 0; j < 3; j++) {
			for (int k = 0; k < 3; k++) {
				int baseX = (i * 3) % tapDimX;
				int baseY = floor((i * 3) / tapDimX) * 3;

				int bitIndex = (j*3 + k) % 8;
				int outIndex = j*3 + k < 8 ? 0 : 1;
				if (states[baseX+j][baseY+k]) {
					out[outIndex] = setBit(out[outIndex], bitIndex);
				}
			}
		}
		writeArduinoMaster(out);
	}
	writeArduinoMaster(0x82);
}

public byte setBit(byte val, int pos) {
	return (byte)(val | (1 << pos));
}

public byte clearBit(byte val, int pos) {
	return (byte)(val & ~(1 << pos));
}

public void writeArduinoMaster(int val) {
	if (debugSerial) {
		println(String.format("Write: %x", (byte)val));
	}
	arduinoMaster.write(val);
}

public void writeArduinoMaster(byte[] bytes) {
	if (debugSerial) {
		print("Write:");
		for (int i = 0; i < bytes.length; i++) {
			print(String.format(" %x", bytes[i]));
		}
		println();
	}
	arduinoMaster.write(bytes);
}

// Conf

class TapConf {

	Serial arduinoMaster;
	int upPulseLen, interPulseDelay, downPulseLen, pauseLen;

	public TapConf(Serial ard, int up, int inter, int down, int pause) {
		upPulseLen = up;
		interPulseDelay = inter;
		downPulseLen = down;
		pauseLen = pause;
		arduinoMaster = ard;
	}

	public void sendConf() {
		writeArduinoMaster(0x80);

		writeArduinoMaster((byte)(upPulseLen & 0xFF));
		writeArduinoMaster((byte)((upPulseLen & 0xFF00) >> 8));

		writeArduinoMaster((byte)(interPulseDelay & 0xFF));
		writeArduinoMaster((byte)((interPulseDelay & 0xFF00) >> 8));

		writeArduinoMaster((byte)(downPulseLen & 0xFF));
		writeArduinoMaster((byte)((downPulseLen & 0xFF00) >> 8));

		writeArduinoMaster((byte)(pauseLen & 0xFF));
		writeArduinoMaster((byte)((pauseLen & 0xFF00) >> 8));

	}

}