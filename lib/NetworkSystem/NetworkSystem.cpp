#include "NetworkSystem.h"
#include <WiFi.h>

// wifi connect
NetworkSystem::NetworkSystem (const char* ssid, const char* pass) {
    _SSID = ssid;
    _PASS = pass;
}

bool NetworkSystem::connect_wifi() {
  WiFi.mode(WIFI_STA);
  if (WiFi.status() == WL_CONNECTED) return true;
  Serial.print("connectting to wifi...");
  
  WiFi.begin(_SSID, _PASS);
  int retrycount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    retrycount++;
    if (retrycount > 20) {
      Serial.println("can`t connect to wifi\n");
      return false;
    }
  }
  Serial.println("connected!\n");
  return true;
}
void NetworkSystem::disconnect_wifi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
}