#ifndef SLEEPMODE_H
#define SLEEPMODE_H

#include <stdint.h>

class SleepMode{
    public:
        SleepMode(const char* ntpserver, int  utc ,const char* webhook);
        bool updateLocalTime();
        bool localHour(int& hour);
        void gotosleep(int second);
        bool deepsleep(int startHour, int endHour, uint32_t* monotonicSec = nullptr);

    private:
        const char* _NTPSERVER;
        int _UTC;
        const char* _WEBHOOK;
};

#endif
