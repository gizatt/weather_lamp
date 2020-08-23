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
#include "SparkFun_AS3935.h"
#include "Adafruit_ST7789.h"

/*

Pin 6: CS for the barometer
Pin 7: CS for the lightning detector
Pin 8: SPI MOSI
Pin 9: SPI SCK
Pin 10: SPI MISO

Lightning detector interrupt not wired, may need a step-up depending on its specs?

*/

#define INDOOR 0x12
#define OUTDOOR 0xE
#define LIGHTNING_INT 0x08
#define DISTURBER_INT 0x04
#define NOISE_INT 0x01

SparkFun_AS3935 lightning;

// Interrupt pin for lightning detection 
const int lightningInt = 4; 
int spiCS = 7; //SPI chip select pin

// This variable holds the number representing the lightning or non-lightning
// event issued by the lightning detector. 
int intVal = 0;
int noise = 2; // Value between 1-7 
int disturber = 2; // Value between 1-10

// Values for modifying the IC's settings. All of these values are set to their
// default values. 
byte noiseFloor = 5;
byte watchDogVal = 2;
byte spike = 2;
byte lightningThresh = 1; 

// Barometer stuff
#define PRESH   0x80
#define   PRESL   0x82
#define   TEMPH   0x84
#define   TEMPL   0x86
#define A0MSB   0x88
#define A0LSB   0x8A
#define B1MSB   0x8C
#define B1LSB   0x8E
#define   B2MSB   0x90
#define B2LSB   0x92
#define C12MSB   0x94
#define   C12LSB   0x96
#define CONVERT   0x24   
#define chipSelectPin 6
float A0_;
float B1_;
float B2_;
float C12_;

#define LED_PIN     5
#define NUM_LEDS    5
CRGB leds[NUM_LEDS];

// SD pin: 3
// Display SPI CS pin: 2
// Display data select pin: 1
#define TFT_CS 2
#define TFT_DC 1
#define TFT_RST 0
#define SD_CS 3

SdFat                SD;         // SD card filesystem
Adafruit_ImageReader reader(SD); // Image-reader object, pass in SD filesys
const uint16_t  Display_Color_Magenta      = 0xF81F;

//Read registers
unsigned int readRegister(byte thisRegister ) {
  unsigned int result = 0;   // result to return
  digitalWrite(chipSelectPin, LOW);
  delay(20);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(thisRegister);
  result = SPI.transfer(0x00);
  digitalWrite(chipSelectPin, HIGH);
  SPI.endTransaction();
  return(result);
}

//read pressure
float baroPressure(){

   // read registers that contain the chip-unique parameters to do the math
  unsigned int A0H = readRegister(A0MSB);
  unsigned int A0L = readRegister(A0LSB);
         A0_ = (A0H << 5) + (A0L >> 3) + (A0L & 0x07) / 8.0;

  unsigned int B1H = readRegister(B1MSB);
  unsigned int B1L = readRegister(B1LSB);
          B1_ = ( ( ( (B1H & 0x1F) * 0x100)+B1L) / 8192.0) - 3 ;
 
  unsigned int B2H = readRegister(B2MSB);
  unsigned int B2L = readRegister(B2LSB);
          B2_ = ( ( ( (B2H - 0x80) << 8) + B2L) / 16384.0 ) - 2 ;
 
  unsigned int C12H = readRegister(C12MSB);
  unsigned int C12L = readRegister(C12LSB);
          C12_ = ( ( ( C12H * 0x100 ) + C12L) / 16777216.0 )  ;

  A0H = readRegister(A0MSB);
  A0L = readRegister(A0LSB);
         A0_ = (A0H << 5) + (A0L >> 3) + (A0L & 0x07) / 8.0;
         
  digitalWrite(chipSelectPin, LOW);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  delay(3);
    SPI.transfer(0x24);
    SPI.transfer(0x00);
    digitalWrite(chipSelectPin, HIGH);
    delay(3);
  digitalWrite(chipSelectPin, LOW);
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
  digitalWrite(chipSelectPin, HIGH);
  SPI.endTransaction();

  unsigned long press = ((presH *256) + presL)/64;
  unsigned long temp  = ((tempH *256) + tempL)/64;
 
  float pressure = A0_+(B1_+C12_*temp)*press+B2_*temp;
  float preskPa = pressure*  (65.0/1023.0)+50.0;
 
return(preskPa);
}

