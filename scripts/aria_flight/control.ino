// Flight control

#include "vector.h"
#include "quaternion.h"
#include "pid.h"
#include "lpf.h"
#include "util.h"

#if WEB_RC_ENABLED
#include "control.h"
#endif

// Parameters for a 118 mm wheelbase micro quad
// ============== Rate loop (inner) ==============
#define PITCHRATE_P 0.06 // higher P responds faster
#define PITCHRATE_I 0.1 // medium I evens out motor differences
#define PITCHRATE_D 0.001 // small D damps oscillation
#define PITCHRATE_I_LIM 0.3 // limits integral build-up
#define ROLLRATE_P PITCHRATE_P // roll and pitch share these
#define ROLLRATE_I PITCHRATE_I 
#define ROLLRATE_D PITCHRATE_D
#define ROLLRATE_I_LIM PITCHRATE_I_LIM
#define YAWRATE_P 0.3 // yaw needs a higher P (low inertia)
#define YAWRATE_I 0.01 // medium I
#define YAWRATE_D 0.01 // small D
#define YAWRATE_I_LIM 0.3
// ============== Angle loop (outer) ==============
#define ROLL_P 6 // higher P responds quickly
#define ROLL_I 0 // outer I term
#define ROLL_D 0 // the angle loop rarely needs D
#define ROLL_I_LIM radians(5.0f) // outer roll integral limit (rad/s), about 5 deg/s, so it can't run away at low throttle or on a mode change
#define PITCH_P ROLL_P // same as roll
#define PITCH_I ROLL_I
#define PITCH_D ROLL_D
#define PITCH_I_LIM ROLL_I_LIM
#define YAW_P 3 // yaw responds a little slower

// ============== Limits ==============
#define PITCHRATE_MAX radians(360) // high rate limit
#define ROLLRATE_MAX radians(360)
#define YAWRATE_MAX radians(300) // yaw a little lower
#define TILT_MAX radians(30) // maximum tilt 30 degrees
#define ALTHOLD_HOVER_THRUST 0.5f // altitude hold base thrust (stub until the barometer's in)
#define ARM_THROTTLE_LIMIT   0.05f // arming throttle limit (0-1), 5%: above it arming is refused
#define RATES_D_LPF_ALPHA 0.2 // cutoff frequency ~ 40 Hz

float motThrMin = 0.10f; // lowest thrust, at stick bottom: parameter MOT_THR_MIN
float motThrMax = 0.9f; // highest thrust, at stick top: parameter MOT_THR_MAX; keeps 10% for attitude correction

const int RAW = 0, ACRO = 1, STAB = 2, ALTHOLD = 3, AUTO = 4; // flight modes
int mode = STAB;
bool armed = false;
int flightModes[] = {STAB, STAB, STAB}; // flight modes on the RC mode switch's three positions: CTL_FLT_MODE_0/1/2

#if WEB_RC_ENABLED
extern uint16_t webRCButtons;
#endif

PID rollRatePID(ROLLRATE_P, ROLLRATE_I, ROLLRATE_D, ROLLRATE_I_LIM, RATES_D_LPF_ALPHA);
PID pitchRatePID(PITCHRATE_P, PITCHRATE_I, PITCHRATE_D, PITCHRATE_I_LIM, RATES_D_LPF_ALPHA);
PID yawRatePID(YAWRATE_P, YAWRATE_I, YAWRATE_D);
PID rollPID(ROLL_P, ROLL_I, ROLL_D, ROLL_I_LIM);
PID pitchPID(PITCH_P, PITCH_I, PITCH_D, PITCH_I_LIM);
PID yawPID(YAW_P, 0, 0);
Vector maxRate(ROLLRATE_MAX, PITCHRATE_MAX, YAWRATE_MAX);
float tiltMax = TILT_MAX;

Quaternion attitudeTarget;
Vector ratesTarget;
Vector ratesExtra; // feedforward rates
Vector torqueTarget;
float thrustTarget;

// ============== Software trim ==============
// Corrects a steady drift from mechanical asymmetry (centre of gravity, motor or propeller differences, IMU tilt).
// Units: radians.
//
// To adjust, from the serial or web console, no re-flash needed:
// p <name> <value>
// Steps of 0.005 rad (about 0.3 degrees) until she stops drifting with the sticks released.
// Stored in flash, kept over power-off.
//
// Drift direction and fix, with the sticks released:
// drifts left: p CTL_TRIM_ROLL +0.01 (positive, tilts right to cancel it)
// drifts right: p CTL_TRIM_ROLL -0.01 (negative, tilts left to cancel it)
// drifts forward: p CTL_TRIM_PITCH -0.01 (negative, nose up to cancel it)
// drifts back: p CTL_TRIM_PITCH +0.01 (positive, nose down to cancel it)
//
// Battery level and load can shift this slightly: retune now and then.
float trimRoll  = 0.0f; // roll trim (rad): CTL_TRIM_ROLL
float trimPitch = 0.0f; // pitch trim (rad): CTL_TRIM_PITCH

extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;
extern float motors[4];
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;
extern float batteryVoltage;  // battery.ino

void control() {
	interpretControls();
#if WEB_RC_ENABLED
	interpretWebRC();
#endif
	failsafe();
	controlAttitude();
	controlRates();
	controlTorque();
}

void interpretControls() {
	if (controlMode < 0.25) mode = flightModes[0];
	else if (controlMode <= 0.75) mode = flightModes[1];
	else if (controlMode > 0.75) mode = flightModes[2];

	if (mode == AUTO) return; // pilot is not effective in AUTO mode

#if WEB_RC_ENABLED
	if (!isUsingWebRC()) { // stick-gesture arming only while Web RC isn't active
#endif
	extern bool imuOK;
	static bool armWarnNotified = false; // so the refusal only prints once
	if (controlThrottle < 0.05 && controlYaw > 0.95) { // arm gesture
		if (!imuOK) {
			// IMU fault: arming refused
			if (!armWarnNotified) {
				print("IMU fault, arming refused!\n");
#if WEB_RC_ENABLED
				setWebRCWarn("IMU fault, arming refused");
#endif
				armWarnNotified = true;
			}
		} else if (batteryVoltage > VBAT_ABSENT_THRESHOLD && batteryVoltage < VBAT_WARN_THRESHOLD) {
			// L1 or below: arming refused, say so when it changes
			if (!armWarnNotified) {
				print("Battery low (%.2fV), arming refused\n", batteryVoltage);
#if WEB_RC_ENABLED
				char warnBuf[64];
				snprintf(warnBuf, sizeof(warnBuf), "Battery low (%.2fV), arming refused", batteryVoltage);
				setWebRCWarn(warnBuf);
#endif
				armWarnNotified = true;
			}
		} else {
			armed = true;
			armWarnNotified = false; // reset after a successful arm
		}
	}
	if (controlThrottle < 0.05 && controlYaw < -0.95) armed = false; // disarm gesture
#if WEB_RC_ENABLED
	}
#endif

	if (abs(controlYaw) < 0.1) controlYaw = 0; // yaw dead zone

	if (controlThrottle < 0.05f) {
		thrustTarget = 0.0f; // bottom dead zone: idle, PIDs not running
	} else {
		thrustTarget = mapf(controlThrottle, 0.05f, 1.0f, motThrMin, motThrMax);
	}

	if (mode == STAB) {
		float yawTarget = attitudeTarget.getYaw();
		if (!armed || invalid(yawTarget) || controlYaw != 0) yawTarget = attitude.getYaw(); // reset yaw target
		// trimRoll and trimPitch add to the stick command, for fixed drift
		// see the trim notes above, or CLI: p CTL_TRIM_ROLL / CTL_TRIM_PITCH
		attitudeTarget = Quaternion::fromEuler(Vector(controlRoll * tiltMax + trimRoll, controlPitch * tiltMax + trimPitch, yawTarget));
		ratesExtra = Vector(0, 0, -controlYaw * maxRate.z); // positive yaw stick means clockwise rotation in FLU
	}

	if (mode == ACRO) {
		attitudeTarget.invalidate(); // skip attitude control
		ratesTarget.x = controlRoll * maxRate.x;
		ratesTarget.y = controlPitch * maxRate.y;
		ratesTarget.z = -controlYaw * maxRate.z; // positive yaw stick means clockwise rotation in FLU
	}

	if (mode == RAW) { // direct torque control
		attitudeTarget.invalidate(); // skip attitude control
		ratesTarget.invalidate(); // skip rate control
		torqueTarget = Vector(controlRoll, controlPitch, -controlYaw) * 0.1;
	}
}

void controlAttitude() {
	if (!armed || attitudeTarget.invalid() || thrustTarget < motThrMin) return; // skip attitude control

	const Vector up(0, 0, 1);
	Vector upActual = Quaternion::rotateVector(up, attitude);
	Vector upTarget = Quaternion::rotateVector(up, attitudeTarget);

	Vector error = Vector::rotationVectorBetween(upTarget, upActual);

	ratesTarget.x = rollPID.update(error.x) + ratesExtra.x;
	ratesTarget.y = pitchPID.update(error.y) + ratesExtra.y;

	float yawError = wrapAngle(attitudeTarget.getYaw() - attitude.getYaw());
	ratesTarget.z = yawPID.update(yawError) + ratesExtra.z;
}


void controlRates() {
	if (!armed || ratesTarget.invalid() || thrustTarget < motThrMin) return; // skip rates control

	Vector error = ratesTarget - rates;

	// Calculate desired torque, where 0 - no torque, 1 - maximum possible torque
	torqueTarget.x = rollRatePID.update(error.x);
	torqueTarget.y = pitchRatePID.update(error.y);
	torqueTarget.z = yawRatePID.update(error.z);
}

