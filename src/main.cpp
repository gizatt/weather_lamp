// Adafruit GFX Library - Version: Latest
#include <Adafruit_GFX.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <gfxfont.h>
#include <SdFat.h>                // SD card & FAT filesystem library
#include <Adafruit_ImageReader.h> // Image-reading functions
#include <FastLED.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_ST7789.h"
#include <WiFiNINA.h>
#include <WiFiUdp.h>

#include "pin_assignments.h"
#include "lightning_detector.h"
#include "barometer.h"
#include "time_manager.h"
#include "secrets.h"

/*
Main loop of the storm lamp. Initializes and keeps track of:
- Maintaining an internet connection, for NTP update.
- The SD card, for both logging + serving the webpage + populating
  the screen.
- The screen, for displaying current status + data.
- The lightning detector itself, which is polled for detections
  periodically. (TODO: Interrupt handler?)
- A barometer, which is polled for additional info.
- A web server for controlling the unit + displaying data.
- The lighting, which manages a set of independent LEDs + LED
  strips to change the lighting of the actual lamp.

Possible future features:
- Poll a web server for more local weather data to display or use.
*/

LightningDetector lightning_detector;
Barometer barometer;

SdFat SD;                        // SD card filesystem
Adafruit_ImageReader reader(SD); // Image-reader object, pass in SD filesys
const uint16_t Display_Color_Magenta = 0xF81F;

#define NUM_LEDS 150
CRGB leds[NUM_LEDS];
float curr_r = 0;
float curr_g = 0;
float curr_b = 0;

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}

void setup()
{
  // Pre-set all of the SPI chip selects
  // to output and disable them, so none
  // of them fight before setup is complete.
  pinMode(PIN_BARO_CS, OUTPUT);
  digitalWrite(PIN_BARO_CS, HIGH);
  pinMode(PIN_TFT_CS, OUTPUT);
  digitalWrite(PIN_BARO_CS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_BARO_CS, HIGH);
  pinMode(PIN_LIGHTNING_CS, OUTPUT);
  digitalWrite(PIN_LIGHTNING_CS, HIGH);

  Serial.begin(115200);
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV16);

  // Turn off strip so the Wifi has as much power to
  // connect as possible
  FastLED.addLeds<WS2812, PIN_LED_STRIP, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.clear();
  FastLED.show();
  delay(500);

  // Set up screen
  //Adafruit_ST7789 tft = Adafruit_ST7789(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
  //tft.init(135, 240); // Init ST7789 240x135
  //tft.fillScreen(Display_Color_Magenta);

  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC: ");
  printMacAddress(mac);

  // Wifi Setup
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  
  setup_time_management();
  Serial.println("Current time: " + get_current_timestamp());

  while (!SD.begin(PIN_SD_CS, SD_SCK_MHZ(4)))
  { // ESP32 requires 25 MHz limit
    Serial.println(F("SD begin() failed"));
    delay(1000);
  }

  //ImageReturnCode stat;
  //stat = reader.drawBMP("/ASSETS/LUCIE.BMP", tft, 0, 0);
  //reader.printStatus(stat);

  delay(100);

  while (!lightning_detector.SetupDetector())
  {
    delay(1000);
    Serial.println("Retrying to set up lightning detector...");
  }
  Serial.println("Setting up logging...");
  if (!lightning_detector.SetupLogging(&SD, "/LGHT.LOG"))
  {
    Serial.println("Failed to setup logging for lightning detector.");
  }
  Serial.println("Done with setup.");

  FastLED.addLeds<WS2812, PIN_LED_STRIP, GRB>(leds, NUM_LEDS);
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CRGB(i % 255, i*2 % 255, i*3 % 255);
    FastLED.show();
    delay(5);
  }
}

int k = 0;
bool avg_pressure_init = false;
float avg_pressure = 0;
void loop()
{
  lightning_detector.CheckAndLogStatus(true);
  barometer.Update(true);

  float new_r = 0.;
  auto tmp = lightning_detector.get_millis_since_last_disturber();
  if (tmp > 0){
    new_r = 1. - float(tmp) / 1000.;
  }
  new_r = max(min(new_r, 1.), 0.025);
  float new_b = 0.;
  tmp = lightning_detector.get_millis_since_last_strike();
  if (tmp > 0){
    new_b = 1. - float(tmp) / 1000.;
  }
  new_b = max(min(new_b, 1.), 0.05);
  
  float new_g = float(random(0, 3)) / 255.;

  float alpha = 0.9;
  curr_r = new_r; // curr_r * alpha + new_r * alpha;
  curr_g = curr_g * alpha + new_g * (1. - alpha);
  curr_b = new_b; // curr_b * alpha + new_b * alpha;

  for (int i = NUM_LEDS-1; i > 0; i--)
  {
    leds[i] = leds[i-1];
  }
  leds[0] = CRGB(byte(curr_r*255), byte(curr_g*255), byte(curr_b*255));
  FastLED.show();
  delay(10);

}