// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Miguel Angel Exposito Sanchez

#include "Pitagram.h"
#include "Debug.h"

#include "ImgFormatDefs.h"

#include <SPI.h>

#define SPI_CLOCK SD_SCK_MHZ(50)
#define SD_CONFIG SdSpiConfig(kCfgSDChipSelect, SHARED_SPI, SPI_CLOCK)

Pitagram g_pitagram;

volatile bool Pitagram::sbPendingMFButtonChange = false;

Pitagram::Pitagram()
{    
    mButtonHandler.setCallback(onMFButtonEvent, this);
}

void Pitagram::init()
{   

#if 0
    g_power.powerUpEP();
    delay(500);
    initEPD();    
    mEpd.Clear(EPD_5IN65F_WHITE);
    while(1);
#endif
    dbgPrintln("Boot");
    
    transitionToState(STATE_CHANGE_IMG);
}

bool Pitagram::initSD()
{
    sd.end();
    dbgPrintln("Initializing SD card...");
    // see if the card is present and can be initialized:
    if (!sd.begin(SD_CONFIG))
    {
        dbgPrintln("Card failed, or not present");
        return false;            
    }
    dbgPrintln("card initialized.");

    return true;
}

bool Pitagram::initEPD() const
{
    return (mEpd.Init() == 0);
}

bool Pitagram::checkHeader(File &f) const
{    
    ImgFmtHeader header;
    const unsigned int dataRead = f.read(&header, kImgFmtHeaderLength);

    if(dataRead < kImgFmtHeaderLength)
        return false;

    return (strncmp(header.magic, IMG_FMT_HEADER_MAGIC, kImgFmtMagicHeaderLength) == 0);
}

void Pitagram::update()
{
    mButtonHandler.update();

    evalBattery();

    if(mPendingNextState != STATE_INVALID)
    {
        const State tmp = mPendingNextState;
        mPendingNextState = STATE_INVALID;
        transitionToStateInternal(tmp);        
    }
    
    switch(mState)
    {
        case STATE_WAIT_FOR_NEXT:
            if(isTimeToShowNextImage())
                //transitionToState(STATE_CHANGE_IMG);
                g_power.resetMCUByWDT();
            else
                g_power.enterSleep(true);
        break;
        case STATE_WAIT_RETRY:
            if(isWaitTimeOver())
                transitionToState(STATE_CHANGE_IMG);                
            else
                g_power.enterSleep(true);
        break;
        case STATE_LOW_BATTERY:
        case STATE_STANDBY:
            g_power.enterSleep(true);
        break;
        default:        
        break;
    }
}

void Pitagram::evalBattery()
{
    do
    {
        if (PowerMgr::getCurrentSleepTicks() == 0 ||
            isTimeToCheckBattery() ||
            mState == STATE_LOW_BATTERY)
        {
            mLastBatteryTick = PowerMgr::getCurrentSleepTicks();
            const long reading = g_power.readVcc();
            const bool bBatLow = g_power.isBattLow(reading);

            dbgPrint("VCC = ");
            dbgPrintln(reading);
            dbgPrint("bBatLow = ");
            dbgPrintln(bBatLow);
            dbgPrint("bBatLowCounts = ");
            dbgPrintln(mBattLowCounter + 1);

            if (bBatLow)
            {
                if (++mBattLowCounter < kCfgBattLowCounts)
                    break;
                else
                    mBattLowCounter = kCfgBattLowCounts;
            }
            else
            {
                mBattLowCounter = 0;
            }

            if (mState != STATE_LOW_BATTERY)
            {
                if (bBatLow)
                    transitionToState(STATE_LOW_BATTERY);
            }
            else
            {
                if (!bBatLow)
                    transitionToState(STATE_WAIT_FOR_NEXT);
            }
        }
    } while (0);
}

void Pitagram::onMFButtonChange(bool bIsPressed)
{
    dbgPrint("Button: ");
    dbgPrintln(bIsPressed);
    Serial1.flush();    // Watch this
    mButtonHandler.triggerButtonChange(bIsPressed);    
}

unsigned long Pitagram::dataProviderCallback(const unsigned long size, unsigned char* buffer, const void* userdata)
{   
    //g_pitagram.sd.card()->chip();
    File* f = (File*)userdata;
    if(!f->available())
        return 0;
    return f->read(buffer, kCfgBufferSize);
}

void Pitagram::waitProviderCallback(unsigned int pin, bool high)
{
    while(digitalRead(pin) != high)
    {
        g_power.enterSleep();
    }
}

void Pitagram::sendFileToDisplay(const File &f)
{
    mEpd.EPD_5IN65F_Display_ext_full(buffer, kCfgBufferSize, Pitagram::dataProviderCallback, Pitagram::waitProviderCallback, &f);
}

