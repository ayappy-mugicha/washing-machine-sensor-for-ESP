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
    WasherStatus getcombinedData();
private:
    MPU6050 _accel;
};

#endif
