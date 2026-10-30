// Parameters stored in flash
// Parameters storage in flash memory

#include <Preferences.h>
#include <nvs_flash.h> // nvs_flash_init()/nvs_flash_erase(): rebuild NVS if Preferences.begin() fails
#include "util.h"

extern float channelZero[16];
extern float channelMax[16];
extern float rollChannel, pitchChannel, throttleChannel, yawChannel, modeChannel;
extern int wifiMode, udpLocalPort, udpRemotePort;
extern int motorPins[4];
extern int pwmFrequency, pwmResolution, pwmStop, pwmMin, pwmMax;
extern float motThrMin;
extern float motThrMax;
extern int mavlinkSysId;
extern Rate telemetrySlow, telemetryFast;
extern float rcLossTimeout, descendTime;
extern int flightModes[3];
extern Vector accBias, accScale;
extern Vector imuRotation;
extern LowPassFilter<Vector> gyroBiasFilter;
extern int rcRxPin;
extern int rcProtocol; // RC protocol: 0=SBUS, 1=CRSF(ELRS)
extern int rcTxPin; // telemetry TX pin, -1 off
extern int rcBaud; // serial baud
extern float trimRoll, trimPitch; // software trim, in control.ino
extern float levelGateThreshold; // stick gate threshold, in estimate.ino
extern float levelBiasGain; // Mahony I gain, in estimate.ino
extern Vector levelGyroBias; // Mahony virtual gyro bias, in estimate.ino

Preferences storage;

struct Parameter {
	const char *name; // max length is 15 (Preferences key limit)
	bool integer;
	union { float *f; int *i; };
	float cache; // what's stored in flash
	void (*callback)(); // called after parameter change
	Parameter(const char *name, float *variable, void (*callback)() = nullptr) : name(name), integer(false), f(variable), cache(0), callback(callback) {};
	Parameter(const char *name, int *variable, void (*callback)() = nullptr) : name(name), integer(true), i(variable), cache(0), callback(callback) {};
	float getValue() const { return integer ? *i : *f; }
	void setValue(const float value) { if (integer) *i = (int)value; else *f = value; }
};

