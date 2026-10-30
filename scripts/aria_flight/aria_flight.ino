// Main program
// Build with Arduino IDE 2.3.x or newer; 1.8.19 isn't supported
// Needs the esp32 board package (esp32 by Espressif Systems) and the MAVLink library

#include "vector.h"
#include "quaternion.h"
#include "util.h"
#include "board_config.h"
#include "aria_fleet.h"

// WiFi and Web RC are switched per chip in board_config.h.
// To override: #undef here and #define again
#define WIFI_ENABLED    BOARD_WIFI_ENABLED
#define WEB_RC_ENABLED  BOARD_WEB_RC_ENABLED

float t = NAN; // current step time, s
float dt; // time since the previous step, s
float controlRoll, controlPitch, controlYaw, controlThrottle; // pilot's inputs, range [-1, 1]
float controlMode = NAN;
Vector gyro; // gyroscope data
Vector acc; // accelerometer data, m/s/s
Vector rates; // filtered angular rates, rad/s
Quaternion attitude; // estimated attitude (quaternion)
bool landed; // are we landed and stationary

void setup() {
	Serial.begin(115200); // serial at 115200
	disableBrownOut(); // disable the brown-out reset, or the voltage dip as the motors start resets the chip
	print("ARIA flight controller\n");
	print("Starting up\n");
	setupParameters(); // load parameters from flash (defaults if none stored)
	setupLED(); // status LED
	setupMotors(); // motor outputs (PWM/DShot)
	setLED(true); // LED on while starting
#if WIFI_ENABLED
	setupWiFi(); // WiFi (Web RC, MAVLink)
#endif
#if WEB_RC_ENABLED
	setupWebRC(); // Web RC
#endif
	ariaFleetStart(); // ARIA: the fleet, her :5014 page, Bluetooth, the app's flight link
	setupIMU(); // IMU (gyro and accelerometer)
	setupRC(); // RC receiver (SBUS, ELRS)
	setLED(false); // LED off: started
	print("Start-up complete\n");
	print("================================\n");
}

void loop() {
	readIMU(); // read the IMU, apply calibration and rotation
	step(); // work out t and dt, count the loop rate
	readRC(); // read the RC receiver
#if WEB_RC_ENABLED
	readWebRC(); // read Web RC
	processConsoleCommandQueue(); // run web console commands here, not in the HTTP callback
#endif
	readAriaLink(); // ARIA: flight commands from the app, browser or Bluetooth
	estimate(); // attitude estimate (complementary filter on the IMU)
	updateBatteryVoltage(); // sample battery voltage
	control(); // flight controller: attitude PIDs to motor outputs
	sendMotors(); // write motor outputs (PWM/DShot)
	handleInput(); // serial and web console commands
#if WIFI_ENABLED
	processMavlink(); // MAVLink
#endif
	logData(); // flight log
	syncParameters(); // write changed parameters to flash later, to spare it
	updateLED(); // LED from flight state
	ariaTalkTick(); // ARIA: Brainfuck talk on the LED
}
