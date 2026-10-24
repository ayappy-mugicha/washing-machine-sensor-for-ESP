#ifndef VIBRATIONSENSOR6050_H
#define VIBRATIONSENSOR6050_H

#include <Arduino.h>
#include "MPU6050.h"

struct WasherStatus
{
    float x;
    float y;
    float z;
    float intensity;
    bool ok;
};

struct VibrationWindow
{
    bool ok;
    float rms;
    float meanRaw;
    float peak;
    float tiltDeg;
    int successCount;
    int openCount;
};

class VibrationSensor6050 {
public:
    VibrationSensor6050();
    bool begin();
    bool setup();
    bool sensor_exsis();
    float getIntensity();
    float offset();
    bool end();
    bool readRawAccel(float& x, float& y, float& z);
    WasherStatus getcombinedData();
    VibrationWindow sampleWindow(float offsetIntensity, int count, int intervalMs,
                                 float openDoorDeg = 30.0f);
    static float intensityFrom(float x, float y, float z);
private:
    MPU6050 _accel;
};

#endif
