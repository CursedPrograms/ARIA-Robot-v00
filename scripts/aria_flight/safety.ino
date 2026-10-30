// Fail-safes
// Fail-safe functions

bool isInverted = false; // upside down now (Z axis cos < INVERTED_COS_THRESHOLD)

float rcLossTimeout = 1; // RC loss timeout (s): parameter SF_RC_LOSS_TIME
float descendTime = 10; // seconds down to a stop: parameter SF_DESCEND_TIME
#define WEB_RC_LOSS_TIMEOUT_MS 8000UL // Web RC loss limit (ms), must be over the 2000 ms heartbeat

// Upside-down protection
#define INVERTED_COS_THRESHOLD -0.7f // cos(134 deg): past 134 degrees counts as upside down (allows for gyro drift)
#define INVERTED_TIMEOUT        1.5f // stop after 1.5 s upside down

extern float controlTime;
extern float controlRoll, controlPitch, controlThrottle, controlYaw;

#if WEB_RC_ENABLED
// Web RC variables
extern bool webRCEnabled;
extern bool useWebRC;
extern unsigned long webRCLastUpdate;
bool isUsingWebRC();
#endif

extern bool armed;
extern int mode;
extern float dt;
extern float thrustTarget;
extern float t;
extern float batteryVoltage;  // battery.ino
extern Quaternion attitudeTarget;
extern Quaternion attitude;
extern Vector ratesExtra;      // control.ino
extern PID rollRatePID, pitchRatePID, yawRatePID;  // control.ino
extern PID rollPID, pitchPID, yawPID;              // control.ino
extern float motThrMin;        // control.ino

void failsafe() {
	rcLossFailsafe();
#if WEB_RC_ENABLED
	webRCLossFailsafe();
#endif
	autoFailsafe();
	invertedFailsafe();
	batteryFailsafe();
}

// RC loss failsafe
void rcLossFailsafe() {
	if (controlTime == 0) return; // no RC at all
	if (!armed) return;
#if WEB_RC_ENABLED
	if (isUsingWebRC()) return; // Web RC looks after its own timeout (webRCLossFailsafe)
#endif
	if (t - controlTime > rcLossTimeout) {
		descend();
	}
}

// Smooth descend on RC lost
void descend() {
	if (mode != AUTO) {
		// first time in: keep the current yaw, level her, clear rate feed-forward, reset PID integrals
		float currentYaw = attitude.getYaw();
		attitudeTarget = Quaternion::fromEuler(Vector(0, 0, currentYaw));
		ratesExtra = Vector(0, 0, 0);
		rollRatePID.reset();
		pitchRatePID.reset();
		yawRatePID.reset();
		rollPID.reset();
		pitchPID.reset();
		yawPID.reset();
		// cap thrust at hover, so a high-throttle start doesn't make the landing too long
		if (thrustTarget > ALTHOLD_HOVER_THRUST) thrustTarget = ALTHOLD_HOVER_THRUST;
		mode = AUTO;
	}
	// follow actual yaw each frame, so the yaw PID can't build up during the descent
	float currentYaw = attitude.getYaw();
	attitudeTarget = Quaternion::fromEuler(Vector(0, 0, currentYaw));

	thrustTarget -= dt / descendTime;
	if (thrustTarget < 0) {
		thrustTarget = 0;
		armed = false;
	}
}

// Allow pilot to interrupt automatic flight
void autoFailsafe() {
	static float roll, pitch, yaw, throttle;
	
	// control* covers SBUS, MAVLink and Web RC, so just check those
	if (roll != controlRoll || pitch != controlPitch || yaw != controlYaw || abs(throttle - controlThrottle) > 0.05) {
		if (mode == AUTO) mode = STAB;
	}
	
	roll = controlRoll;
	pitch = controlPitch;
	yaw = controlYaw;
	throttle = controlThrottle;
}

