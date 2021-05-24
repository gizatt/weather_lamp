#pragma once

#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

class LightningScraper
{
private:
    websockets::WebsocketsClient client;
public:
    LightningScraper();
    bool SetupConnection();
};