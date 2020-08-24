#pragma once

#include "SparkFun_AS3935.h"
#include <SdFat.h>

#define AS3935_INDOOR 0x12
#define AS3935_OUTDOOR 0xE
#define AS3935_LIGHTNING_INT 0x08
#define AS3935_DISTURBER_INT 0x04
#define AS3935_NOISE_INT 0x01

class LightningDetector
{
    SparkFun_AS3935 _lightning;
    // Values for modifying the IC's settings. All of these values are set to their
    // default values.
    
    byte _desired_noise_floor = 5;
    byte _desired_watchdog_thresh = 2;
    byte _desired_spike_thresh = 2;

    byte _current_noise_floor = 5;
    byte _current_watchdog_thresh = 2;
    byte _current_spike_thresh = 2;

    // Logging info.
    String _log_path;
    SdFat *_SD = nullptr;

    long int _millis_of_last_strike;
    time_t _time_of_last_strike;
    bool _time_of_last_strike_valid = false;
    long int _millis_of_last_disturber;
    time_t _time_of_last_disturber;
    bool _time_of_last_disturber_valid = false;

public:
    LightningDetector();
    bool SetupDetector();
    bool SetupLogging(SdFat *SD, String log_path);
    void ResendConfiguration();
    int CheckAndLogStatus(bool verbose = true);
    long int get_millis_since_last_strike();
    long int get_millis_since_last_disturber();
};