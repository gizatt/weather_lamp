#include "time_manager.h"

#define NTP_PACKET_SIZE 48 // NTP time is in the first 48 bytes of message
byte packet_buffer[NTP_PACKET_SIZE];
WiFiUDP udp;
bool ntp_initialized;

// send an NTP request to the time server at the given address
void send_ntp_packet(IPAddress &address)
{
    // set all bytes in the buffer to 0
    memset(packet_buffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packet_buffer[0] = 0b11100011; // LI, Version, Mode
    packet_buffer[1] = 0;          // Stratum, or type of clock
    packet_buffer[2] = 6;          // Polling Interval
    packet_buffer[3] = 0xEC;       // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packet_buffer[12] = 49;
    packet_buffer[13] = 0x4E;
    packet_buffer[14] = 49;
    packet_buffer[15] = 52;
    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udp.beginPacket(address, 123); //NTP requests are to port 123
    udp.write(packet_buffer, NTP_PACKET_SIZE);
    udp.endPacket();
}

time_t get_ntp_time()
{
    IPAddress ntpServerIP; // NTP server's ip address

    while (udp.parsePacket() > 0)
        ; // discard any previously received packets
    Serial.println("Transmit NTP Request");
    // get a random server from the pool
    WiFi.hostByName(ntpServerName, ntpServerIP);
    Serial.print(ntpServerName);
    Serial.print(": ");
    Serial.println(ntpServerIP);
    send_ntp_packet(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500)
    {
        int size = udp.parsePacket();
        if (size >= NTP_PACKET_SIZE)
        {
            Serial.println("Receive NTP Response");
            udp.read(packet_buffer, NTP_PACKET_SIZE); // read packet into the buffer
            unsigned long secsSince1900;
            // convert four bytes starting at location 40 to a long integer
            secsSince1900 = (unsigned long)packet_buffer[40] << 24;
            secsSince1900 |= (unsigned long)packet_buffer[41] << 16;
            secsSince1900 |= (unsigned long)packet_buffer[42] << 8;
            secsSince1900 |= (unsigned long)packet_buffer[43];
            return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
        }
    }
    Serial.println("No NTP Response :-(");
    return 0; // return 0 if unable to get the time
}

bool setup_time_management()
{
    if (ntp_initialized)
    {
        return true;
    }
    udp.begin(UDP_LOCAL_PORT);
    Serial.print("Local port: ");
    Serial.println(UDP_LOCAL_PORT);
    Serial.print("Waiting for sync...");
    setSyncProvider(get_ntp_time);
    setSyncInterval(300);
    Serial.println("synchronized.");
    ntp_initialized = true;
    return true;
}

String printDigits(int digits)
{
    // utility function for digital clock display: prints preceding colon and leading 0
    String out = "";
    out += ":";
    if (digits < 10)
        out += '0';
    out += digits;
    return out;
}

String get_current_timestamp()
{
    // digital clock display of the time
    String out = "";
    out += hour();

    out += printDigits(minute());
    out += printDigits(second());
    out += " ";
    out += dayShortStr(weekday());
    out += " ";
    out += day();
    out += " ";
    out += monthShortStr(month());
    out += " ";
    out += year();
    return out;
}