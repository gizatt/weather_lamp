#include "lightning_scraper.h"

using namespace websockets;

void onMessageCallback(WebsocketsMessage message) {
    Serial.print("Got Message: ");
    Serial.println(message.data());
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connnection Opened");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connnection Closed");
    } else if(event == WebsocketsEvent::GotPing) {
        Serial.println("Got a Ping!");
    } else if(event == WebsocketsEvent::GotPong) {
        Serial.println("Got a Pong!");
    }
}

LightningScraper::LightningScraper()
{
}

bool LightningScraper::SetupConnection()
{
    // run callback when messages are received
    client.onMessage(onMessageCallback);
    // run callback when events are occuring
    client.onEvent(onEventsCallback);

    bool connected = client.connect("wss://ws5.blitzortung.org:3000");
    if(connected) {
        Serial.println("Connected!");
        //client.send("Hello Server");
        return true;
    } else {
        Serial.println("Not Connected!");
        return false;
    }
}
