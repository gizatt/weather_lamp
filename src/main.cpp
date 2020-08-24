// Adafruit GFX Library - Version: Latest
#include <Adafruit_GFX.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <gfxfont.h>
#include <SDfat.h>                // SD card & FAT filesystem library
#include <Adafruit_ImageReader.h> // Image-reading functions
#include <FastLED.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_ST7789.h"
#include <WiFiNINA.h>
#include <WiFiUdp.h>

#include "pin_assignments.h"
#include "lightning_detector.h"
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

// Barometer stuff
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
#define PIN_BARO_CS 6
float A0_;
float B1_;
float B2_;
float C12_;

// SD pin: 3
// Display SPI CS pin: 2
// Display data select pin: 1
#define PIN_TFT_CS 2
#define PIN_TFT_DC 1
#define PIN_TFT_RST 0
#define PIN_SD_CS 3

SdFat SD;                        // SD card filesystem
Adafruit_ImageReader reader(SD); // Image-reader object, pass in SD filesys
const uint16_t Display_Color_Magenta = 0xF81F;

#define PIN_LED_STRIP 5
#define NUM_LEDS 100
CRGB leds[NUM_LEDS];
float curr_r = 0;
float curr_g = 0;
float curr_b = 0;

//Read registers
unsigned int readRegister(byte thisRegister)
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

//read pressure
float baroPressure()
{

  // read registers that contain the chip-unique parameters to do the math
  unsigned int A0H = readRegister(A0MSB);
  unsigned int A0L = readRegister(A0LSB);
  A0_ = (A0H << 5) + (A0L >> 3) + (A0L & 0x07) / 8.0;

  unsigned int B1H = readRegister(B1MSB);
  unsigned int B1L = readRegister(B1LSB);
  B1_ = ((((B1H & 0x1F) * 0x100) + B1L) / 8192.0) - 3;

  unsigned int B2H = readRegister(B2MSB);
  unsigned int B2L = readRegister(B2LSB);
  B2_ = ((((B2H - 0x80) << 8) + B2L) / 16384.0) - 2;

  unsigned int C12H = readRegister(C12MSB);
  unsigned int C12L = readRegister(C12LSB);
  C12_ = (((C12H * 0x100) + C12L) / 16777216.0);

  A0H = readRegister(A0MSB);
  A0L = readRegister(A0LSB);
  A0_ = (A0H << 5) + (A0L >> 3) + (A0L & 0x07) / 8.0;

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

  float pressure = A0_ + (B1_ + C12_ * temp) * press + B2_ * temp;
  float preskPa = pressure * (65.0 / 1023.0) + 50.0;

  return (preskPa);
}

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
  Adafruit_ST7789 tft = Adafruit_ST7789(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
  tft.init(135, 240); // Init ST7789 240x135
  tft.fillScreen(Display_Color_Magenta);

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

  ImageReturnCode stat;
  stat = reader.drawBMP("/ASSETS/LUCIE.BMP", tft, 0, 0);
  reader.printStatus(stat);

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
  k = (k + 1) % 500; // Every 5 seconds
  if (k == 0)
  {
    Serial.print("The pressure is : ");
    if (avg_pressure_init)
    {
      avg_pressure = avg_pressure * 0.9 + baroPressure() * 0.1;
    }
    else
    {
      avg_pressure = baroPressure();
      avg_pressure_init = true;
    }
    Serial.print(avg_pressure);
    Serial.println(" kPa");
  }

  float new_r = 0.;
  auto tmp = lightning_detector.get_millis_since_last_disturber();
  if (tmp > 0){
    new_r = 1. - float(tmp) / 1000.;
    new_r = max(min(new_r, 1.), 0.);
  }
  float new_b = 0.;
  tmp = lightning_detector.get_millis_since_last_strike();
  if (tmp > 0){
    new_b = 1. - float(tmp) / 1000.;
    new_b = max(min(new_b, 1.), 0.);
  }
  

  float new_g = float(random(0, 10));

  float alpha = 0.9;
  curr_r = new_r; // curr_r * alpha + new_r * alpha;
  curr_g = curr_g * alpha + new_g * (1. - alpha);
  curr_b = new_b; // curr_b * alpha + new_b * alpha;

  for (int i = NUM_LEDS-1; i > 0; i--)
  {
    leds[i] = leds[i-1];
  }
  leds[0] = CRGB(byte(curr_r*255), byte(curr_g*255), byte(curr_b)*255);
  FastLED.show();
  delay(10);

}