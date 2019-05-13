import processing.serial.*;

/*
Pattern of board connections would be like so

2 3 8
1 4 7
0 5 6

With the first tapper located in the upper left corner
of board zero (next to board one on the outside edge)
*/

final int numXBoards = 2;
final int numYBoards = 2;

// The product of numXBoards*numYBoards should equal NUM_BOARDS in arduino

final int tapDimX = 6*numXBoards;
final int tapDimY = 6*numYBoards;

final float intialUpPulseLen = 20; //ms
final float initialInterPulseDelay = 20; //ms
final float initialDownPulseLen = 20; //ms
final float initialPauseLen = 20; //ms

final int boardTappersX = 6;
final int boardTappersY = 6;
final int bridgesPerChip = 6;
final int chipsPerBoard = boardTappersX*boardTappersY/bridgesPerChip;

final float minFreq = 1;
final float maxFreq = 50;

final float borderPct = 0.2;

boolean debugSerial = false;
boolean confd = false;

TapConf tapConf;

boolean[][] states = new boolean[tapDimX][tapDimY];

Serial arduinoMaster;

boolean shifted = false;
boolean lockMode = true;
boolean lockModeInitial = false;

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
		print(i);
		print(" >");
		println(ports[i]);

		if (ports[i].indexOf("usbmodem") != -1) {
			targetIndex = i;
		}
	}

	print("Using: ");
	print(targetIndex);
	print(" ");
	println(ports[targetIndex]);

	arduinoMaster = new Serial(this, Serial.list()[targetIndex], 115200);
	arduinoMaster.bufferUntil(10);

	tapConf = new TapConf(arduinoMaster, (int) (intialUpPulseLen*100), (int) (initialInterPulseDelay*100), (int) (initialDownPulseLen*100), (int) (initialPauseLen*100));
}

public void draw() {
	background(0);

	drawTapInteraction();	

	translate(width/2, 0);
	drawWaveAndConf();
}

public void drawTapInteraction() {
	for (int i = 0; i < tapDimX; i++) {
		for (int j = 0; j < tapDimY; j++) {
			strokeWeight(4);
			fill(states[i][j] ? 200 : 0);
			stroke(255);
			rect((i+borderPct) * width / 2 / tapDimX, (j+borderPct) * height / tapDimY, (width / 2 / tapDimX) * (1.0 - borderPct * 2), (height / tapDimY) * (1.0 - borderPct * 2));
		}
	}

	strokeWeight(2);
	stroke(100);

	for (int i = 1; i < floor(tapDimX / (float)boardTappersX); i++) {
		line(i * boardTappersX * width / 2 / tapDimX, 0, i * boardTappersX * width / 2 / tapDimX, height);
	}

	for (int i = 1; i < floor(tapDimY / (float)boardTappersY); i++) {
		line(0, i * boardTappersY * height / tapDimY , width/2, i * boardTappersY * height / tapDimY);
	}

	fill(255);
	stroke(255);
	text("'SHIFT' to sustain    'm' to toggle locking mode", 20, 20);
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
	line(tapConf.downEndPixel(), 2*height/4, width/2, 2*height/4);

	fill(255);
	stroke(255);

	int spacing = 15;
	int count = 0;

	text(String.format("upPulseLen : %.2f ms", (float)tapConf.upPulseLen / 100), 20, 20+spacing*count++);
	text(String.format("interPulseDelay : %.2f ms", (float)tapConf.interPulseDelay / 100), 20, 20+spacing*count++);
	text(String.format("downPulseLen : %.2f ms", (float)tapConf.downPulseLen / 100), 20, 20+spacing*count++);
	text(String.format("pauseLen : %.2f ms", (float)tapConf.pauseLen / 100), 20, 20+spacing*count++);
	count++;
	text(String.format("period/freq : %.2f * ms / %.2f * Hz", tapConf.period() / 100f, 100000f / tapConf.period()), 20, 20+spacing*count++);

	fill(255);
	noStroke();

	float freq = 100000f / tapConf.period();
	// int freqWidth = constrain(map(freq, minFreq, maxFreq, 0, width/2), 0, width/2);
	int freqWidth = int(constrain(map(log(constrain(freq, 1, 20e3)), log(minFreq), log(maxFreq), 0, width/2), 0, width/2));

	rect(0, height - 60, freqWidth, 20);

	fill(0);
	stroke(0);
	text("freq slider (log)", 20, height-48);

}

// keypress

public void keyReleased() {
	if (key == CODED && keyCode == SHIFT) shifted = false;
}

public void keyPressed() {
	if (key == CODED && keyCode == SHIFT) {
		shifted = true;
	}

	if (key == 'm') {
		lockMode = !lockMode;
	}
}

// Mouse

public void mouseDragged() {
	switch (mode) {
		case TAP_INTERACTION: {
			if (isInterstitial()) return;

			if (lockMode) {
				setState(mouseXToX(mouseX), mouseYToY(mouseY), !lockModeInitial);
			} else if (shifted) {
				setState(mouseXToX(mouseX), mouseYToY(mouseY), true);
			} else {
				setSingleEnabled(mouseXToX(mouseX), mouseYToY(mouseY));
			}
			break;
		}
		case WAVE_AND_CONF: {
			if (mouseY > height - 80) {
				// float freq = constrain(map(mouseX - width/2, 0, width/2, minFreq, maxFreq), minFreq, maxFreq);
				float freq = constrain(exp(map(mouseX - width/2, 0, width/2, log(minFreq), log(maxFreq))), minFreq, maxFreq);
				int period = int(100000f / freq);
				tapConf.setPeriod(period);
			} else {
				pdp = tapConf.closestDragPoint(mouseX-width/2);
				tapConf.setPoint(pdp, mouseX-width/2);
			}
			break;
		}
	}
}