void controlTorque() {
	if (!torqueTarget.valid()) return; // skip torque control

	if (!armed) {
		memset(motors, 0, sizeof(motors)); // stop motors if disarmed
		return;
	}

	if (thrustTarget < motThrMin) {
		for (int i = 0; i < 4; i++) motors[i] = motThrMin; // idle thrust
		return;
	}

	motors[MOTOR_FRONT_LEFT] = thrustTarget + torqueTarget.x - torqueTarget.y + torqueTarget.z;
	motors[MOTOR_FRONT_RIGHT] = thrustTarget - torqueTarget.x - torqueTarget.y - torqueTarget.z;
	motors[MOTOR_REAR_LEFT] = thrustTarget + torqueTarget.x + torqueTarget.y - torqueTarget.z;
	motors[MOTOR_REAR_RIGHT] = thrustTarget - torqueTarget.x + torqueTarget.y + torqueTarget.z;

	desaturate(motors[MOTOR_FRONT_LEFT], motors[MOTOR_FRONT_RIGHT], motors[MOTOR_REAR_LEFT], motors[MOTOR_REAR_RIGHT]);

	motors[0] = constrain(motors[0], 0, 1);
	motors[1] = constrain(motors[1], 0, 1);
	motors[2] = constrain(motors[2], 0, 1);
	motors[3] = constrain(motors[3], 0, 1);
}

void desaturate(float& a, float& b, float& c, float& d) {
	// avg is about thrustTarget (the torque parts sum to zero): keep avg, scale the torque differences
	float avg = (a + b + c + d) * 0.25f;
	float maxVal = max(max(a, b), max(c, d));
	float minVal = min(min(a, b), min(c, d));

	float scale = 1.0f;
	if (maxVal > 1.0f && maxVal > avg) {
		scale = min(scale, (1.0f - avg) / (maxVal - avg));
	}
	if (minVal < 0.0f && avg > minVal) {
		scale = min(scale, avg / (avg - minVal));
	}

	if (scale < 1.0f) {
		a = avg + (a - avg) * scale;
		b = avg + (b - avg) * scale;
		c = avg + (c - avg) * scale;
		d = avg + (d - avg) * scale;
	}
}

const char* getModeName() {
	switch (mode) {
		case RAW:     return "RAW";
		case ACRO:    return "ACRO";
		case STAB:    return "STAB";
		case ALTHOLD: return "ALTHOLD";
		case AUTO:    return "AUTO";
		default:      return "UNKNOWN";
	}
}

#if WEB_RC_ENABLED
// Web RC buttons (arming and mode changes only)
void interpretWebRC() {
	if (!isUsingWebRC()) return;

	// arm and disarm on the rising edge, so they don't repeat every cycle
	static uint16_t lastWebRCButtons = 0;
	uint16_t risingEdge = webRCButtons & ~lastWebRCButtons;
	lastWebRCButtons = webRCButtons;

	// log arming changes
	static bool lastArmedState = false;
	if (armed != lastArmedState) {
		print(armed ? "Web RC: armed\n" : "Web RC: disarmed\n");
		lastArmedState = armed;
	}

	// Button 0: arm (rising edge)
	if (risingEdge & 0x0001) {
		extern bool imuOK;
		extern char webRCWarnMsg[];
		if (!imuOK) {
			setWebRCWarn("IMU fault, arming refused");
		} else if (batteryVoltage > VBAT_ABSENT_THRESHOLD && batteryVoltage < VBAT_WARN_THRESHOLD) {
			// low battery: arming refused (same as interpretControls())
			char warnBuf[64];
			snprintf(warnBuf, sizeof(warnBuf), "Battery low (%.2fV), arming refused", batteryVoltage);
			setWebRCWarn(warnBuf);
		} else if (controlThrottle > ARM_THROTTLE_LIMIT) {
			setWebRCWarn("Throttle too high, can't arm");
		} else {
			armed = true;
			webRCWarnMsg[0] = '\0'; // armed: clear any old warning
		}
	}

	// Button 1: disarm (rising edge)
	if (risingEdge & 0x0002) {
		armed = false;
	}

	// Button 2: emergency stop (rising edge)
	if (risingEdge & 0x0004) {
		armed = false;
		thrustTarget = 0.0f;
	}

	// Button 6: STAB (rising edge)
	if (risingEdge & 0x0040) {
		mode = STAB;
	}

	// Button 7: ACRO (rising edge)
	if (risingEdge & 0x0080) {
		mode = ACRO;
	}

	// Button 8: ALTHOLD (rising edge), not implemented yet: goes to STAB so the page's mode cycle can't get stuck
	if (risingEdge & 0x0100) {
		mode = STAB;
		setWebRCWarn("Altitude hold isn't supported yet: switched to STAB");
	}

	// log mode changes
	static int lastMode = STAB;
	if (mode != lastMode) {
		print("Web RC: mode changed to %s\n", getModeName());
		lastMode = mode;
	}
}
#endif
