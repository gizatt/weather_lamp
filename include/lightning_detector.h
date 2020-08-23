#pragma once

#include "SparkFun_AS3935.h"
#include <SDfat.h>

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
    byte _noiseFloor = 5;
    byte _watchDogVal = 2;
    byte _spike = 2;
    byte _lightningThresh = 1;

    // Logging info.
    String _log_path;
    SdFat *_SD = nullptr;

public:
    LightningDetector();
    bool SetupDetector();
    bool SetupLogging(SdFat *SD, String log_path);
    void ResendConfiguration();
    int CheckAndLogStatus(bool verbose = true);
};