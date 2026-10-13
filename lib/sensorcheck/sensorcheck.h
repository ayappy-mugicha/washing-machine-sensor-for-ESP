#ifndef SENSORCHECK_H
#define SENSORCHECK_H

#include <Arduino.h>

class SensorCheck {
public:
    bool begin();
    void getsensor();
};

#endif