Parameter parameters[] = {
	// ===== Control (rate PID, inner) =====
	// The rate loop controls how fast she turns: the innermost loop.
	// P: gain. Higher responds faster, too high oscillates.
	// I: removes steady error (motor differences), too high saturates.
	// D: damps overshoot, too high amplifies noise.
	// WU (windup): limits how far I can build up.
	{"CTL_R_RATE_P",  &rollRatePID.p}, // roll rate P (rad/s to torque)
	{"CTL_R_RATE_I",  &rollRatePID.i}, // roll rate I
	{"CTL_R_RATE_D",  &rollRatePID.d}, // roll rate D
	{"CTL_R_RATE_WU", &rollRatePID.windup}, // roll rate integral limit
	{"CTL_P_RATE_P",  &pitchRatePID.p}, // pitch rate P
	{"CTL_P_RATE_I",  &pitchRatePID.i}, // pitch rate I
	{"CTL_P_RATE_D",  &pitchRatePID.d}, // pitch rate D
	{"CTL_P_RATE_WU", &pitchRatePID.windup}, // pitch rate integral limit
	{"CTL_Y_RATE_P",  &yawRatePID.p}, // yaw rate P (low inertia, usually higher)
	{"CTL_Y_RATE_I",  &yawRatePID.i}, // yaw rate I
	{"CTL_Y_RATE_D",  &yawRatePID.d}, // yaw rate D

	// ===== Control (angle PID, outer) =====
	// Turns attitude error into a rate command for the inner loop.
	// I corrects a fixed hover offset from asymmetry; it needs WU set too.
	{"CTL_R_P",  &rollPID.p}, // roll angle P (rad of error to rad/s)
	{"CTL_R_I",  &rollPID.i}, // roll angle I, for fixed asymmetry (default 0.5)
	{"CTL_R_D",  &rollPID.d}, // roll angle D (usually 0)
	{"CTL_R_WU", &rollPID.windup}, // roll angle integral limit (rad/s): set it whenever I isn't 0
	{"CTL_P_P",  &pitchPID.p}, // pitch angle P
	{"CTL_P_I",  &pitchPID.i}, // pitch angle I (default 0.5)
	{"CTL_P_D",  &pitchPID.d}, // pitch angle D (usually 0)
	{"CTL_P_WU", &pitchPID.windup}, // pitch angle integral limit (rad/s)
	{"CTL_Y_P",  &yawPID.p}, // yaw angle P

	// ===== Control (limits) =====
	{"CTL_P_RATE_MAX", &maxRate.y}, // maximum pitch rate (rad/s): stick commands are cut here
	{"CTL_R_RATE_MAX", &maxRate.x}, // maximum roll rate (rad/s)
	{"CTL_Y_RATE_MAX", &maxRate.z}, // maximum yaw rate (rad/s)
	{"CTL_TILT_MAX",   &tiltMax}, // maximum tilt in STAB (rad)

	// ===== Control (software trim) =====
	// Corrects hover drift from centre of gravity or motor differences without touching her.
	{"CTL_TRIM_ROLL",  &trimRoll}, // roll trim (rad), positive rolls right, steps of 0.005
	{"CTL_TRIM_PITCH", &trimPitch}, // pitch trim (rad), positive tips back, steps of 0.005

	// ===== Control (flight modes) =====
	// The RC mode channel maps the switch's three positions to these modes:
	// 0=RAW, 1=ACRO, 2=STAB, 3=ALTHOLD, 4=AUTO
	{"CTL_FLT_MODE_0", &flightModes[0]}, // mode for switch position 0
	{"CTL_FLT_MODE_1", &flightModes[1]}, // mode for switch position 1
	{"CTL_FLT_MODE_2", &flightModes[2]}, // mode for switch position 2

	// ===== IMU (mounting rotation) =====
	// If the IMU isn't lined up with her body, set the rotation here (rad).
	{"IMU_ROT_ROLL",  &imuRotation.x}, // IMU rotation about body X (roll axis, rad)
	{"IMU_ROT_PITCH", &imuRotation.y}, // IMU rotation about body Y (pitch axis, rad)
	{"IMU_ROT_YAW",   &imuRotation.z}, // IMU rotation about body Z (yaw axis, rad)

	// ===== IMU (gyro bias filter) =====
	// Gyro bias is estimated at power-up by a low-pass filter.
	// Smaller alpha filters harder and settles slower.
	{"IMU_GYRO_BIAS_A", &gyroBiasFilter.alpha}, // gyro bias filter alpha (0-1)

	// ===== IMU (accelerometer calibration) =====
	// Run the six-side calibration after building or remounting. Results go here.
	// acc_corrected = (acc_raw - bias) * scale
	{"IMU_ACC_BIAS_X", &accBias.x}, // accelerometer X bias (m/s^2)
	{"IMU_ACC_BIAS_Y", &accBias.y}, // accelerometer Y bias (m/s^2)
	{"IMU_ACC_BIAS_Z", &accBias.z}, // accelerometer Z bias (m/s^2)
	{"IMU_ACC_SCALE_X", &accScale.x}, // accelerometer X scale (ideally 1.0)
	{"IMU_ACC_SCALE_Y", &accScale.y}, // accelerometer Y scale
	{"IMU_ACC_SCALE_Z", &accScale.z}, // accelerometer Z scale

	// ===== Attitude estimate =====
	// Complementary filter: gyro for fast motion, accelerometer for gravity.
	{"EST_ACC_WEIGHT",   &accWeight}, // accelerometer weight (0-1): higher corrects harder but feels vibration more
	{"EST_RATES_LPF_A",  &ratesFilter.alpha}, // rate filter alpha (0-1): lower cuts lower (about 40 Hz at 0.2)
	{"EST_LVL_GATE_THR", &levelGateThreshold}, // stick gate threshold (0-1): past it applyLevel's weight fades out
	{"EST_LVL_BIAS_GAIN",&levelBiasGain}, // Mahony I gain: higher converges faster (about 30 s at 0.00002)

	// ===== Motors (pins) =====
	// A change re-runs setupMotors(), no reboot needed.
	{"MOT_PIN_FL", &motorPins[MOTOR_FRONT_LEFT],  setupMotors}, // front left motor GPIO
	{"MOT_PIN_FR", &motorPins[MOTOR_FRONT_RIGHT], setupMotors}, // front right motor GPIO
	{"MOT_PIN_RL", &motorPins[MOTOR_REAR_LEFT],   setupMotors}, // rear left motor GPIO
	{"MOT_PIN_RR", &motorPins[MOTOR_REAR_RIGHT],  setupMotors}, // rear right motor GPIO

	// ===== Motors (PWM) =====
	// MOSFETs: pwmMax=-1, pwmStop and pwmMin=0, pure duty cycle.
	// ESCs: pwmMax the top pulse (microseconds), pwmMin the bottom, pwmFrequency 400.
	{"MOT_PWM_FREQ", &pwmFrequency, setupMotors}, // PWM frequency (Hz): 25000 for MOSFETs, 400 for ESCs
	{"MOT_PWM_RES",  &pwmResolution, setupMotors}, // PWM resolution (bits), usually 10
	{"MOT_PWM_STOP", &pwmStop}, // PWM when stopped: 0 for duty, idle pulse for ESCs
	{"MOT_PWM_MIN",  &pwmMin}, // lowest PWM: 0 for duty, lowest pulse for ESCs
	{"MOT_PWM_MAX",  &pwmMax}, // highest PWM: -1 is duty (MOSFETs), a positive value is the ESC's top pulse

	// ===== Motors (thrust mapping) =====
	// Maps throttle (0-1) onto motor output, leaving room for attitude correction.
	{"MOT_THR_MIN", &motThrMin}, // lowest thrust (0-1), at or above 0.05 so motors don't stall
	{"MOT_THR_MAX", &motThrMax}, // highest thrust (0-1), at or below 0.9 to leave correction room

	// ===== RC (channel calibration) =====
	// Raw RC values are calibrated to -1..1 or 0..1.
	// channelZero[n]: raw neutral; channelMax[n]: raw full travel.
	// normalized = (raw - zero) / max
	{"RC_ZERO_0", &channelZero[0]}, // channel 0 neutral
	{"RC_ZERO_1", &channelZero[1]}, // channel 1 neutral
	{"RC_ZERO_2", &channelZero[2]}, // channel 2 neutral
	{"RC_ZERO_3", &channelZero[3]}, // channel 3 neutral
	{"RC_ZERO_4", &channelZero[4]}, // channel 4 neutral
	{"RC_ZERO_5", &channelZero[5]}, // channel 5 neutral
	{"RC_ZERO_6", &channelZero[6]}, // channel 6 neutral
	{"RC_ZERO_7", &channelZero[7]}, // channel 7 neutral
	{"RC_MAX_0",  &channelMax[0]}, // channel 0 travel
	{"RC_MAX_1",  &channelMax[1]}, // channel 1 travel
	{"RC_MAX_2",  &channelMax[2]}, // channel 2 travel
	{"RC_MAX_3",  &channelMax[3]}, // channel 3 travel
	{"RC_MAX_4",  &channelMax[4]}, // channel 4 travel
	{"RC_MAX_5",  &channelMax[5]}, // channel 5 travel
	{"RC_MAX_6",  &channelMax[6]}, // channel 6 travel
	{"RC_MAX_7",  &channelMax[7]}, // channel 7 travel

	// ===== RC (channel mapping) =====
	// Which physical channel (from 0) each axis uses.
	{"RC_ROLL",     &rollChannel}, // roll channel
	{"RC_PITCH",    &pitchChannel}, // pitch channel
	{"RC_THROTTLE", &throttleChannel}, // throttle channel
	{"RC_YAW",      &yawChannel}, // yaw channel
	{"RC_MODE",     &modeChannel}, // flight mode channel (three-position switch)

	// ===== RC (serial hardware) =====
	// All four re-run setupRC, no reboot needed.
	{"RC_RX_PIN",   &rcRxPin,    setupRC}, // receiver data GPIO (UART RX)
	{"RC_PROTOCOL", &rcProtocol, setupRC}, // RC protocol: 0=SBUS (inverted), 1=CRSF/ELRS (420000 baud)
	{"RC_TX_PIN",   &rcTxPin,    setupRC}, // telemetry GPIO (UART TX), -1 off
	{"RC_BAUD",     &rcBaud,     setupRC}, // RC serial baud: SBUS 100000, CRSF 420000

	// ===== WiFi =====
	{"WIFI_MODE",     &wifiMode}, // WiFi mode: 0 off, 1 access point, 2 client
	{"WIFI_LOC_PORT", &udpLocalPort}, // local UDP port (ground stations send here)
	{"WIFI_REM_PORT", &udpRemotePort}, // remote UDP port (she sends here)

	// ===== MAVLink telemetry =====
	{"MAV_SYS_ID",    &mavlinkSysId}, // MAVLink system ID (1-254), unique per drone
	{"MAV_RATE_SLOW", &telemetrySlow.rate}, // slow telemetry rate (Hz): heartbeat, battery
	{"MAV_RATE_FAST", &telemetryFast.rate}, // fast telemetry rate (Hz): attitude, rates

	// ===== Fail-safe =====
	{"SF_RC_LOSS_TIME",  &rcLossTimeout}, // RC loss timeout (s), then automatic descent
	{"SF_DESCEND_TIME",  &descendTime}, // seconds to descend to zero thrust: lower is faster
};

