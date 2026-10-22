#ifndef DISCORD_CLIENT_H
#define DISCORD_CLIENT_H
#include <Arduino.h>

class DiscordClient {
public:
    DiscordClient(const char* url);

    void sendMessage(String message, bool status = true, bool disconnectAfter = true);

private:
    const char* _WebhookUrl;
};

#endif