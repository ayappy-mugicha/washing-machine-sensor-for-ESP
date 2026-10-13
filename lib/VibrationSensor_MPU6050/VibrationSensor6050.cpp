#include "VibrationSensor6050.h"

VibrationSensor6050::VibrationSensor6050() : _accel(0x68) { // 住所を指定
}

bool VibrationSensor6050::begin() {
    float offsetIntencity = 0;
    Wire.begin(6,7);
    // センサーを起こす（Sleepモードを解除する）
    // 0x6B（電源管理レジスタ）に 0 を書き込むよ
    Wire.beginTransmission(0x68);
    Wire.write(0x6B);
    Wire.write(0);
    if (Wire.endTransmission() != 0) {
        return false; // そもそも通信できていない
    }
    return true;
}
bool VibrationSensor6050::end() {
    Wire.beginTransmission(0x68);
    Wire.write(0x6B);
    Wire.write(0x40);

    if (Wire.endTransmission() == 0) {
        Wire.end();
        return true; // 止まったらtrueを返す
    }
    // Wire.endTransmission();
    return false; // とめられまてん
}
WasherStatus VibrationSensor6050::getcombinedData() {
    WasherStatus data;
    Wire.beginTransmission(0x68);
    Wire.write(0x3B); // 加速度データの先頭アドレス
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)0x68, (uint8_t)6, (uint8_t)true);

    if (Wire.available() >= 6) {
        int16_t ax = Wire.read() << 8 | Wire.read();
        int16_t ay = Wire.read() << 8 | Wire.read();
        int16_t az = Wire.read() << 8 | Wire.read();

        // 生データを重力単位に変換
        float x = (float)ax / 16384.0 * 9.8;
        float y = (float)ay / 16384.0 * 9.8;
        float z = (float)az / 16384.0 * 9.8;
        data.x = abs(x);
        data.y = abs(y);
        data.z = abs(z - 9.8);
        data.intensity = abs(x) + abs(y) + abs(z - 9.8);
    }
    return data;
}
bool VibrationSensor6050::setup() {
    begin();
    delay(50);
    getIntensity();
    delay(10);
    return true;
}
bool VibrationSensor6050::sensor_exsis() {
  int count = 0;
  while(!begin()) {
    Serial.println("*センサーが見つからないよ！*配線を確認してね。");
    delay(1000);
    count++;
    if ((count % 10) == 0) {
      return false;
    }
    if (begin()) {
      return true;
    }
  }
  return true;
}
float VibrationSensor6050::getIntensity() {
    Wire.beginTransmission(0x68);
    Wire.write(0x3B); // 加速度データの先頭アドレス
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)0x68, (uint8_t)6, (uint8_t)true);

    if (Wire.available() >= 6) {
        int16_t ax = Wire.read() << 8 | Wire.read();
        int16_t ay = Wire.read() << 8 | Wire.read();
        int16_t az = Wire.read() << 8 | Wire.read();

        // 生データを重力単位に変換
        float x = (float)ax / 16384.0 * 9.8;
        float y = (float)ay / 16384.0 * 9.8;
        float z = (float)az / 16384.0 * 9.8;
        Serial.println("x:" + String(abs(x)));
        Serial.println("y:" + String(abs(y)));
        Serial.println("z:" + String(abs(z - 9.8)));
        return abs(x) + abs(y) + abs(z - 9.8);
    }
    
    return 0.0; // 読み取れなかったら0
}

float VibrationSensor6050::offset() {
  Serial.println("offsetting sensor");
  delay(1000);
  float sum = 0;
  float intent = 0;
  for (int i = 0; i < 10; i++) {
      sum += VibrationSensor6050().getIntensity();
      delay(100);
  }
  intent = sum / 10.0;
  Serial.printf("offset complate offsetintencity:%.2f\n", intent);
  return intent;
}
