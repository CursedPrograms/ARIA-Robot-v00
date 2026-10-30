// Gyro and accelerometer
// Work with the IMU sensor

#include <SPI.h>
#include "drivers/MPU9250.h"
#include "vector.h"
#include "lpf.h"
#include "util.h"
#include "board_config.h"

MPU9250 imu(SPI, BOARD_SPI_CS);

bool imuOK = false; // did the IMU start? false refuses arming and readIMU() skips waiting

// IMU mounting (Euler angles, rad). The default (0, 0, -PI/2) is this PCB's:
// chip face up, X silk to her right, Y silk to her front: Vector(data.y, -data.x, data.z)
// for another mounting, change this and run `preset` to reset stored parameters.
Vector imuRotation(0, 0, -PI / 2);

Vector accBias;
Vector accScale(1, 1, 1);
Vector gyroBias;
LowPassFilter<Vector> gyroBiasFilter(0.001); // low-pass filter for the gyro bias estimate

void setupIMU() {
	print("Setup IMU\n");
	SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI, BOARD_SPI_CS);
	if (!imu.begin()) {
		print("IMU FAILED TO START! Check the IMU wiring!\n");
		return; // don't call configureIMU() or start the timer interrupt: imuOK stays false
	}
	imuOK = true;
	configureIMU();
}

void configureIMU() {
	imu.setAccelRange(imu.ACCEL_RANGE_4G);
	imu.setGyroRange(imu.GYRO_RANGE_2000DPS);
	imu.setDLPF(imu.DLPF_MAX);
	imu.setRate(imu.RATE_1KHZ_APPROX);
	imu.setupInterrupt();
}

void readIMU() {
	if (!imuOK) return; // skip on an IMU fault: gyro and acc stay zero, the main loop keeps running
	imu.waitForData();
	imu.getGyro(gyro.x, gyro.y, gyro.z);
	imu.getAccel(acc.x, acc.y, acc.z);
	calibrateGyroOnce();
	// apply scale and bias
	acc = (acc - accBias) / accScale;
	gyro = gyro - gyroBias;
	// rotate to body frame using imuRotation Euler angles
	// rotation quaternion cache: imuRotation only changes with a parameter,
	// so each frame costs 3 float compares instead of 6 sinf/cosf calls.
	// the first readIMU() after a parameter change rebuilds it, one frame late (about 1 ms).
	static Quaternion _imuRotQuat;
	static float _cachedRotX = NAN, _cachedRotY = NAN, _cachedRotZ = NAN;
	if (imuRotation.x != _cachedRotX || imuRotation.y != _cachedRotY || imuRotation.z != _cachedRotZ) {
		_imuRotQuat = Quaternion::fromEuler(imuRotation).inversed();
		_cachedRotX = imuRotation.x;
		_cachedRotY = imuRotation.y;
		_cachedRotZ = imuRotation.z;
	}
	acc  = Quaternion::rotateVector(acc,  _imuRotQuat);
	gyro = Quaternion::rotateVector(gyro, _imuRotQuat);
}

void calibrateGyroOnce() {
	static Delay landedDelay(2);
	if (!landedDelay.update(landed)) return; // calibrate only if definitely stationary

	gyroBias = gyroBiasFilter.update(gyro);
}

void calibrateAccel() {
	print("Calibrating accelerometer\n");
	imu.setAccelRange(imu.ACCEL_RANGE_2G); // the most sensitive mode

	print("1/6 Level: nose forward, bottom down, on a flat surface, completely level. Keep still, calibration starts in 8 s\n");
	pause(8);
	calibrateAccelOnce();
	print("Level done. Carry on.\n");
	pause(1);
	print("2/6 Nose up: point the nose at the sky, tail on the surface, vertical. Keep still, calibration starts in 8 s\n");
	pause(8);
	calibrateAccelOnce();
	print("Nose up done. Carry on.\n");
	pause(1);
	print("3/6 Nose down: point the nose at the ground on the surface, tail up, vertical. Keep still, calibration starts in 8 s\n");
	pause(8);
	calibrateAccelOnce();
	print("Nose down done. Carry on.\n");
	pause(1);
	print("4/6 Right side down: right side on the surface, left arms up, vertical. Keep still, calibration starts in 8 s\n");
	pause(8);
	calibrateAccelOnce();
	print("Right side done. Carry on.\n");
	pause(1);
	print("5/6 Left side down: left side on the surface, right arms up, vertical. Keep still, calibration starts in 8 s\n");
	pause(8);
	calibrateAccelOnce();
	print("Left side done. Carry on.\n");
	pause(1);
	print("6/6 Upside down: turn her right over, top down, bottom up, level. Keep still, calibration starts in 8 s\n");
	pause(8);
	calibrateAccelOnce();

	printIMUCalibration();
	print("All calibration done! Set her upright and run ps to check it.\n");
	configureIMU();
}

void calibrateAccelOnce() {
	const int samples = 1000;
	static Vector accMax(-INFINITY, -INFINITY, -INFINITY);
	static Vector accMin(INFINITY, INFINITY, INFINITY);

	// Compute the average of the accelerometer readings
	acc = Vector(0, 0, 0);
	for (int i = 0; i < samples; i++) {
		imu.waitForData();
		Vector sample;
		imu.getAccel(sample.x, sample.y, sample.z);
		acc = acc + sample;

	#ifdef CONFIG_IDF_TARGET_ESP32C3
		// give up 1 ms every 10 frames, so on the one-core C3 WiFi isn't starved and beacons aren't lost
		if (i % 10 == 0) delay(1);
	#endif
	
	}
	acc = acc / samples;

	// Update the maximum and minimum values
	if (acc.x > accMax.x) accMax.x = acc.x;
	if (acc.y > accMax.y) accMax.y = acc.y;
	if (acc.z > accMax.z) accMax.z = acc.z;
	if (acc.x < accMin.x) accMin.x = acc.x;
	if (acc.y < accMin.y) accMin.y = acc.y;
	if (acc.z < accMin.z) accMin.z = acc.z;
	// Compute scale and bias
	accScale = (accMax - accMin) / 2 / ONE_G;
	accBias = (accMax + accMin) / 2;
}

void printIMUCalibration() {
	print("gyro bias: %f %f %f\n", gyroBias.x, gyroBias.y, gyroBias.z);
	print("accel bias: %f %f %f\n", accBias.x, accBias.y, accBias.z);
	print("accel scale: %f %f %f\n", accScale.x, accScale.y, accScale.z);
}

void printIMUInfo() {
	imu.status() ? print("status: ERROR %d\n", imu.status()) : print("status: OK\n");
	print("model: %s\n", imu.getModel());
	print("who am I: 0x%02X\n", imu.whoAmI());
	print("rate: %.0f\n", loopRate);
	print("gyro: %f %f %f\n", gyro.x, gyro.y, gyro.z);
	print("acc: %f %f %f\n", acc.x, acc.y, acc.z);
	imu.waitForData();
	Vector rawGyro, rawAcc;
	imu.getGyro(rawGyro.x, rawGyro.y, rawGyro.z);
	imu.getAccel(rawAcc.x, rawAcc.y, rawAcc.z);
	print("raw gyro: %f %f %f\n", rawGyro.x, rawGyro.y, rawGyro.z);
	print("raw acc: %f %f %f\n", rawAcc.x, rawAcc.y, rawAcc.z);
}
