#include <Arduino.h>
#include "sensorcheck.h"
#include <Wire.h>

bool SensorCheck::begin () {
    Wire.begin();
    Serial.println("\nI2C Scanner starting");
    return true;
}

void SensorCheck::getsensor () {
    Wire.beginTransmission(0x68);
    Wire.write(0x3B); 
    Wire.endTransmission(false);
    
    // 6バイト（X, Y, Zの3軸分）のデータをちょうだい！とお願いする
    Wire.requestFrom(0x68, 6, true);

    if (Wire.available() >= 6) {
        int16_t ax = Wire.read() << 8 | Wire.read();
        int16_t ay = Wire.read() << 8 | Wire.read();
        int16_t az = Wire.read() << 8 | Wire.read();

        Serial.print("Raw Accel -> X: "); Serial.print(ax);
        Serial.print(" Y: "); Serial.print(ay);
        Serial.print(" Z: "); Serial.println(az);
    } else {
        Serial.println("Sensor is not responding to data requests...");
    }
    delay(500);
}