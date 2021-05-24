#include "barometer.h"
#include "pin_assignments.h"
#include "time_manager.h"

#define PRESH 0x80
#define PRESL 0x82
#define TEMPH 0x84
#define TEMPL 0x86
#define A0MSB 0x88
#define A0LSB 0x8A
#define B1MSB 0x8C
#define B1LSB 0x8E
#define B2MSB 0x90
#define B2LSB 0x92
#define C12MSB 0x94
#define C12LSB 0x96
#define CONVERT 0x24

#define MIN_REASONABLE_PRESSURE_KPA 93.  // Assume at sea level...
#define MAX_REASONABLE_PRESSURE_KPA 107.
#define UPDATE_PERIOD_MS 1000
#define LOG_PERIOD_MS 30000
#define ESTIMATE_ALPHA 0.9

//Read registers
static unsigned int readRegister(byte thisRegister)
{
    unsigned int result = 0; // result to return
    digitalWrite(PIN_BARO_CS, LOW);
    delay(20);
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    SPI.transfer(thisRegister);
    result = SPI.transfer(0x00);
    digitalWrite(PIN_BARO_CS, HIGH);
    SPI.endTransaction();
    return (result);
}

Barometer::Barometer()
{
    _raw_pressure_read_buffer_head = 0;
    for (int k = 0; k < PRESSURE_READ_BUFFER_LEN; k++){
        _raw_pressure_read_buffer[k] = 0.0;
    }
}

void Barometer::ReadConfigValues(){
    // Read registers that contain the chip-unique parameters to do the math
    unsigned int A0H = readRegister(A0MSB);
    unsigned int A0L = readRegister(A0LSB);
    _A0 = (A0H << 5) + (A0L >> 3) + (A0L & 0x07) / 8.0;

    unsigned int B1H = readRegister(B1MSB);
    unsigned int B1L = readRegister(B1LSB);
    _B1 = ((((B1H & 0x1F) * 0x100) + B1L) / 8192.0) - 3;

    unsigned int B2H = readRegister(B2MSB);
    unsigned int B2L = readRegister(B2LSB);
    _B2 = ((((B2H - 0x80) << 8) + B2L) / 16384.0) - 2;

    unsigned int C12H = readRegister(C12MSB);
    unsigned int C12L = readRegister(C12LSB);
    _C12 = (((C12H * 0x100) + C12L) / 16777216.0);

    if (_SD)
    {
        File log_file = _SD->open(_log_path, O_RDWR | O_CREAT | O_AT_END);
        if (log_file)
        {
            String out_string = get_current_timestamp() + "; ";
            out_string += "A0 ";
            out_string += _A0;
            out_string += ";B1 ";
            out_string += _B1;
            out_string += ";B2 ";
            out_string += _B2;
            out_string += ";C12 ";
            out_string += _C12;
            log_file.println(out_string);
            log_file.close();
        }
        else
        {
            Serial.print("Error opening logfile ");
            Serial.println(_log_path);
        }
    }
}

bool Barometer::is_pressure_sane(float estimate){
    return (estimate > MIN_REASONABLE_PRESSURE_KPA &&
            estimate < MAX_REASONABLE_PRESSURE_KPA);
}

bool Barometer::SetupLogging(SdFat *SD, String log_path)
{
    _SD = SD;
    _log_path = log_path;
    // Trial run a log file open to see if it works.
    File log_file;
    bool successful_open = false;
    if (_SD)
    {
        log_file = _SD->open(_log_path, O_RDWR | O_CREAT | O_AT_END);
        if (log_file)
        {
            successful_open = true;
        }
    }
    return successful_open;
}


float Barometer::ReadRawPressure()
{
    digitalWrite(PIN_BARO_CS, LOW);
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    delay(3);
    SPI.transfer(0x24);
    SPI.transfer(0x00);
    digitalWrite(PIN_BARO_CS, HIGH);
    delay(3);
    digitalWrite(PIN_BARO_CS, LOW);
    SPI.transfer(PRESH);
    unsigned int presH = SPI.transfer(0x00);
    delay(3);
    SPI.transfer(PRESL);
    unsigned int presL = SPI.transfer(0x00);
    delay(3);
    SPI.transfer(TEMPH);
    unsigned int tempH = SPI.transfer(0x00);
    delay(3);
    SPI.transfer(TEMPL);
    unsigned int tempL = SPI.transfer(0x00);
    delay(3);
    SPI.transfer(0x00);
    delay(3);
    digitalWrite(PIN_BARO_CS, HIGH);
    SPI.endTransaction();

    unsigned long press = ((presH * 256) + presL) / 64;
    unsigned long temp = ((tempH * 256) + tempL) / 64;

    float pressure = _A0 + (_B1 + _C12 * temp) * press + _B2 * temp;
    float preskPa = pressure * (65.0 / 1023.0) + 50.0;

    return (preskPa);
}

float Barometer::UpdatePressureEstimate(){
    float new_raw_pressure = ReadRawPressure();
    // Log pressure into circular buffer for display.
    _raw_pressure_read_buffer[_raw_pressure_read_buffer_head] = new_raw_pressure;
    _raw_pressure_read_buffer_head =
        (_raw_pressure_read_buffer_head + 1)
        % PRESSURE_READ_BUFFER_LEN;
    if (is_pressure_sane(new_raw_pressure)){
        if (is_pressure_sane(_pressure_estimate)){
            _pressure_estimate = _pressure_estimate * ESTIMATE_ALPHA +
                                 new_raw_pressure * (1. - ESTIMATE_ALPHA);
        } else {
            _pressure_estimate = new_raw_pressure;
        }
    } else {
        ReadConfigValues();
        // Wait till next update cycle to try to read pressure.
    }
    return _pressure_estimate;
}

void Barometer::LogPressureEstimate(bool verbose){
    String out_string = get_current_timestamp() + "; ";
    out_string += "pressure ";
    out_string += _pressure_estimate;
    if (_SD)
    {
        File log_file = _SD->open(_log_path, O_RDWR | O_CREAT | O_AT_END);
        if (log_file)
        {
            log_file.println(out_string);
            log_file.close();
        }
        else
        {
            Serial.print("Error opening logfile ");
            Serial.println(_log_path);
        }
    }
    if (verbose)
    {
        Serial.println(out_string);
    }
}

void Barometer::Update(bool verbose){
    long now = millis();
    // Be sure to handle time wraparound; run the
    // update and logging every 5 seconds.
    if (now < _last_read_millis || 
        now > (_last_read_millis + UPDATE_PERIOD_MS)) {
        UpdatePressureEstimate();
        _last_read_millis = now;
    }
    if (now < _last_log_millis || 
        now > (_last_log_millis + LOG_PERIOD_MS)) {
        LogPressureEstimate(verbose);
        _last_log_millis = now;
    }
}