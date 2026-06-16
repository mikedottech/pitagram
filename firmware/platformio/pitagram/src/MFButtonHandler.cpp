// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Miguel Angel Exposito Sanchez

#include "MFButtonHandler.h"
#include "PowerMgr.h"
#include <Arduino.h>
#include "Debug.h"

#include "Config.h"

void MFButtonHandler::update()
{
    {
        noInterrupts();
        
        if(bWasDebouncing != bIsDebouncing)
        {
            if(bIsDebouncing)
                g_power.retainPower();
            else
                g_power.releasePower();
        }

        bWasDebouncing = bIsDebouncing;

        if(bIsDebouncing)
        {
            if(millis() - mDebounceStartTimestamp >= kCfgButtonDebounceTime)
            {            
                bIsDebouncing = false;
                triggerButtonChange(!digitalRead(kCfgMFButtonPin));
            }
        }

        interrupts();
    }

    switch(mState)
    {
        case State::Idle:
            break;
        case State::Hold:
            if(millis() - mStartTimestamp >= kCfgButtonLongClickTime)
            {   
                mPendingEvent = {
                    MFButtonEventType::LongClick,
                    0
                };
                transitionToState(State::Idle);
            }
            break;
        case State::WaitNext:
            if(millis() - mStartTimestamp >= kCfgButtonDoubleClickTime)
            {
                mPendingEvent = {
                    MFButtonEventType::Click,
                    mClickCount
                };
                transitionToState(State::Idle);
            }
            break;
        default:
            break;
    }

    if(mPendingEvent.type != MFButtonEventType::Invalid)
    {
        notifyCallback(mPendingEvent);
        mPendingEvent = {
            MFButtonEventType::Invalid,
            0
        };
    }
}

void MFButtonHandler::onButtonChangeISR()
{
    if(!bIsDebouncing)
    {
        bIsDebouncing = true;
        mDebounceStartTimestamp = millis();
    }
}

void MFButtonHandler::triggerButtonChange(bool pressed)
{
    if(pressed == bLastPressed)
        return;

    if(pressed)
    {    
        ++ mClickCount;
        mStartTimestamp = millis();
    }

    switch(mState)
    {        
        case State::Idle:
            if(pressed)
            {
                transitionToState(State::Hold);
            }
            break;
        case State::Hold:
            if(!pressed)
                transitionToState(State::WaitNext);
            break;
        default:
            break;
    }

    bLastPressed = pressed;
}

void MFButtonHandler::transitionToState(MFButtonHandler::State next)
{
    if(mState == next)
        return;

    dbgPrint("MF STATE: ");
    dbgPrintln((int)next);

    switch(next)
    {
        case State::Idle:
            mClickCount = 0;
            g_power.releasePower();
            break;
        case State::Hold:
            g_power.retainPower();
            break;
        default:
            break;
    }

    mState = next;    
}

void MFButtonHandler::notifyCallback(const MFButtonEvent evt)
{
    if(mCallback)
        mCallback(evt, mpCallbackUserData);
}
