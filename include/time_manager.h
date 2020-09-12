#pragma once

#include <Arduino.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

/* Manages an NTP connection and serves timestamps
to consumers for logging purposes.

NTP stuff based on 
https://github.com/PaulStoffregen/Time/blob/master/examples/TimeNTP_ESP8266WiFi/TimeNTP_ESP8266WiFi.ino
*/

#define UDP_LOCAL_PORT 8888

// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
//static const char ntpServerName[] = "time.nist.gov";
//static const char ntpServerName[] = "time-a.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-b.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-c.timefreq.bldrdoc.gov";

// static const int timeZone = 1;     // Central European Time
// static const int timeZone = -5; // Eastern Standard Time (USA)
static const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)

// I think there's a way to do this in an object-oriented
// way with singletons, but it seems like a lot of work
// and boilerplate for the same functionality...

bool setup_time_management();
String get_current_timestamp();