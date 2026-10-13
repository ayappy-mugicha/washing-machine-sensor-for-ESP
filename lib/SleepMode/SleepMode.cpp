#include "SleepMode.h"
#include <Arduino.h>
#include <VibrationSensor6050.h>
#include <Discord_Client.h>
#include <time.h>
#include <NetworkSystem.h>
#include "../../src/config.h"
SleepMode::SleepMode(const char* ntpserver, int utc ,const char* webhook) { // 住所を指定
    _NTPSERVER = ntpserver;
    _UTC = utc;
    _WEBHOOK = webhook;
    
}

bool SleepMode::updateLocalTime() {
    configTime(_UTC , 0 , _NTPSERVER);
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("時刻の取得に失敗しました");
        return false;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    return true;
}

void SleepMode::gotosleep(int second){
    VibrationSensor6050 sensor;
    NetworkSystem network(SECRET_SSID, SECRET_PASSWORD);
    Serial.printf("%d秒間スリープします",second);
    Serial.flush();

    network.disconnect_wifi();
    delay(100);
    sensor.end();
    esp_sleep_enable_timer_wakeup(second * 1000000ULL);
    esp_deep_sleep_start();
    return;
}

bool SleepMode::deepsleep(int startHour, int endHour) {
    struct tm timeinfo;
    NetworkSystem network(SECRET_SSID, SECRET_PASSWORD); 
    DiscordClient Discord(DISCORD_WEBHOOK);
    if(!getLocalTime(&timeinfo)) return false;

    int currentHour = timeinfo.tm_hour;
    uint64_t sleepTimeSec = 0;

    // 深夜判定（19時以降 または 6時より前）
    if (currentHour >= endHour || currentHour < startHour) {
        Serial.println("深夜モード：長時間スリープに入ります...");
        Discord.sendMessage("おねんねするねまた明日!!");
        
        if (currentHour >= endHour) {
            // 今日の残り時間 + 明朝6時まで
            sleepTimeSec = ( (24 - currentHour) + startHour ) * 3600;
        } else {
            // 今日の朝6時まで
            sleepTimeSec = ( startHour - currentHour ) * 3600;
        }
        network.disconnect_wifi();
        delay(100);
        // 秒をマイクロ秒に変換してDeep Sleep (ULLはunsigned long longの略)
        esp_sleep_enable_timer_wakeup(sleepTimeSec * 1000000ULL);
        esp_deep_sleep_start();
    }
    return false;
}