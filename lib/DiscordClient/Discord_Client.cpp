#include "Discord_Client.h"
#include <HttpClient.h>
#include <WiFiClientSecure.h>
#include "../NetworkSystem/NetworkSystem.h"
#include "../../src/config.h"

DiscordClient::DiscordClient (const char* url){
    _WebhookUrl = url;
}

void DiscordClient::sendMessage (String message , bool status) {
    HTTPClient http;
    WiFiClientSecure client;
    NetworkSystem network(SECRET_SSID, SECRET_PASSWORD);

    if (network.connect_wifi()){
        client.setInsecure();

        String symbol = (status == true) ? "+":"-";
        String payload = "## 洗濯機君からのお知らせ!! \\n";
        payload += "```diff\\n";
        payload += symbol + message + "\\n";
        payload += "```";

        Serial.println(payload);

        if (http.begin(client, _WebhookUrl)) {
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.addHeader("Content-Type", "application/json");
            String jsonPayload = "{\"content\": \"" + payload + "\"}";

            Serial.printf("\n sending message for Discord %s",jsonPayload);
            int httpResponseCode = http.POST(jsonPayload);

            if (httpResponseCode > 0) Serial.printf("\n success! Responsecode: %d \n", httpResponseCode);
            else Serial.printf("\n can not send for Discord Responsecode: %d \n", httpResponseCode);
            http.end();
        }
        network.disconnect_wifi();
    } else {
        Serial.println("Discordに送信できません。wifiに繋がっていません。再接続します");
        // sendmessage(message,status);
        network.connect_wifi();
    }
    return;

}
