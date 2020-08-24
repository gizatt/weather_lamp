#include "lightning_detector.h"
#include "pin_assignments.h"
#include "time_manager.h"

LightningDetector::LightningDetector()
{
}

bool LightningDetector::SetupDetector()
{
    pinMode(PIN_LIGHTNING_INT, INPUT);

    bool success = _lightning.beginSPI(PIN_LIGHTNING_CS, 2000000);
    if (success)
    {
        Serial.println("Schmow-ZoW, Lightning Detector Ready!");
        this->ResendConfiguration();
    }
    else
    {
        Serial.println("Lightning Detector did not start up!");
    }
    return success;
}

bool LightningDetector::SetupLogging(SdFat *SD, String log_path)
{
    _SD = SD;
    _log_path = log_path;
    // Trial run a log write to see if it works.
    File log_file;
    bool successful_write = false;
    if (_SD)
    {
        log_file = _SD->open(_log_path, O_RDWR | O_CREAT | O_AT_END);
        if (log_file)
        {
            log_file.print(get_current_timestamp());
            log_file.print("; spike thresh: ");
            log_file.print(_current_spike_thresh);
            log_file.print("; noise floor: ");
            log_file.print(_current_noise_floor);
            log_file.print("; watchdog thresh: ");
            log_file.println(_current_watchdog_thresh);
            log_file.close();
            successful_write = true;
        }
    }
    return successful_write;
}

void LightningDetector::ResendConfiguration()
{
    // The lightning detector defaults to an indoor setting at
    // the cost of less sensitivity, if you plan on using this outdoors
    // uncomment the following line:
    _lightning.setIndoorOutdoor(OUTDOOR);

    _lightning.setNoiseLevel(_desired_noise_floor);
    _current_noise_floor = _lightning.readNoiseLevel();
    Serial.print("Noise floor is set at: ");
    Serial.println(_current_noise_floor);

    // Watchdog threshold setting can be from 1-10, one being the lowest. Default setting is
    // two. If you need to check the setting, the corresponding function for
    // reading the function follows.

    _lightning.watchdogThreshold(_desired_watchdog_thresh);
    _current_watchdog_thresh = _lightning.readWatchdogThreshold();
    Serial.print("Watchdog Threshold is set to: ");
    Serial.println(_current_watchdog_thresh);

    _lightning.spikeRejection(_desired_spike_thresh);
    _current_spike_thresh = _lightning.readSpikeRejection();
    Serial.print("Spike Rejection is set to: ");
    Serial.println(_current_spike_thresh);
}
int LightningDetector::CheckAndLogStatus(bool verbose)
{
    // Hardware has alerted us to an event, now we read the interrupt register
    int interrupt_val = -1;
    if (digitalRead(PIN_LIGHTNING_INT) == HIGH)
    {
        interrupt_val = _lightning.readInterruptReg();
        String out_string = get_current_timestamp() + "; ";
        // Open the logging file, if it exists

        byte distance = 0;
        switch (interrupt_val)
        {
        case AS3935_NOISE_INT:
            out_string += "Noise";
            break;
        case AS3935_DISTURBER_INT:
            _millis_of_last_disturber = millis();
            _time_of_last_disturber = now();
            _time_of_last_disturber_valid = true;
            out_string += "Disturber";
            break;
        case AS3935_LIGHTNING_INT:
            _millis_of_last_strike = millis();
            _time_of_last_strike = now();
            _time_of_last_strike_valid = true;
            distance = _lightning.distanceToStorm();
            out_string += "Lighting (";
            out_string += distance;
            out_string += " km)";
            break;
        default:
            out_string += "Unknown (val ";
            out_string += interrupt_val;
            out_string += ")";
            break;
        }

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
    return interrupt_val;
}

long int LightningDetector::get_millis_since_last_strike()
{
    if (_time_of_last_strike_valid)
    {
        return millis() - _millis_of_last_strike;
    }
    return -1;
}

long int LightningDetector::get_millis_since_last_disturber()
{
    if (_time_of_last_disturber_valid)
    {
        return millis() - _millis_of_last_disturber;
    }
    return -1;
}