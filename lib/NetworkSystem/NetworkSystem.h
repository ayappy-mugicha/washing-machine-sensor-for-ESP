#ifndef NETWORKSYSTEM_H
#define NETWORKSYSTEM_H
#include <Arduino.h>

class NetworkSystem {
public:
    NetworkSystem(const char* ssid, const char* pass);
    bool connect_wifi();
    void disconnect_wifi();
private:
    const char* _SSID;
    const char* _PASS;
};

#endif
