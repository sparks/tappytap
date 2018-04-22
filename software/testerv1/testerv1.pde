import processing.serial.*;

float borderPct = 0.2;

boolean debugSerial = false;

int tapDimX = 3;
int tapDimY = 3;

TapConf tapConf;

boolean[][] states = new boolean[tapDimX][tapDimY];

Serial arduinoMaster;

boolean draggable = false;

public enum PulseDragPoint {
	UP_END, DOWN_START, DOWN_END
}

public enum Mode {
	TAP_INTERACTION, WAVE_AND_CONF
}

Mode mode = Mode.TAP_INTERACTION;
PulseDragPoint pdp = PulseDragPoint.UP_END;

public void setup() {
	size(1000, 500);

	int targetIndex = 0;

	String[] ports = Serial.list();

	println("Serial ports:");
	for (int i = 0; i < ports.length; i++) {
		print(" >");
		println(ports[i]);

		if (ports[i].indexOf("usbmodem") != -1) {
			targetIndex = i;
		}
	}

	print("Using: ");
	println(ports[targetIndex]);

	arduinoMaster = new Serial(this, Serial.list()[targetIndex], 115200);
	arduinoMaster.bufferUntil(10);

	tapConf = new TapConf(arduinoMaster, 100, 100, 100, 100);
}

public void draw() {
	background(0);

	drawTapInteraction();
	
	translate(500, 0);
	drawWaveAndConf();

	fill(255);
	stroke(255);

	text("'z' toggle wave conf --- 'x' toggle drag sustain --- 'SHIFT' tmp drag sustain", 20, 20);
	text("'q/a w/s e/d r/f' incr wave values +/- 1 (with 'SHIFT' +/- 10)", 20, height-10);
}

public void drawTapInteraction() {
	for (int i = 0; i < tapDimX; i++) {
		for (int j = 0; j < tapDimY; j++) {
			strokeWeight(4);
			fill(states[i][j] ? 200 : 0);
			stroke(255);
			rect((i+borderPct) * 500 / tapDimX, (j+borderPct) * height / tapDimY, (500 / tapDimX) * (1.0 - borderPct * 2), (height / tapDimY) * (1.0 - borderPct * 2));
		}
	}
}

public void drawWaveAndConf() {
	stroke(255);
	strokeWeight(5);
	
	if (tapConf.upEndPixel() != 0) {
		line(0, 2*height/4, 0, height/4);
		line(0, height/4, tapConf.upEndPixel(), height/4);
		line(tapConf.upEndPixel(), height/4, tapConf.upEndPixel(), 2*height/4);
	}
	line(tapConf.upEndPixel(), 2*height/4, tapConf.downStartPixel(), 2*height/4);
	if (tapConf.downStartPixel() != tapConf.downEndPixel()) {
		line(tapConf.downStartPixel(), 2*height/4, tapConf.downStartPixel(), 3*height/4);
		line(tapConf.downStartPixel(), 3*height/4, tapConf.downEndPixel(), 3*height/4);
		line(tapConf.downEndPixel(), 3*height/4, tapConf.downEndPixel(), 2*height/4);
	}
	line(tapConf.downEndPixel(), 2*height/4, 500, 2*height/4);

	fill(255);
	stroke(255);

	int spacing = 15;
	int count = 0;

	text(String.format("upPulseLen : %d * 10 µs", tapConf.upPulseLen), 20, 40+spacing*count++);
	text(String.format("interPulseDelay : %d * 10 µs", tapConf.interPulseDelay), 20, 40+spacing*count++);
	text(String.format("downPulseLen : %d * 10 µs", tapConf.downPulseLen), 20, 40+spacing*count++);
	text(String.format("pauseLen : %d * 10 µs", tapConf.pauseLen), 20, 40+spacing*count++);
	text(String.format("period/freq : %.2f * ms / %.2f * Hz", tapConf.period() / 10f, 100000f / tapConf.period()), 20, 40+spacing*count++);
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
			if (tapConf.upPulseLen < 0) tapConf.upPulseLen = 0;
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
			if (tapConf.interPulseDelay < 0) tapConf.interPulseDelay = 0;
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
			if (tapConf.downPulseLen < 0) tapConf.downPulseLen = 0;
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
			if (tapConf.pauseLen < 0) tapConf.pauseLen = 0;
			tapConf.sendConf();
			break;
		}

		case 'z': {
			mode = Mode.values()[(mode.ordinal() + 1) % Mode.values().length];
			break;
		}

		case 'x': {
			draggable = !draggable;
			break;
		}
	}
}

// Mouse