void setupParameters() {
	print("Setup parameters\n");
	if (!storage.begin("flix", false)) {
		// nvs_flash_init() inside Preferences.begin() returns false after a partition change, an NVS version mismatch or NVS damaged by power loss.
		// The ESP-IDF answer is to erase the partition and retry; otherwise it fails silently for ever and nothing is stored.
		print("[NVS] Preferences.begin failed, erasing NVS and retrying...\n");
		nvs_flash_erase();
		esp_err_t err = nvs_flash_init();
		print("[NVS] nvs_flash_init retry result: err=%d\n", err);
		if (!storage.begin("flix", false)) {
			print("[NVS] Second start failed too: parameters won't be stored, check the partition table and hardware!\n");
		} else {
			print("[NVS] Rebuilt: parameters are back to defaults (old data's gone, run ca and cr again)\n");
		}
	}
	// Read parameters from storage
	for (auto &parameter : parameters) {
		if (!storage.isKey(parameter.name)) {
			storage.putFloat(parameter.name, parameter.getValue()); // store default value
		}
		float stored = storage.getFloat(parameter.name, parameter.getValue());
		// a NaN on an integer parameter means bad flash: go back to the default and rewrite it
		if (parameter.integer && !isfinite(stored)) {
			stored = (float)parameter.getValue();
			storage.putFloat(parameter.name, stored);
		}
		parameter.setValue(stored);
		parameter.cache = parameter.getValue();
	}
}