#if WEB_RC_ENABLED
// Web RC loss
void webRCLossFailsafe() {
	if (!webRCEnabled || !useWebRC) return;
	if (!armed) return;

	// compare milliseconds directly: integer division would add up to 1 s of error
	if (millis() - webRCLastUpdate > WEB_RC_LOSS_TIMEOUT_MS) {
		print("Web RC lost, descending\n");
		descend();
		webRCEnabled = false;
		useWebRC = false;
	}
}
#endif

// Upside-down protection: body Z more than 120 degrees from world Z for 1.5 s stops her
void invertedFailsafe() {
	if (!armed) {
		isInverted = false;
		return;
	}

	// body Z's world Z component: about +1 upright, -1 upside down
	Vector worldUp = Quaternion::rotateVector(Vector(0, 0, 1), attitude);

	static float invertedStartTime = 0;
	if (worldUp.z < INVERTED_COS_THRESHOLD) {
		isInverted = true;
		if (invertedStartTime == 0) invertedStartTime = t;
		if (t - invertedStartTime > INVERTED_TIMEOUT) {
			armed = false;
			thrustTarget = 0.0f;
			invertedStartTime = 0;
			print("Upside down: stopped\n");
		}
	} else {
		isInverted = false;
		invertedStartTime = 0;
	}
}

// Battery protection
// L1 (3.4 V): arming refused (in control.ino), automatic disarm at idle
// L2 (2.8 V): in flight, LED fast blink only
// L3 (2.6 V): in flight, automatic landing (descend(), speed from SF_DESCEND_TIME)
void batteryFailsafe() {
	static bool l3Latched = false;
	static float lowSince = 0.0f;
	static float l3LastNotify = 0.0f;

	if (batteryVoltage < VBAT_ABSENT_THRESHOLD) return; // no battery: ignore
	if (!armed) {
		l3Latched = false;
		lowSince = 0.0f;
		l3LastNotify = 0.0f;
		return;
	}

	// once L3 trips keep descending until disarmed, so it can't flicker at the threshold
	if (l3Latched) {
		if (t - l3LastNotify >= 2.0f) {
			print("Battery critical (%.2fV), still descending, thrust %.0f%%\n",
			      batteryVoltage, thrustTarget * 100.0f);
#if WEB_RC_ENABLED
			char warnBuf[64];
			snprintf(warnBuf, sizeof(warnBuf),
			         "Battery critical (%.2fV), descending, thrust %.0f%%",
			         batteryVoltage, thrustTarget * 100.0f);
			setWebRCWarn(warnBuf);
#endif
			l3LastNotify = t;
		}
		descend();
		return;
	}

	bool flying = thrustTarget >= BATTERY_FLYING_THRUST_MIN;

	bool actionCondition = false;
	bool criticalAction = false;

	// in flight: only L3 acts; L2 stays an LED warning
	if (flying) {
		if (batteryVoltage < VBAT_CRITICAL_THRESHOLD) {
			actionCondition = true;
			criticalAction = true;
		}
	} else {
		// armed idle: L1 disarms
		if (batteryVoltage < VBAT_WARN_THRESHOLD) {
			actionCondition = true;
		}
	}

	if (!actionCondition) {
		lowSince = 0.0f;
		return;
	}

	if (lowSince == 0.0f) lowSince = t;
	if (t - lowSince < BATTERY_ACTION_DEBOUNCE_TIME) return;

	lowSince = 0.0f;
	if (criticalAction) {
		l3Latched = true;
		l3LastNotify = 0.0f;
		print("Battery critical (%.2fV), automatic landing\n", batteryVoltage);
#if WEB_RC_ENABLED
		char warnBuf[64];
		snprintf(warnBuf, sizeof(warnBuf),
		         "Battery critical (%.2fV), automatic landing",
		         batteryVoltage);
		setWebRCWarn(warnBuf);
#endif
		descend();
		return;
	}

	armed = false;
	thrustTarget = 0.0f;
	print("Battery low (%.2fV), disarmed\n", batteryVoltage);
#if WEB_RC_ENABLED
	char warnBuf[64];
	snprintf(warnBuf, sizeof(warnBuf), "Battery low (%.2fV), disarmed", batteryVoltage);
	setWebRCWarn(warnBuf);
#endif
}
