// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Miguel Angel Exposito Sanchez

#ifndef _PITAGRAM_H_
#define _PITAGRAM_H_

#include "epd5in65f.h"
#include "SdFat.h"
#include "PowerMgr.h"
#include "MFButtonHandler.h"
#include "Config.h"
#include "Definitions.h"

class Pitagram
{
public:
    Pitagram();
    void init();
    void update();

    void evalBattery();

    void onMFButtonChange(bool bIsPressed);

    static void mfButtonChangeISR();

private:
    typedef enum {
        STATE_INVALID = -1,
        STATE_IDLE = 0,
        STATE_CHANGE_IMG,
        STATE_WAIT_FOR_NEXT,
        STATE_WAIT_RETRY,
        STATE_LOW_BATTERY,
        STATE_STANDBY
    } State;

    enum class DisplayFileCommand {
        Current,
        Prev,
        Next
    };

    Epd mEpd;
    uint32_t mLastImgChangeTick = 0xFFFFFFFF;
    uint32_t mLastWaitTick = 0;
    uint32_t mLastBatteryTick = 0;
    uint32_t mCurrentWaitIntervalTicks = 0;
    uint8_t mBattLowCounter = 0;
    State mState = STATE_INVALID;
    State mPendingNextState = STATE_INVALID;

    typedef struct
    {
        char data[kFileNameLength + 1];
    } FileName;
    
    FileName m_lastFileName = {};
    
    SdFat sd;

    static unsigned long dataProviderCallback(const unsigned long size, unsigned char* buffer, const void* userdata);
    static void waitProviderCallback(unsigned int pin, bool high);

    unsigned char buffer[kCfgBufferSize];
    
    typedef bool (*KeepSleepingConditionCallback)();

    bool checkHeader(File &f) const;
    void sendFileToDisplay(const File &f);
    
    void tryWriteFileMarker(const FileName &fname) const;
    void tryReadFileMarker(FileName& fname) const;
    
    void transitionToState(State nextState);
    void transitionToStateInternal(State nextState);

    void scheduleWaitTimeout(uint32_t ticksToWaitFor);

    bool displayFile(DisplayFileCommand command = DisplayFileCommand::Next);

    bool initSD();
    void endSD();
    bool initEPD() const;
    void endEPD() const;
    
    bool isTimeToShowNextImage() const;
    bool isWaitTimeOver() const;
    bool isTimeToCheckBattery() const;

    void openNextFile(const FileName& lastFileName, File& file) const;
    void openPrevFile(const FileName& lastFileName, File& file) const;
    File openFileCommon(const FileName& lastFileName, bool bIsNext) const;

    void tryDisplayFile(DisplayFileCommand command = DisplayFileCommand::Next, bool bForce = false);

    static void onMFButtonEvent(const MFButtonEvent event, void* pUserData);

    MFButtonHandler mButtonHandler;

    static volatile bool sbPendingMFButtonChange;
};

extern Pitagram g_pitagram;

#endif
