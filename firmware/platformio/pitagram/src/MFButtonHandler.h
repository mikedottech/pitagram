#pragma once
#include <inttypes.h>

enum class MFButtonEventType : uint8_t
{
    Invalid,
    Click,    
    LongClick
};

struct MFButtonEvent
{
    MFButtonEventType type;
    uint8_t nClicks;
};

class MFButtonHandler
{
    public:

        void update();

        void onButtonChangeISR();

        void triggerButtonChange(bool pressed);

        typedef void (*ButtonEventCallback)(const MFButtonEvent evt, void* pUserData);

        inline void setCallback(ButtonEventCallback cb, void* pUserData) { mCallback = cb; mpCallbackUserData = pUserData; }

    private:
        enum class State
        {
            Idle,
            Hold,
            WaitNext            
        };

        MFButtonEvent mPendingEvent = {MFButtonEventType::Invalid, 0};
                
        uint32_t mStartTimestamp = 0;
        uint32_t mDebounceStartTimestamp = 0;
        uint8_t mClickCount = 0;
        State mState = State::Idle;
        ButtonEventCallback mCallback = nullptr;
        void* mpCallbackUserData = nullptr;
        bool bLastPressed = false;
        volatile bool bIsDebouncing = false;
        bool bWasDebouncing = false;
        
        void transitionToState(State next);
        void notifyCallback(const MFButtonEvent evt);
};
