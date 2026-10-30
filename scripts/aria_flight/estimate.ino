// Attitude from the gyro and accelerometer
// Attitude estimation from gyro and accelerometer

#include "quaternion.h"
#include "vector.h"
#include "lpf.h"
#include "util.h"

float accWeight = 0.003;

// ============== Level correction, P term ==============
float levelWeight = 0; // P weight (off: it can't tell gyro drift from mechanical asymmetry and makes things worse)
float levelMaxTilt = radians(30); // rad, level correction fades out at this tilt angle (matches TILT_MAX)
// Stick gate threshold
// When the pilot's stick (the larger of roll and pitch, 0-1) passes this,
// applyLevel's weight fades to 0, so it can't couple with the PID integral and drift on release.
// To adjust:
// - lower: the gate acts sooner and hover drift correction weakens.
// - higher: applyLevel interferes longer while the sticks are moving.
// - recommended 0.2 (sensitive) to 0.4 (loose); default 0.2.
// - live from the CLI: p EST_LVL_GATE_THR <value>
float levelGateThreshold = 0.2f; // stick gate threshold: EST_LVL_GATE_THR

// ============== Mahony-style level correction, I term ==============
// levelBiasGain: how fast gravity error integrates into a virtual gyro bias.
// It's subtracted from the gyro in applyGyro() and never touches the attitude quaternion,
// so the PID loop doesn't see the correction and can't oscillate against its own I term.
// Higher converges faster but can overshoot; it settles in about 30 s.
// Parameter: EST_LVL_BIAS_GAIN
float levelBiasGain = 0; // Mahony I gain (off: it can't tell gyro drift from mechanical asymmetry and makes things worse)
Vector levelGyroBias(0, 0, 0); // virtual gyro bias estimated by applyLevel() (rad/s)

extern float controlRoll, controlPitch; // pilot's inputs, in aria_flight.ino
LowPassFilter<Vector> ratesFilter(0.2); // cutoff frequency ~ 40 Hz

void estimate() {
	applyGyro();
	applyAcc();
	applyLevel();
}

void applyGyro() {
	// Mahony I term: subtract the virtual bias from the gyro, then filter and integrate
	// so long-term level correction goes through the bias, unseen by the PID
	rates = ratesFilter.update(gyro - levelGyroBias);

	// apply rates to attitude
	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(rates * dt));
}

void applyAcc() {
	// test should we apply accelerometer gravity correction
	float accNorm = acc.norm();
	landed = !motorsActive() && abs(accNorm - ONE_G) < ONE_G * 0.1f;

	if (!landed) return;

	// calculate accelerometer correction
	Vector up = Quaternion::rotateVector(Vector(0, 0, 1), attitude);
	Vector correction = Vector::rotationVectorBetween(acc, up) * accWeight;

	// apply correction
	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(correction));
}

// Level correction (Mahony style: P corrects attitude directly, I integrates into a virtual gyro bias)
// How it works:
// - P (levelWeight): a small correction to attitude for quick response, halved to reduce kicks.
// - I (levelBiasGain): integrates the gravity error into levelGyroBias,
// subtracted from the gyro in applyGyro(). That path avoids the PID's error input
// and the inner-loop oscillation that correcting attitude directly used to cause.
void applyLevel() {
	if (landed) {
		levelGyroBias = Vector(0, 0, 0); // clear the bias on landing: relearn on the next flight
		return;
	}

	Vector up = Quaternion::rotateVector(Vector(0, 0, 1), attitude);
	float tilt = acos(constrain(up.z, -1.0f, 1.0f));

	// skip when nearly level: near zero tilt the cross product is all vibration noise
	if (tilt < radians(0.1f)) return;

	// P weight: smaller the nearer the tilt is to levelMaxTilt
	float dynamicWeight = levelWeight * constrain(1.0f - tilt / levelMaxTilt, 0.0f, 1.0f);

	// ---- Stick gate ----
	float stickDeflection = max(abs(controlRoll), abs(controlPitch));
	float stickGate = constrain(1.0f - stickDeflection / levelGateThreshold, 0.0f, 1.0f);
	dynamicWeight *= stickGate;
	// ------------------------------------

	// gravity error: the rotation between body Z and world Z
	Vector error = Vector::rotationVectorBetween(Vector(0, 0, 1), up);

	// P: correct attitude directly (tiny, keeps quick drift response)
	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(error * dynamicWeight));

	// I: integrate into the virtual gyro bias (Mahony style)
	// pause integrating while the sticks move (stickGate=0), so manoeuvres aren't learned as bias
	levelGyroBias += error * (levelBiasGain * stickGate);

	// limit: 3 deg/s of correction at most, so the bias can't run away
	float biasNorm = levelGyroBias.norm();
	const float biasLimit = radians(3.0f);
	if (biasNorm > biasLimit) levelGyroBias = levelGyroBias * (biasLimit / biasNorm);
}
