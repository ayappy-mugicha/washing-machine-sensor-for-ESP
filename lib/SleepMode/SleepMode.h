#ifndef SLEEPMODE_H
#define SLEEPMODE_H


class SleepMode{
    public:
        SleepMode(const char* ntpserver, int  utc ,const char* webhook);
        bool updateLocalTime();
        void gotosleep(int second);
        bool deepsleep(int startHour, int endHour);

    private:
        const char* _NTPSERVER;
        int _UTC;
        const char* _WEBHOOK;
};

#endif
