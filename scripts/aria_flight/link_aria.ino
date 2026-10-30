// link_aria.ino - her own controls on CF-Drone's flight core. The frames arrive on the other core
// (aria_fleet.cpp, aria_ble.cpp); the flight loop picks them up here, between its own inputs.

#include "aria_link_logic.h"
#include "aria_fleet.h"

extern bool armed;
extern bool imuOK;
extern float t;
extern float batteryVoltage;
extern float controlTime;
extern float controlRoll, controlPitch, controlYaw, controlThrottle, controlMode;

static AriaLink ariaLink;
static portMUX_TYPE ariaLinkLock = portMUX_INITIALIZER_UNLOCKED;
volatile int ariaTalkLed = -1;          // -1: the LED is CF-Drone's. 0/1: she's talking

void ariaLinkFrame(const uint8_t *frame) {
	const uint32_t now = millis();
	portENTER_CRITICAL(&ariaLinkLock);
	ariaLinkTake(&ariaLink, frame, now);
	portEXIT_CRITICAL(&ariaLinkLock);
}

void readAriaLink() {
	const uint32_t now = millis();
	portENTER_CRITICAL(&ariaLinkLock);
	const AriaLinkOut out = ariaLinkStep(&ariaLink, armed, now);
	portEXIT_CRITICAL(&ariaLinkLock);

	if (out.lost) {
		descend();
		controlTime = t; // this is the only descent running: CF-Drone's RC loss would double it
		return;
	}
	if (!out.drive) return;

	controlRoll = out.roll;
	controlPitch = out.pitch;
	controlYaw = out.yaw;
	controlThrottle = out.throttle;
	controlMode = NAN;
	if (out.refresh) controlTime = t;

	if (out.disarm) armed = false;
	if (out.arm && imuOK && !(batteryVoltage > VBAT_ABSENT_THRESHOLD && batteryVoltage < VBAT_WARN_THRESHOLD)) {
		armed = true;
	}
}

void ariaTalkTick() {
	if (ariaTalkLed >= 0) setLED(ariaTalkLed);
}