void Pitagram::transitionToState(State nextState)
{
    mPendingNextState = nextState;
}

void Pitagram::transitionToStateInternal(State nextState)
{
    if(mState == nextState)
        return;
    
    dbgPrint("FSM ");
    dbgPrint(mState);
    dbgPrint(" -> ");
    dbgPrintln(nextState);

    switch(nextState)
    {
        case STATE_CHANGE_IMG:
        {
            dbgPrintln("Next img");
            bool bError = displayFile();
            transitionToState(bError ? STATE_WAIT_RETRY : STATE_WAIT_FOR_NEXT);
        }
        break;
        case STATE_WAIT_FOR_NEXT:
        {
            dbgPrintln("Sleeping for next img");            
            if(isTimeToShowNextImage())
            {
                dbgPrintln("Scheduling next wakeup");
                mLastImgChangeTick = PowerMgr::getCurrentSleepTicks();
            } else {
                dbgPrintln("Already waiting.");
            }
        }
        break;
        case STATE_WAIT_RETRY:
        {
            dbgPrintln("Sleeping for retry");
            scheduleWaitTimeout(kCfgRetryIntervalTicks);
        }
        break;
        case STATE_LOW_BATTERY:
            dbgPrintln("Entering low battery mode");
            // If it was in standby while we entered low battery mode,
            // then the screen was already blank
            if(mState != STATE_STANDBY)
                break;
        case STATE_STANDBY:
        {
            g_power.powerUpEP();
            initEPD();
            mEpd.Clear(EPD_5IN65F_WHITE);
            endEPD();
            g_power.powerDownEP();
        }
        break;
        default:
        break;
    }

    mState = nextState;
}

void Pitagram::scheduleWaitTimeout(uint32_t ticksToWaitFor)
{
    mLastWaitTick = PowerMgr::getCurrentSleepTicks();
    mCurrentWaitIntervalTicks = ticksToWaitFor;
}

bool Pitagram::displayFile(DisplayFileCommand command)
{
    bool bError = false;
    dbgPrintln("Power up EP");                
    g_power.powerUpEP();
    dbgPrintln("Power up SD");                
    g_power.powerUpSD();    
    dbgPrintln("Init");

    SPI.begin();

    if(initSD() && initEPD())
    {
        if(m_lastFileName.data[0] == 0)
        {
            tryReadFileMarker(m_lastFileName);
        }
        dbgPrintln("Show");
        {
            File file;
            switch(command)
            {
                case DisplayFileCommand::Next:
                    openNextFile(m_lastFileName, file);
                    break;
                case DisplayFileCommand::Prev:
                    openPrevFile(m_lastFileName, file);
                    break;
                case DisplayFileCommand::Current:
                    if(m_lastFileName.data[0] != 0)
                    {
                        file = sd.open(m_lastFileName.data, O_READ);
                        if(file)
                        {
                            checkHeader(file);
                        }
                    }
                    break;
            }            

            if(file)
            {
                static char sfname[kFileNameLength] = {0};                
                file.getSFN(sfname, kFileNameLength);

                dbgPrint("File ");
                dbgPrint(sfname);
                dbgPrintln(" open. Starting upload...");
                sendFileToDisplay(file);
                file.close();
                strncpy(m_lastFileName.data, sfname, kFileNameLength);
                m_lastFileName.data[kFileNameLength-1] = 0;
                tryWriteFileMarker(m_lastFileName);
            } else {
                dbgPrintln("ERROR");
                bError = true;
            }
        }
    } else {
        bError = true;
    }
    endSD();
    endEPD();
    g_power.powerDownEP();
    SPI.end();
    return bError;
}

void Pitagram::endEPD() const
{
    mEpd.Pof();
    mEpd.Sleep();
}

void Pitagram::endSD()
{
    sd.end();
}

