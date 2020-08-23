#include "lightning_detector.h"
#include "pin_assignments.h"

LightningDetector::LightningDetector(SdFat *SD, String log_path)
{
    _SD = SD;
    _log_path = log_path;

    pinMode(PIN_LIGHTNING_INT, INPUT);

    while (!_lightning.beginSPI(PIN_LIGHTNING_CS, 2000000))
    {
        Serial.println("Lightning Detector did not start up!");
        delay(1000);
    }
    Serial.println("Schmow-ZoW, Lightning Detector Ready!");

    this->ResendConfiguration();
}

void LightningDetector::ResendConfiguration()
{
    // The lightning detector defaults to an indoor setting at
    // the cost of less sensitivity, if you plan on using this outdoors
    // uncomment the following line:
    _lightning.setIndoorOutdoor(OUTDOOR);

    _lightning.setNoiseLevel(_noiseFloor);
    int noiseVal = _lightning.readNoiseLevel();
    Serial.print("Noise Level is set at: ");
    Serial.println(noiseVal);

    // Watchdog threshold setting can be from 1-10, one being the lowest. Default setting is
    // two. If you need to check the setting, the corresponding function for
    // reading the function follows.

    _lightning.watchdogThreshold(_watchDogVal);
    int watchVal = _lightning.readWatchdogThreshold();
    Serial.print("Watchdog Threshold is set to: ");
    Serial.println(watchVal);

    _lightning.spikeRejection(_spike);
    int spikeVal = _lightning.readSpikeRejection();
    Serial.print("Spike Rejection is set to: ");
    Serial.println(spikeVal);
}
int LightningDetector::CheckAndLogStatus(bool verbose)
{
    // Hardware has alerted us to an event, now we read the interrupt register
    int interrupt_val = -1;
    Serial.println("loop");
    if (digitalRead(PIN_LIGHTNING_INT) == HIGH)
    {
        interrupt_val = _lightning.readInterruptReg();
        // Open the logging file, if it exists
        // File log_file;
        bool valid_log_file = false;
        Serial.println("Trying to read SD");

        /* if (_SD)
        {
            log_file = _SD->open(_log_path, O_RDWR | O_CREAT | O_AT_END);
            Serial.print("Opened log file");
            Serial.println(log_file);
            Serial.flush();
            if (log_file)
            {
                valid_log_file = true;
                log_file.print(millis());
                log_file.print(": ");
            }
            else
            {
                Serial.print("Error opening logfile ");
                Serial.println(_log_path);
            }
        }*/
        if (verbose)
        {
            Serial.print(millis());
            Serial.print(": ");
        }
        if (valid_log_file)
        {
            //log_file.print(": ");
        }
        byte distance = 0;
        switch (interrupt_val)
        {
        case AS3935_NOISE_INT:
            if (verbose)
            {
                Serial.println("Noise.");
            }
            if (valid_log_file)
            {
                //log_file.println("Noise.");
            }
            // Too much noise? Uncomment the code below, a higher number means better
            // noise rejection.
            //lightning.setNoiseLevel(noise);
            break;
        case AS3935_DISTURBER_INT:
            if (verbose)
            {
                Serial.println("Disturber.");
            }
            if (valid_log_file)
            {
                //log_file.println("Disturber.");
            }
            // Too many disturbers? Uncomment the code below, a higher number means better
            // disturber rejection.
            //lightning.watchdogThreshold(disturber);
            break;
        case AS3935_LIGHTNING_INT:
            distance = _lightning.distanceToStorm();
            if (verbose)
            {
                Serial.print("Lightning detected approximately ");
                Serial.print(distance);
                Serial.println("km away!");
            }
            if (valid_log_file)
            {
                //log_file.print("Lightning detected approximately ");
                //log_file.print(distance);
                //log_file.println("km away!");
            }
            break;
        default:
            if (verbose)
            {
                Serial.print("Unknown interrupt val read: ");
                Serial.print(interrupt_val);
                Serial.print("\n");
            }
            if (valid_log_file)
            {
                //log_file.print("Unknown interrupt val read: ");
                //log_file.print(interrupt_val);
                //log_file.print("\n");
            }
            break;
        }
    }
    return interrupt_val;
}