void setup()
{
  // When lightning is detected the interrupt pin goes HIGH.
  pinMode(lightningInt, INPUT); 
  pinMode(chipSelectPin, OUTPUT);
  digitalWrite(chipSelectPin, HIGH);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(chipSelectPin, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(chipSelectPin, HIGH);
  pinMode(spiCS, OUTPUT);
  digitalWrite(spiCS, HIGH);
  
  Serial.begin(115200); 
  SPI.begin(); 
  SPI.setClockDivider(SPI_CLOCK_DIV16);

  
  // Barometer setup
  // initalize the data ready and chip select pins:
  delay (100);

  // todo: read these multiple times and get consensus.
  // I'm worried these lines are so noisy that this is risky...
  delay(100);

  delay(5000);
  // Set up screen
  Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
  tft.init(135, 240);           // Init ST7789 240x135
  tft.fillScreen(Display_Color_Magenta);

  while( !lightning.beginSPI(spiCS, 2000000) ){ 
    Serial.println ("Lightning Detector did not start up!"); 
    delay(1000);
  }
  Serial.println("Schmow-ZoW, Lightning Detector Ready!");

  while(!SD.begin(SD_CS, SD_SCK_MHZ(4))) { // ESP32 requires 25 MHz limit
    Serial.println(F("SD begin() failed"));
    delay(1000);
  }
  
  ImageReturnCode stat;
  stat = reader.drawBMP("/ASSETS/LUCIE.BMP", tft, 0, 0);
  reader.printStatus(stat);
  
  delay(1000);
    
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);

  // The lightning detector defaults to an indoor setting at 
  // the cost of less sensitivity, if you plan on using this outdoors 
  // uncomment the following line:
  lightning.setIndoorOutdoor(OUTDOOR); 
  
  lightning.setNoiseLevel(noiseFloor);  
  
  int noiseVal = lightning.readNoiseLevel();
  Serial.print("Noise Level is set at: ");
  Serial.println(noiseVal);
  
  // Watchdog threshold setting can be from 1-10, one being the lowest. Default setting is
  // two. If you need to check the setting, the corresponding function for
  // reading the function follows.    
  
  lightning.watchdogThreshold(watchDogVal); 
  
  int watchVal = lightning.readWatchdogThreshold();
  Serial.print("Watchdog Threshold is set to: ");
  Serial.println(watchVal);
  
}

int k = 0;
bool avg_pressure_init = false;
float avg_pressure = 0;
void loop()
{
   // Hardware has alerted us to an event, now we read the interrupt register
  if(digitalRead(lightningInt) == HIGH){
    intVal = lightning.readInterruptReg();
    if(intVal == NOISE_INT){
      Serial.println("Noise."); 
      // Too much noise? Uncomment the code below, a higher number means better
      // noise rejection.
      //lightning.setNoiseLevel(noise); 
    }
    else if(intVal == DISTURBER_INT){
      Serial.println("Disturber."); 
      // Too many disturbers? Uncomment the code below, a higher number means better
      // disturber rejection.
      //lightning.watchdogThreshold(disturber);  
    }
    else if(intVal == LIGHTNING_INT){
      Serial.println("Lightning Strike Detected!"); 
      // Lightning! Now how far away is it? Distance estimation takes into
      // account any previously seen events in the last 15 seconds. 
      byte distance = lightning.distanceToStorm(); 
      Serial.print("Approximately: "); 
      Serial.print(distance); 
      Serial.println("km away!"); 
    }
  }
  k = (k + 1) % 50;
  if (k == 0){
    Serial.print("The pressure is : ");
    if (avg_pressure_init) {
      avg_pressure = avg_pressure*0.9 + baroPressure()*0.1;
    } else {
      avg_pressure = baroPressure();
      avg_pressure_init = true;
    }
    Serial.print(avg_pressure);
    Serial.println(" kPa");
  }
  delay(50); // Slow it down.
  
  for (int i = 0; i <= NUM_LEDS; i++) {
    leds[i] = CRGB ( 0, 0, 255);
    FastLED.show();
    delay(1);
  }
  for (int i = NUM_LEDS; i >= 0; i--) {
    leds[i] = CRGB ( 255, 0, 0);
    FastLED.show();
    delay(1);
  }
}
