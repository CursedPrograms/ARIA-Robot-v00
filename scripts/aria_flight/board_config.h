#pragma once

// Board hardware configuration
// Board-level hardware configuration
// ESP32, C3 and S3 are told apart at compile time: nothing to set by hand

#ifdef CONFIG_IDF_TARGET_ESP32C3
// ---------------------- ESP32C3 ------------------------- 

// ---- Motor pins (array order RL, RR, FR, FL, as the constants in motors.ino) ----
// GPIO 0/1/3/10 have no pull-ups and aren't strapping pins (the C3's are 2, 8, 9)
#define BOARD_MOTOR_PINS   {3, 10, 0, 1}    // RL=GPIO3, RR=GPIO10, FR=GPIO0, FL=GPIO1

// ---- Battery ADC ----
// GPIO 2 is a strapping pin with a weak pull-up, which doesn't affect ADC readings
// strapping is sampled at boot, so analogRead() sees a normal input
#define BOARD_VBAT_ADC_PIN 2 // ADC1_CH2 (GPIO 2)

// ---- RC serial (CRSF/ELRS by default; RC_PROTOCOL=0 for SBUS) ----
// GPIO 9 as CRSF RX: standard levels, no inverter
#define BOARD_RC_SERIAL    Serial1
#define BOARD_RC_RX_PIN    9
#define BOARD_RC_TX_PIN    -1 // telemetry TX, -1 is off
#define BOARD_RC_PROTOCOL  1           // 0=SBUS, 1=CRSF(ELRS)
#define BOARD_RC_BAUD      420000 // CRSF 420000 by default, SBUS 100000

// ---- IMU SPI (the Arduino pins_arduino.h defaults) ----
// matches the Arduino C3 SPI defaults, so SPI.begin() works with no arguments
#define BOARD_SPI_SCK      4 // SCK = GPIO 4
#define BOARD_SPI_MISO     5 // MISO = GPIO 5
#define BOARD_SPI_MOSI     6 // MOSI = GPIO 6
#define BOARD_SPI_CS       7 // SS = GPIO 7

// ---- I2C (C3 defaults) ----
#define BOARD_I2C_SDA          (-1) // GPIO 8, shared with the LED: don't use both
#define BOARD_I2C_SCL          (-1)        // GPIO9

// ---- LED: GPIO 8, blue, active low ----
#define BOARD_LED_ENABLED  1
#define BOARD_LED_PIN      8
#define BOARD_LED_INVERTED 1 // on when low

// ---- Performance and resources (C3, light) ----
#define BOARD_VBAT_ADC_SAMPLES       8 // ADC samples, less blocking (ESP32/S3: 16)
#define BOARD_LOG_DURATION           4 // 4 s log, saves about 22 KB of RAM (ESP32/S3: 8 s)
#define BOARD_CONSOLE_LINES          20 // console lines, saves about 7 KB of RAM (ESP32/S3: 50)
#define BOARD_CONSOLE_LINE_LEN       160 // characters a line (ESP32/S3: 240)
#define BOARD_MAVLINK_TELEM_FAST_HZ  5 // MAVLink fast telemetry, slower to load WiFi less (ESP32/S3: 10 Hz)
#define BOARD_WIFI_ENABLED           0 // C3 has one core: WiFi off leaves it all to flight
#define BOARD_WEB_RC_ENABLED         0 // Web RC needs WiFi, so it's off too

// ---- Expansion board sensors ----
#undef  EXPANSION_BOARD_ENABLED
#define EXPANSION_BOARD_ENABLED      0 // pins clash on the C3: expansion board forced off
#define BOARD_VL53_XSHUT_PIN   (-1) // VL53L1X XSHUT, provisional GPIO 10
#define BOARD_PMW_CS_PIN       (-1) // PMW3901 CS, none on the C3: -1 disables it

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
// ---------------------- ESP32S3 -------------------------

// ---- Motor pins (from an S3 schematic in RayCyder/CF-Drone: PWM3/2/1/4, AO3416 low-side N-MOS) ----
// ARIA: songge8's S3 map is {4, 5, 6, 7}. If the motors don't turn, find them with aria_pin_finder and set MOT_PIN_*
#define BOARD_MOTOR_PINS   {2, 3, 6, 8}    // RL=GPIO2, RR=GPIO3, FR=GPIO6, FL=GPIO8

// ---- Battery ADC: GPIO 1 (ADC1_CH0; the S3's ADC1 is GPIO 1-10) ----
#define BOARD_VBAT_ADC_PIN 1

// ---- RC serial (CRSF/ELRS by default): IO4 on the schematic ----
#define BOARD_RC_SERIAL    Serial2
#define BOARD_RC_RX_PIN    4
#define BOARD_RC_TX_PIN    -1
#define BOARD_RC_PROTOCOL  1
#define BOARD_RC_BAUD      420000

