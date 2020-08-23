#include "lightning_detector.h"
#include "pin_assignments.h"

LightningDetector::LightningDetector()
{
    pinMode(PIN_LIGHTNING_INT, INPUT);

    while (!lightning.beginSPI(PIN_LIGHTNING_CS, 2000000))
    {
        Serial.println("Lightning Detector did not start up!");
        delay(1000);
    }
    Serial.println("Schmow-ZoW, Lightning Detector Ready!");

    this->ResendConfiguration();
}

void LightningDetector::ResendConfiguration(){
    // The lightning detector defaults to an indoor setting at
    // the cost of less sensitivity, if you plan on using this outdoors
    // uncomment the following line:
    lightning.setIndoorOutdoor(OUTDOOR);


    lightning.setNoiseLevel(_noiseFloor);
    int noiseVal = lightning.readNoiseLevel();
    Serial.print("Noise Level is set at: ");
    Serial.println(noiseVal);

    // Watchdog threshold setting can be from 1-10, one being the lowest. Default setting is
    // two. If you need to check the setting, the corresponding function for
    // reading the function follows.

    lightning.watchdogThreshold(_watchDogVal);
    int watchVal = lightning.readWatchdogThreshold();
    Serial.print("Watchdog Threshold is set to: ");
    Serial.println(watchVal);

    lightning.spikeRejection(_spike); 
    int spikeVal = lightning.readSpikeRejection();
    Serial.print("Spike Rejection is set to: ");
    Serial.println(spikeVal);
}
int LightningDetector::CheckAndPrintStatus()
{
    // Hardware has alerted us to an event, now we read the interrupt register
    int interrupt_val = -1;
    if (digitalRead(PIN_LIGHTNING_INT) == HIGH)
    {
        interrupt_val = lightning.readInterruptReg();
        byte distance = 0;
        switch (interrupt_val)
        {
        case AS3935_NOISE_INT:
            Serial.println("Noise.");
            // Too much noise? Uncomment the code below, a higher number means better
            // noise rejection.
            //lightning.setNoiseLevel(noise);
            break;
        case AS3935_DISTURBER_INT:
            Serial.println("Disturber.");
            // Too many disturbers? Uncomment the code below, a higher number means better
            // disturber rejection.
            //lightning.watchdogThreshold(disturber);
            break;
        case AS3935_LIGHTNING_INT:
            Serial.println("Lightning Strike Detected!");
            // Lightning! Now how far away is it? Distance estimation takes into
            // account any previously seen events in the last 15 seconds.
            distance = lightning.distanceToStorm();
            Serial.print("Approximately: ");
            Serial.print(distance);
            Serial.println("km away!");
            break;
        default:
            Serial.print("Unknown interrupt val read: ");
            Serial.print(interrupt_val);
            Serial.print("\n");
            break;
        }
    }
    return interrupt_val;
}