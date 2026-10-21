#include "VibrationSensor6050.h"
#include <math.h>

VibrationSensor6050::VibrationSensor6050() : _accel(0x68) { // 住所を指定
}

float VibrationSensor6050::intensityFrom(float x, float y, float z) {
    return fabsf(x) + fabsf(y) + fabsf(z - 9.8f);
}

bool VibrationSensor6050::readRawAccel(float& x, float& y, float& z) {
    Wire.beginTransmission(0x68);
    Wire.write(0x3B);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }
    if (Wire.requestFrom((uint8_t)0x68, (uint8_t)6, (uint8_t)true) != 6) {
        return false;
    }
    if (Wire.available() < 6) {
        return false;
    }

    int16_t ax = Wire.read() << 8 | Wire.read();
    int16_t ay = Wire.read() << 8 | Wire.read();
    int16_t az = Wire.read() << 8 | Wire.read();

    x = (float)ax / 16384.0f * 9.8f;
    y = (float)ay / 16384.0f * 9.8f;
    z = (float)az / 16384.0f * 9.8f;
    return true;
}

bool VibrationSensor6050::begin() {
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
    return false; // とめられまてん
}
WasherStatus VibrationSensor6050::getcombinedData() {
    WasherStatus data;
    data.x = 0;
    data.y = 0;
    data.z = 0;
    data.intensity = 0;
    data.ok = false;

    float x = 0;
    float y = 0;
    float z = 0;
    if (!readRawAccel(x, y, z)) {
        return data;
    }
    data.x = fabsf(x);
    data.y = fabsf(y);
    data.z = fabsf(z - 9.8f);
    data.intensity = intensityFrom(x, y, z);
    data.ok = true;
    return data;
}

VibrationWindow VibrationSensor6050::sampleWindow(float offsetIntensity, int count, int intervalMs) {
    VibrationWindow window;
    window.ok = false;
    window.rms = 0;
    window.meanRaw = 0;
    window.peak = 0;
    window.lastY = 0;
    window.successCount = 0;
    if (count <= 0) {
        return window;
    }

    double sumRaw = 0;
    double sumSq = 0;
    float peak = 0;
    float lastY = 0;
    int ok = 0;

    for (int i = 0; i < count; i++) {
        float x = 0;
        float y = 0;
        float z = 0;
        if (readRawAccel(x, y, z)) {
            const float raw = intensityFrom(x, y, z);
            const float residual = fabsf(raw - offsetIntensity);
            sumRaw += raw;
            sumSq += (double)residual * (double)residual;
            if (residual > peak) {
                peak = residual;
            }
            lastY = fabsf(y);
            ok++;
        }
        delay(intervalMs);
    }

    window.successCount = ok;
    window.lastY = lastY;
    window.peak = peak;
    if (ok < (count + 1) / 2) {
        return window;
    }
    window.meanRaw = (float)(sumRaw / (double)ok);
    window.rms = (float)sqrt(sumSq / (double)ok);
    window.ok = true;
    return window;
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
    float x = 0;
    float y = 0;
    float z = 0;
    if (!readRawAccel(x, y, z)) {
        return 0.0;
    }
    Serial.println("x:" + String(fabsf(x)));
    Serial.println("y:" + String(fabsf(y)));
    Serial.println("z:" + String(fabsf(z - 9.8f)));
    return intensityFrom(x, y, z);
}

float VibrationSensor6050::offset() {
  Serial.println("offsetting sensor");
  float sum = 0;
  int ok = 0;
  for (int i = 0; i < 10; i++) {
      float x = 0;
      float y = 0;
      float z = 0;
      if (readRawAccel(x, y, z)) {
          sum += intensityFrom(x, y, z);
          ok++;
      }
      delay(100);
  }
  if (ok == 0) {
      Serial.println("offset failed");
      return 0;
  }
  const float intent = sum / (float)ok;
  Serial.printf("offset complate offsetintencity:%.2f\n", intent);
  return intent;
}
