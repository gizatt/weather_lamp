#pragma once

#include <SdFat.h>

#define PRESSURE_READ_BUFFER_LEN 100

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

    float _raw_pressure_read_buffer[PRESSURE_READ_BUFFER_LEN];
    int _raw_pressure_read_buffer_head;

    float _pressure_estimate = 0.0;
    long _last_read_millis = 0;
    long _last_log_millis = 0;

public:
    Barometer();
    void ReadConfigValues();
    bool SetupLogging(SdFat *SD, String log_path);
    bool is_pressure_sane();
    float get_pressure_estimate(){
        return _pressure_estimate;
    }
    float ReadRawPressure();
    float UpdatePressureEstimate();
    void LogPressureEstimate(bool verbose = true);
    void Update(bool verbose = true);
};