File Pitagram::openFileCommon(const FileName &lastFileName, bool bIsNext) const
{
    bool bLastFound = false;
    File dir;    
    File file;
    static FileName prevFile = {};
    static char sfname[kFileNameLength] = {0};
    
    if (!dir.open("/"))
    {
        dbgPrintln("Failed to open root");
        return File();
    }

    if(lastFileName.data[0] == 0)
        bLastFound = true;

    for(;;)
    {
        file = dir.openNextFile();
        if(!file)
        {            
            dbgPrintln("No more files");
            dir.rewindDirectory();
            bLastFound = true;
        }
        
        file.getSFN(sfname, kFileNameLength);
        
        dbgPrintln(sfname);
        //dbgPrint("EXT: ");
        //dbgPrint(fname + (strlen(fname) -4));
        if(strcmp(sfname + (strlen(sfname) - 4), IMG_FMT_FILE_EXTENSION) != 0 ||
            !checkHeader(file))
        {
            dbgPrintln("Skipping file");
            file.close();
        } else if(!bLastFound && strncmp(lastFileName.data, sfname, kFileNameLength) == 0)
        {
            // We found the last file
            dbgPrintln("Found last file");
            if(!bIsNext)
            {
                //dbgPrint("PREV FILE ");
                //dbgPrint(prevFile.data);
                // We're looking for the previous file
                // Do we have a previous file name?
                if(prevFile.data[0] != 0)
                {
                    file.close();
                    file.open(prevFile.data, FILE_READ);
                    if (!checkHeader(file))
                    {
                        dbgPrintln("Bad header");
                        file.close();
                        return File();
                    }
                    break;
                } else {
                    // We don't. Return the current one
                    break;
                }
            } else {
                // We're looking for the next one.
                // Do one more iteration
                file.close();
                bLastFound = true;
            }
        } else if(bLastFound)
        {
            // We found the file
            break;
        } else {
            // None of the above
            file.close();
        }
        strncpy(prevFile.data, sfname, kFileNameLength);
        prevFile.data[kFileNameLength-1] = 0;
    }
    dir.close();
    return file;
}

void Pitagram::tryDisplayFile(DisplayFileCommand command, bool bForce)
{
    if(mState == STATE_WAIT_FOR_NEXT || bForce)
    {
        const bool bError = displayFile(command);
        if(bError)
            transitionToState(STATE_WAIT_RETRY);
    }
}

void Pitagram::onMFButtonEvent(const MFButtonEvent event, void* pUserData)
{
    Pitagram* pThis = static_cast<Pitagram*>(pUserData);
    
    // Wake up by button press
    if(pThis->mState == STATE_STANDBY)
    {
        pThis->tryDisplayFile(DisplayFileCommand::Current, true);
        pThis->transitionToState(STATE_WAIT_FOR_NEXT);
        return;
    }

    dbgPrint(">> Click ");
    dbgPrintln(event.nClicks);
    switch(event.type)
    {
        // Multi click
        case MFButtonEventType::Click:
            switch(event.nClicks)
            {
                // Single click
                case 1:
                    pThis->tryDisplayFile(DisplayFileCommand::Next);
                break;
                // Double click
                case 2:
                    pThis->tryDisplayFile(DisplayFileCommand::Prev);
                break;
                // Triple click
                case 3:
                    pThis->mLastImgChangeTick = PowerMgr::getCurrentSleepTicks();
                break;
                default:
                break;
            }
            
            break;
        // Long click
        case MFButtonEventType::LongClick:
            dbgPrintln(">> LongClick");
            pThis->transitionToState(STATE_STANDBY);
            break;
        default:
            break;
    }
}

void Pitagram::openPrevFile(const FileName &lastFileName, File& file) const
{
    file = openFileCommon(lastFileName, false);
}

void Pitagram::openNextFile(const FileName &lastFileName, File& file) const
{
    file = openFileCommon(lastFileName, true);
}

bool Pitagram::isTimeToShowNextImage() const
{
    // dbgPrint("ELAPSED: ");
    // dbgPrint(PowerMgr::elapsedSleepTicks(mLastImgChangeTick));
    // dbgPrint("/");
    // dbgPrintln(kCfgImageChangeIntervalTicks);
    return (PowerMgr::elapsedSleepTicks(mLastImgChangeTick) >= kCfgImageChangeIntervalTicks);
}

bool Pitagram::isWaitTimeOver() const
{
    return (PowerMgr::elapsedSleepTicks(mLastWaitTick) >= mCurrentWaitIntervalTicks);
}

bool Pitagram::isTimeToCheckBattery() const
{
    return (PowerMgr::elapsedSleepTicks(mLastBatteryTick) >= kCfgBattIntervalTicks);    
}

void Pitagram::tryWriteFileMarker(const FileName& fname) const
{
    SdFile file;    
    if(file.open(MARKER_FILE_NAME, O_READ | O_WRITE | O_CREAT | O_TRUNC))
    {
        file.write(fname.data);
        file.close();
        dbgPrintln("File marker written");
    } else {
        dbgPrintln("Can't write file marker!");
    }
}

void Pitagram::tryReadFileMarker(FileName& fname) const
{
    SdFile file;

    if(file.open(MARKER_FILE_NAME, FILE_READ))
    {
        fname = FileName{};
        file.read(fname.data, kFileNameLength);
        fname.data[kFileNameLength] = 0;
        file.close();
        dbgPrintln("File marker read");
    }
}

void Pitagram::mfButtonChangeISR()
{
    g_pitagram.mButtonHandler.onButtonChangeISR();
}

