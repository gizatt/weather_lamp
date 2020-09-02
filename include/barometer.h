#pragma once

#include <SdFat.h>



class Barometer
{
    // Logging info.
    String _log_path;
    SdFat *_SD = nullptr;

    // Calibration values, which are
    // loaded from the IC during setup.
    float _A0;
    float _B1;
    float _B2;
    float _C12;

    float _pressure_estimate = 0.0;
    long _last_read_millis = 0;

public:
    Barometer();
    void ReadConfigValues();
    bool is_pressure_sane();
    void SetupLogging(SdFat *SD, String log_path);
    float ReadRawPressure();
    float UpdateAndLogPressureEstimate(bool verbose = true);
    void Update(bool verbose = true);
};