int parametersCount() {
	return sizeof(parameters) / sizeof(parameters[0]);
}

const char *getParameterName(int index) {
	if (index < 0 || index >= parametersCount()) return "";
	return parameters[index].name;
}

float getParameter(int index) {
	if (index < 0 || index >= parametersCount()) return NAN;
	return parameters[index].getValue();
}

float getParameter(const char *name) {
	for (auto &parameter : parameters) {
		if (strcasecmp(parameter.name, name) == 0) {
			return parameter.getValue();
		}
	}
	return NAN;
}

bool setParameter(const char *name, const float value) {
	for (auto &parameter : parameters) {
		if (strcasecmp(parameter.name, name) == 0) {
			if (parameter.integer && !isfinite(value)) return false; // can't set integer to NaN or Inf
			parameter.setValue(value);
			if (parameter.callback) parameter.callback();
			return true;
		}
	}
	return false;
}

void syncParameters() {
	static Rate rate(1);
	if (!rate) return; // sync once per second
	if (motorsActive()) return; // don't use flash while flying, it may cause a delay

	for (auto &parameter : parameters) {
		if (parameter.getValue() == parameter.cache) continue;
		if (isnan(parameter.getValue()) && isnan(parameter.cache)) continue; // handle NAN != NAN
		size_t written = storage.putFloat(parameter.name, parameter.getValue());
		if (written != sizeof(float)) continue; // on a failed write keep the old cache: the next 1 Hz pass tries again
		parameter.cache = parameter.getValue();
	}
}

void printParameters() {
	for (auto &parameter : parameters) {
		print("%s = %g\n", parameter.name, parameter.getValue());
	}
}

void resetParameters() {
	storage.clear();
	ESP.restart();
}