public void mouseDraggedTapInteraction() {
	if (isInterstitial()) return;

	if (draggable || keyCode == SHIFT) {
		setState(mouseXToX(mouseX), mouseYToY(mouseY), true);
	} else {
		setSingleEnabled(mouseXToX(mouseX), mouseYToY(mouseY));
	}
}

public void mouseDragged() {
	if (mouseX < 500) {
		mouseDraggedTapInteraction();
	} else {
		pdp = tapConf.closestDragPoint(mouseX-500);
		tapConf.setPoint(pdp, mouseX-500);
	}
}

public void mousePressed() {
	if (mouseX < 500) {
		mouseDragged();
	} else {
		pdp = tapConf.closestDragPoint(mouseX-500);
		tapConf.setPoint(pdp, mouseX-500);
	}
}

public void mouseReleased() {
	setAllStates(false);

	if (tapConf.dirty) tapConf.sendConf();
}

public boolean isInterstitial() {
	float x = (mouseX / ((float)500 / tapDimX)) % 1;
	float y = (mouseY / ((float)height / tapDimY)) % 1;

	return (x < borderPct || y < borderPct || x > 1 - borderPct || y > 1 - borderPct);
}

public int mouseXToX(int mouseX) {
	return constrain(floor(mouseX / (500 / tapDimX)), 0, tapDimX - 1);
}

public int mouseYToY(int mouseY) {
	return constrain(floor(mouseY / (height / tapDimY)), 0, tapDimY - 1);
}

// States

public void setSingleEnabled(int x, int y) {
	boolean changed = true;
	for (int i = 0; i < tapDimX; i++) {
		for (int j = 0; j < tapDimY; j++) {
			if (i == x && j == y) {
				if (states[i][j] != true) changed = true;
				states[i][j] = true;
			} else {
				if (states[i][j] != false) changed = true;
				states[i][j] = false;
			}
		}
	}
	if (changed) pushStates();
}

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

				int bitIndex = (j*3 + k) % 7;
				int outIndex = j*3 + k < 7 ? 0 : 1;
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

void serialEvent(Serial port) {
	if (!debugSerial) return;

	print(port.readString());
}


// Conf

class TapConf {

	Serial arduinoMaster;
	int upPulseLen, interPulseDelay, downPulseLen, pauseLen;
	boolean dirty = false;

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

		dirty = false;
	}

	public int period() {
		return upPulseLen+interPulseDelay+downPulseLen+pauseLen;
	}

	public int upEndPixel() {
		return int(map(tapConf.upPulseLen, 0, tapConf.period(), 0, 500));
	}

	public int downStartPixel() {
		return int(map(tapConf.upPulseLen+tapConf.interPulseDelay, 0, tapConf.period(), 0, 500));
	}

	public int downEndPixel() {
		return int(map(tapConf.upPulseLen+tapConf.interPulseDelay+tapConf.downPulseLen, 0, tapConf.period(), 0, 500));
	}

	public void setPoint(PulseDragPoint point, int pixelX) {
		dirty = true;

		switch (point) {
			case UP_END: {
				int period = tapConf.period(); // will fluctuate during calculation

				upPulseLen = int(constrain(
					map(pixelX, 0, 500, 0, period),
					0,
					period - downPulseLen - pauseLen
				));

				interPulseDelay = period - upPulseLen - downPulseLen - pauseLen;
				break;
			}
			case DOWN_START: {
				int period = tapConf.period(); // will fluctuate during calculation

				interPulseDelay = int(constrain(
					map(pixelX, 0, 500, 0, period) - upPulseLen,
					0,
					period - upPulseLen - pauseLen
				));

				downPulseLen = period - upPulseLen - interPulseDelay - pauseLen;
				break;
			}
			case DOWN_END: {
				int period = tapConf.period(); // will fluctuate during calculation

				downPulseLen = int(constrain(
					map(pixelX, 0, 500, 0, period) - upPulseLen - interPulseDelay,
					0,
					period - upPulseLen - interPulseDelay
				));

				pauseLen = period - upPulseLen - interPulseDelay - downPulseLen;
				break;
			}
		}
	}

	public PulseDragPoint closestDragPoint(int pixelX) {
		int upEndDist = abs(upEndPixel()-pixelX);
		int downStartDist = abs(downStartPixel()-pixelX);
		int downEndDist = abs(downEndPixel()-pixelX);

		if (upEndDist <= downStartDist && upEndDist <= downEndDist) {
			return PulseDragPoint.UP_END;
		} else if (downStartDist <= downEndDist) {
			return PulseDragPoint.DOWN_START;
		} else {
			return PulseDragPoint.DOWN_END;
		}
	}

}