public void mousePressed() {
	if (mouseX < width/2) {
		mode = Mode.TAP_INTERACTION;

		lockModeInitial = getState(mouseXToX(mouseX), mouseYToY(mouseY));

		mouseDragged();
	} else {
		mode = Mode.WAVE_AND_CONF;

		if (mouseY > height - 80) {

		} else {
			pdp = tapConf.closestDragPoint(mouseX-width/2);
			tapConf.setPoint(pdp, mouseX-width/2);
		}
	}
}

public void clearAllTaps() {
	setAllStates(false);
}

public void mouseReleased() {
	if ((!shifted && !lockMode) && mode == Mode.TAP_INTERACTION) {
		clearAllTaps();
	}

	if (tapConf.dirty) tapConf.sendConf();
}

public boolean isInterstitial() {
	float x = (mouseX / ((float)width/2 / tapDimX)) % 1;
	float y = (mouseY / ((float)height / tapDimY)) % 1;

	return (x < borderPct || y < borderPct || x > 1 - borderPct || y > 1 - borderPct);
}

public int mouseXToX(int mouseX) {
	return constrain(floor(mouseX / (width/2 / tapDimX)), 0, tapDimX - 1);
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

public boolean getState(int x, int y) {
	return states[x][y];
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
	for (int boardIx = 0; boardIx < tapDimX * tapDimY / (boardTappersX*boardTappersY); boardIx++) {

		// one state variable per chip
		byte[] out = new byte[chipsPerBoard];

		int boardRowX = floor(boardIx * boardTappersY / tapDimY);
		int boardBaseY = (boardIx * boardTappersY) % tapDimY;
		if (boardRowX % 2 == 0) boardBaseY = tapDimY - boardBaseY - boardTappersY;
		int boardBaseX = boardRowX*boardTappersX;

		for (int chipIx = 0; chipIx < chipsPerBoard; chipIx++) {
				int chipBaseX = (chipIx % 2) * 3;
				int chipBaseY = (chipIx / 2) * 2;
				int outIndex = chipIx;
  
				for (int chipY = 0; chipY < 2; chipY++) {
					for (int chipX = 0; chipX < 3; chipX++) {
						if (states[boardBaseX+chipBaseX+chipX][boardBaseY+chipBaseY+chipY]) {
							int bitIndex = chipX+chipY*3;
							out[outIndex] = setBit(out[outIndex], bitIndex);
							//println(boardIx + ":o-b" + outIndex + " - " + bitIndex + " .... x-y" + (boardBaseX+chipBaseX+chipX) + " - " + (boardBaseY+chipBaseY+chipY));
						}
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
	if (!confd) {
		String in = port.readString();
		if (in == null) return;
		if (trim(in).equals("ready")) tapConf.sendConf();
		confd = true;
		return;
	}

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

	public void setPeriod(int period) {
		float upPulseLenRatio = (float)upPulseLen / period();
		float interPulseDelayRatio = (float)interPulseDelay / period();
		float downPulseLenRatio = (float)downPulseLen / period();
		float pauseLenRatio = (float)pauseLen  / period();

		upPulseLen = (int)(upPulseLenRatio * period);
		interPulseDelay = (int)(interPulseDelayRatio * period);
		downPulseLen = (int)(downPulseLenRatio * period);
		pauseLen = (int)(pauseLenRatio * period);
		dirty = true;
	}

	public int upEndPixel() {
		return int(map(tapConf.upPulseLen, 0, tapConf.period(), 0, width/2));
	}

	public int downStartPixel() {
		return int(map(tapConf.upPulseLen+tapConf.interPulseDelay, 0, tapConf.period(), 0, width/2));
	}

	public int downEndPixel() {
		return int(map(tapConf.upPulseLen+tapConf.interPulseDelay+tapConf.downPulseLen, 0, tapConf.period(), 0, width/2));
	}

	public void setPoint(PulseDragPoint point, int pixelX) {
		dirty = true;

		switch (point) {
			case UP_END: {
				int period = tapConf.period(); // will fluctuate during calculation

				upPulseLen = int(constrain(
					map(pixelX, 0, width/2, 0, period),
					0,
					period - downPulseLen - pauseLen
				));

				interPulseDelay = period - upPulseLen - downPulseLen - pauseLen;
				break;
			}
			case DOWN_START: {
				int period = tapConf.period(); // will fluctuate during calculation

				interPulseDelay = int(constrain(
					map(pixelX, 0, width/2, 0, period) - upPulseLen,
					0,
					period - upPulseLen - pauseLen
				));

				downPulseLen = period - upPulseLen - interPulseDelay - pauseLen;
				break;
			}
			case DOWN_END: {
				int period = tapConf.period(); // will fluctuate during calculation

				downPulseLen = int(constrain(
					map(pixelX, 0, width/2, 0, period) - upPulseLen - interPulseDelay,
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
