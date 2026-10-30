// Board LED
// Board's LED control

#include "board_config.h"

#if BOARD_LED_ENABLED

#define BLINK_PERIOD      500000 // slow blink: 500 ms half period, 1 Hz
#define BLINK_FAST_PERIOD  62500 // fast blink: 62.5 ms half period, 8 Hz


// ---- External ----
extern bool armed;
extern float t;
extern float controlTime;
extern float rcLossTimeout;
extern float thrustTarget;    // control.ino
extern float batteryVoltage;  // battery.ino
extern bool isInverted; // safety.ino
#if WEB_RC_ENABLED
extern bool webRCEnabled;
extern bool useWebRC;
bool isUsingWebRC();
#endif

void setupLED() {
	pinMode(BOARD_LED_PIN, OUTPUT);
	digitalWrite(BOARD_LED_PIN, BOARD_LED_INVERTED ? HIGH : LOW); // off to start
}

void setLED(bool on) {
	static bool state = false;
	if (on == state) {
		return; // don't call digitalWrite if the state is the same
	}
	digitalWrite(BOARD_LED_PIN, (on ^ BOARD_LED_INVERTED) ? HIGH : LOW);
	state = on;
}

void blinkLED() {
	setLED(micros() / BLINK_PERIOD % 2);
}

// Battery warning: threshold by flight state
// in flight (thrustTarget >= 0.15): L2; L1 doesn't apply
// disarmed or armed idle: L1
bool batteryAlertActive() {
	if (batteryVoltage <= VBAT_ABSENT_THRESHOLD) return false;
	bool flying = armed && thrustTarget >= 0.15f;
	if (flying) return batteryVoltage < VBAT_LOW_THRESHOLD; // L2: in flight
	else        return batteryVoltage < VBAT_WARN_THRESHOLD; // L1: disarmed or idle
}

// any warning (low battery, RC lost, upside down)
bool ledAlertActive() {
	// upside down
	if (isInverted) return true;

	// RC lost (SBUS, only once armed)
	if (controlTime != 0 && armed && (t - controlTime > rcLossTimeout)) return true;

#if WEB_RC_ENABLED
	// Web RC lost: was active, timed out
	if (webRCEnabled && useWebRC && !isUsingWebRC()) return true;
#endif

	if (batteryAlertActive()) return true;

	return false;
}

// called from the main loop: drives the LED from flight state
void updateLED() {
	if (!armed) {
		if (batteryAlertActive()) {
			setLED(micros() / BLINK_FAST_PERIOD % 2); // low battery before arming: fast blink
		} else {
			setLED(false); // standby: off
		}
		return;
	}
	if (ledAlertActive()) {
		setLED(micros() / BLINK_FAST_PERIOD % 2); // any warning: 8 Hz
	} else {
		setLED(micros() / BLINK_PERIOD % 2); // normal flight: 1 Hz
	}
}

#else // BOARD_LED_ENABLED == 0 (no usable LED)

void setupLED() {}
void setLED(bool on) {}
void updateLED() {}

#endif // BOARD_LED_ENABLED
