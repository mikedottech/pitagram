// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Miguel Angel Exposito Sanchez

#ifndef _POWERMGR_H_
#define _POWERMGR_H_

#include <inttypes.h>

class PowerMgr
{
public:
    PowerMgr();
    void init();
    void powerOffPeripherals();
    void powerOnPeripherals();
    void update();

    void enterSleep(bool deep = false);
    void setupWatchdogTimer();
    void powerDownSD();
    void powerUpSD();
    void powerDownEP();
    void powerUpEP();
    int32_t readVcc() const;

    void retainPower();
    void releasePower();
    
    void resetMCUByWDT();

    static bool isBattLow(int32_t voltage_mv);
    
    static uint32_t elapsedSleepTicks(uint32_t lastSleepTicks);
    static uint32_t getCurrentSleepTicks();
    
    static void wdtISR();
    static void busyPinChangeISR();
    static void mfButtonChangeISR();    
    
    static void __incrementSleepTicks();
    
    uint8_t mPowerRetainCount = 0;
    struct SleepRequest
    {
        bool bWantsToSleep = false;
        bool bIsDeepSleep = false;
    };
    
    SleepRequest mPendingSleepRequest;
    
    private:
        void enterSleepInternal();
        void resetPins();
        void evalEnterSleep();
        void enableInt1AsLevelChange();
        void disableInt1AsLevelChange();
        void enableInt0AsLevelChange();
        void disableInt0AsLevelChange();
    static volatile uint32_t sNSleepTicks;    
};

extern PowerMgr g_power;

#endif
