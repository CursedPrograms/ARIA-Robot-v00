/*
 * aria_pin_finder.ino - works out how her board is wired. PROPELLERS OFF.
 *
 * Nothing moves until you ask: every pin stays an input at power-up. Then, from the page on WiFi
 * ARIA_PINS / 12345678 at http://192.168.4.1, or over USB serial at 115200:
 *
 *   i          the chip, its flash and PSRAM
 *   p <pin>    pulse one pin gently, to see which motor (or LED) it is
 *   w          walk every pin in turn, pulsing each one
 *   x          stop, all pins off
 *   g          look for the gyro on the known SPI maps and on I2C
 *   s a b c d  look for the gyro on SPI: sck miso mosi cs
 *   a          read every analogue pin, to find the battery
 *
 * The pulse is PULSE_DUTY of full power for PULSE_MS, and always ends by itself. It's the same 25 kHz
 * PWM her flight code uses. Pins the chip can't spare (flash, USB, boot straps) are never touched.
 */
#include <Arduino.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

#include "pin_logic.h"
#include "web_files.h"

#define PULSE_MS 700
#define WALK_GAP_MS 1500
#define PULSE_DUTY 0.15f
#define PWM_HZ 25000
#define PWM_BITS 10

#ifdef CONFIG_IDF_TARGET_ESP32S3
static const int PINS[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 38, 39, 40, 41, 42, 47, 48};
static const int ADC_PINS[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
static const SpiMap SPI_MAPS[] = {{"S3", 12, 13, 11, 10}};
static const I2cMap I2C_MAPS[] = {{"S3", 35, 36}, {"Espressif", 11, 10}, {"sd196821", 16, 15}};
#else
static const int PINS[] = {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
static const int ADC_PINS[] = {32, 33, 34, 35, 36, 39};
static const SpiMap SPI_MAPS[] = {{"plain ESP32", 18, 19, 23, 5}};
static const I2cMap I2C_MAPS[] = {{"plain ESP32", 21, 22}};
#endif
static const int PIN_COUNT = sizeof(PINS) / sizeof(PINS[0]);

WebServer server(80);
static String lastLog = "Nothing tested yet. Propellers off!";
static int pulsing = -1;
static uint32_t pulseEnd = 0;
static int walkIndex = -1;
static uint32_t walkNext = 0;

static void say(const String &text) {
	Serial.println(text);
	lastLog = text;
}

static void stopPulse() {
	if (pulsing >= 0) {
		ledcWrite(pulsing, 0);
		ledcDetach(pulsing);
		pinMode(pulsing, INPUT);
		pulsing = -1;
	}
}

static void stopAll() {
	stopPulse();
	walkIndex = -1;
	say("Stopped: all pins off");
}

static bool pulse(int pin) {
	if (!pinIsListed(pin, PINS, PIN_COUNT)) {
		say("GPIO " + String(pin) + " isn't safe to pulse on this chip");
		return false;
	}
	stopPulse();
	if (!ledcAttach(pin, PWM_HZ, PWM_BITS)) {
		say("GPIO " + String(pin) + ": no PWM channel");
		return false;
	}
	ledcWrite(pin, pulseDuty(PULSE_DUTY, PWM_BITS));
	pulsing = pin;
	pulseEnd = millis() + PULSE_MS;
	say("Pulsing GPIO " + String(pin) + ": what moved?");
	return true;
}

static String chipInfo() {
	String s = String(ESP.getChipModel()) + " rev " + ESP.getChipRevision() + ", " + ESP.getChipCores() + " cores, flash "
		+ ESP.getFlashChipSize() / (1024 * 1024) + " MB, PSRAM " + ESP.getPsramSize() / (1024 * 1024) + " MB, MAC "
		+ WiFi.macAddress();
	return s;
}

// MPU-family WHO_AM_I over SPI: register 0x75 with the read bit
static int spiWhoAmI(int sck, int miso, int mosi, int cs) {
	SPI.end();
	pinMode(cs, OUTPUT);
	digitalWrite(cs, HIGH);
	SPI.begin(sck, miso, mosi, cs);
	SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
	digitalWrite(cs, LOW);
	SPI.transfer(0x75 | 0x80);
	const int id = SPI.transfer(0);
	digitalWrite(cs, HIGH);
	SPI.endTransaction();
	SPI.end();
	pinMode(cs, INPUT);
	return id;
}

static int i2cWhoAmI(int sda, int scl, int address) {
	Wire.begin(sda, scl, 100000);
	Wire.beginTransmission(address);
	Wire.write(0x75);
	int id = -1;
	if (Wire.endTransmission(false) == 0 && Wire.requestFrom(address, 1) == 1) {
		id = Wire.read();
	}
	Wire.end();
	return id;
}

static void spiTry(String &out, const char *name, int sck, int miso, int mosi, int cs) {
	const int id = spiWhoAmI(sck, miso, mosi, cs);
	const char *gyro = gyroName(id);
	out += "SPI " + String(name) + " (sck " + sck + ", miso " + miso + ", mosi " + mosi + ", cs " + cs + "): "
		+ (gyro ? String(gyro) : "nothing, read 0x" + String(id, HEX)) + "\n";
}

static void findGyro() {
	String out;
	for (const SpiMap &m : SPI_MAPS) spiTry(out, m.name, m.sck, m.miso, m.mosi, m.cs);
	for (const I2cMap &m : I2C_MAPS) {
		for (int address : {0x68, 0x69}) {
			const int id = i2cWhoAmI(m.sda, m.scl, address);
			if (id >= 0) {
				const char *gyro = gyroName(id);
				out += "I2C " + String(m.name) + " (sda " + m.sda + ", scl " + m.scl + ") 0x" + String(address, HEX) + ": "
					+ (gyro ? String(gyro) : "unknown, id 0x" + String(id, HEX)) + "\n";
			}
		}
	}
	say(out.length() ? out : "No gyro found");
}

// Her divider is 10k over 33k, so the pin sees 33/43 of the battery: 3.2 V at a full 4.2 V
static void readBattery() {
	String out;
	for (const int pin : ADC_PINS) {
		analogSetPinAttenuation(pin, ADC_11db);
		const float pinVolts = analogReadMilliVolts(pin) / 1000.0f;
		const float battery = batteryFromPin(pinVolts);
		out += "GPIO " + String(pin) + ": " + String(pinVolts, 2) + " V at the pin, " + String(battery, 2) + " V if it's the battery"
			+ (looksLikeBattery(battery) ? "  <-- could be\n" : "\n");
	}
	say(out);
}

static void command(String line) {
	line.trim();
	if (!line.length()) return;
	const char c = line[0];
	int v[4] = {-1, -1, -1, -1};
	sscanf(line.c_str() + 1, "%d %d %d %d", &v[0], &v[1], &v[2], &v[3]);
	if (c == 'i') say(chipInfo());
	else if (c == 'p') pulse(v[0]);
	else if (c == 'w') {
		walkIndex = 0;
		walkNext = 0;
	}
	else if (c == 'x') stopAll();
	else if (c == 'g') findGyro();
	else if (c == 's') {
		String out;
		spiTry(out, "custom", v[0], v[1], v[2], v[3]);
		say(out);
	}
	else if (c == 'a') readBattery();
	else say("Commands: i, p <pin>, w, x, g, s <sck> <miso> <mosi> <cs>, a");
}

static void tick() {
	const uint32_t now = millis();
	if (pulsing >= 0 && (int32_t)(now - pulseEnd) >= 0) {
		stopPulse();
	}
	if (walkIndex >= 0 && (int32_t)(now - walkNext) >= 0) {
		if (walkIndex >= PIN_COUNT) {
			walkIndex = -1;
			say("Walk finished");
			return;
		}
		pulse(PINS[walkIndex++]);
		walkNext = now + PULSE_MS + WALK_GAP_MS;
	}
}

void setup() {
	Serial.begin(115200);
	WiFi.softAP("ARIA_PINS", "12345678");

	server.on("/", []() { server.send(200, "text/html", WEB_INDEX_HTML); });
	server.on("/style.css", []() { server.send(200, "text/css", WEB_STYLE_CSS); });
	server.on("/app.js", []() { server.send(200, "text/javascript", WEB_APP_JS); });
	server.on("/cmd", []() {
		command(server.arg("c"));
		server.send(200, "text/plain", lastLog);
	});
	server.on("/pins", []() {
		String out = "[";
		for (int i = 0; i < PIN_COUNT; i++) out += (i ? "," : "") + String(PINS[i]);
		server.send(200, "application/json", out + "]");
	});
	server.on("/last", []() { server.send(200, "text/plain", lastLog); });
	server.begin();

	delay(1500);
	say(chipInfo());
	say("ARIA pin finder. PROPELLERS OFF. WiFi ARIA_PINS / 12345678, http://192.168.4.1");
}

void loop() {
	server.handleClient();
	if (Serial.available()) command(Serial.readStringUntil('\n'));
	tick();
	delay(1);
}
