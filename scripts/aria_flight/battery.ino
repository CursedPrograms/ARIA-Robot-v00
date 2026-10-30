// Battery voltage
// Battery voltage monitoring

#include "board_config.h"

#define VBAT_ADC_PIN       BOARD_VBAT_ADC_PIN // battery sense pin
#define VBAT_ADC_SAMPLES   BOARD_VBAT_ADC_SAMPLES // ADC samples: C3 8 (less blocking), ESP32 and S3 16 (less noise)
#define VBAT_DIVIDER       (43.0f / 33.0f) // divider: 10k high side, 33k low side, so Vbat = Vadc * 43/33
#define VBAT_WARN_THRESHOLD     3.5f // L1: warning at rest or idle (V)
#define VBAT_LOW_THRESHOLD      3.3f // L2: low warning in flight (V)
#define VBAT_CRITICAL_THRESHOLD 3.0f // L3: critical in flight, automatic landing (V)
#define VBAT_ABSENT_THRESHOLD   0.5f // below this there's no battery: ignore all warnings

#define BATTERY_FLYING_THRUST_MIN    0.15f // thrust at or over this counts as flying, so L1 doesn't apply
#define BATTERY_ACTION_DEBOUNCE_TIME  0.9f // low voltage must last this long (s)

extern float t;
float batteryVoltage = 0.0f; // latest voltage, kept fresh by updateBatteryVoltage()

void updateBatteryVoltage() {
	static float lastCheck = 0;
	if (t - lastCheck < 0.5f) return; // sample every 0.5 s
	lastCheck = t;
	batteryVoltage = readBatteryVoltage();
}

float readBatteryVoltage() {
	long sum = 0;
	for (int i = 0; i < VBAT_ADC_SAMPLES; i++) sum += analogReadMilliVolts(VBAT_ADC_PIN);
	return (sum / (float)VBAT_ADC_SAMPLES / 1000.0f) * VBAT_DIVIDER;
}