// ---- IMU SPI: the S3's FSPI default pins ----
// SCK 12, MISO 13, MOSI 11, CS 10 (all default)
#define BOARD_SPI_SCK      12
#define BOARD_SPI_MISO     13
#define BOARD_SPI_MOSI     11
#define BOARD_SPI_CS       10

// ---- I2C (S3 defaults) ----
#define BOARD_I2C_SDA          35          // GPIO35
#define BOARD_I2C_SCL          36          // GPIO36

// ---- LED: none on this schematic, and GPIO 2 is the rear left motor: never drive it as an LED ----
#define BOARD_LED_ENABLED  0
#define BOARD_LED_PIN      (-1)
#define BOARD_LED_INVERTED 0

// ---- Performance and resources (S3) ----
#define BOARD_VBAT_ADC_SAMPLES       16 // ADC samples
#define BOARD_LOG_DURATION           4 // log seconds
#define BOARD_CONSOLE_LINES          30 // console lines
#define BOARD_CONSOLE_LINE_LEN       240 // characters a line
#define BOARD_MAVLINK_TELEM_FAST_HZ  10 // MAVLink fast telemetry
#define BOARD_WIFI_ENABLED           1
#define BOARD_WEB_RC_ENABLED         1

// ---- Expansion board sensors ----
#define EXPANSION_BOARD_ENABLED      1 // 1 = probe expansion sensors (BMP388/VL53L1X/QMC5883L/PMW3901); 0 = skip, all capability flags stay false and basic flight is unaffected
#define BOARD_VL53_XSHUT_PIN   37 // VL53L1X XSHUT, provisional GPIO 37
#define BOARD_PMW_CS_PIN       38 // PMW3901 CS, provisional GPIO 38

#else // ---- Plain ESP32 ----
// ---------------------- Plain ESP32 -------------------------

#define BOARD_MOTOR_PINS   {12, 13, 15, 14} // motor pins RL=GPIO12, RR=GPIO13, FR=GPIO15, FL=GPIO14

#define BOARD_VBAT_ADC_PIN 36 // battery ADC pin (GPIO 36, input only)

#define BOARD_RC_SERIAL    Serial2 // RC serial (CRSF/ELRS by default)
#define BOARD_RC_RX_PIN    4 // RC RX pin (GPIO 4)
#define BOARD_RC_TX_PIN    -1 // telemetry TX, -1 is off
#define BOARD_RC_PROTOCOL  1               // 0=SBUS, 1=CRSF(ELRS)
#define BOARD_RC_BAUD      420000 // CRSF 420000 by default, SBUS 100000

#define BOARD_SPI_SCK      18 // SPI clock (GPIO 18)
#define BOARD_SPI_MISO     19 // SPI MISO (GPIO 19)
#define BOARD_SPI_MOSI     23 // SPI MOSI (GPIO 23)
#define BOARD_SPI_CS       5 // SPI chip select (GPIO 5)

#define BOARD_I2C_SDA      21 // GPIO 21 (ESP32 I2C SDA default)
#define BOARD_I2C_SCL      22 // GPIO 22 (ESP32 I2C SCL default)

#define BOARD_LED_ENABLED  1 // onboard LED on
#define BOARD_LED_PIN      2 // LED pin (GPIO 2)
#define BOARD_LED_INVERTED 0 // on when high

// ---- Performance and resources (ESP32) ----
#define BOARD_VBAT_ADC_SAMPLES       16 // ADC samples
#define BOARD_LOG_DURATION           4 // log seconds
#define BOARD_CONSOLE_LINES          30 // console lines
#define BOARD_CONSOLE_LINE_LEN       240 // characters a line
#define BOARD_MAVLINK_TELEM_FAST_HZ  10 // MAVLink fast telemetry
#define BOARD_WIFI_ENABLED           1 // WiFi switch
#define BOARD_WEB_RC_ENABLED         1 // Web RC switch

// ---- Expansion board sensors ----
#define EXPANSION_BOARD_ENABLED      1 // 1 = probe expansion sensors (BMP388/VL53L1X/QMC5883L/PMW3901); 0 = skip, all capability flags stay false and basic flight is unaffected
#define BOARD_VL53_XSHUT_PIN   32 // VL53L1X XSHUT (GPIO 32)
#define BOARD_PMW_CS_PIN       33 // PMW3901 SPI CS (GPIO 33)

#endif

// ---- Fallback (unknown chip: everything on) ----

// ---- The expansion board switch ----
// 1 = probe expansion sensors (BMP388/VL53L1X/QMC5883L/PMW3901)
// 0 = skip, all capability flags stay false and basic flight is unaffected
#if !defined(EXPANSION_BOARD_ENABLED)
#define EXPANSION_BOARD_ENABLED 1 // fallback: on for an unknown chip
#endif

// ---- WiFi and Web RC fallback (unknown chip: on) ----
#if !defined(BOARD_WIFI_ENABLED)
#define BOARD_WIFI_ENABLED   1
#endif
#if !defined(BOARD_WEB_RC_ENABLED)
#define BOARD_WEB_RC_ENABLED 1
#endif
