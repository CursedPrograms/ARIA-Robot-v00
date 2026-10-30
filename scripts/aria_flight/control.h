// control.h
#ifndef CONTROL_H
#define CONTROL_H

// Flight modes
enum FlightMode {
    MODE_MANUAL = 0,
    MODE_ACRO = 1,
    MODE_STAB = 2,
    MODE_ALTHOLD = 3, // barometer altitude hold (throttle = climb rate)
    MODE_AUTO = 4
};

struct ControlState {
    int mode;
    bool armed;
    float thrustTarget;
    float rollTarget;
    float pitchTarget;
    float yawTarget;
};

// declarations
void control();
void interpretControls();
void interpretWebRC();
void combineInputs();
void controlAttitude();
void controlRates();
void controlTorque();

// helpers
const char* getModeName(int mode);

// external functions
bool isUsingWebRC();
void setWebRCWarn(const char* msg);

#endif // CONTROL_H
