// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Miguel Angel Exposito Sanchez

#include "PowerMgr.h"

#include "epd5in65f.h"

#include "Debug.h"

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>

#include <assert.h>

#include <SPI.h>

#include "Config.h"

// Max measured EPD current: 26 mA
// Max measured SD current: 35 mA (17.4 idle)

PowerMgr g_power;

volatile uint32_t PowerMgr::sNSleepTicks = 0;

PowerMgr::PowerMgr()
{
}

void PowerMgr::resetMCUByWDT()
{
    cli();
    
    // Clear watchdog reset flag
    MCUSR &= ~(1 << WDRF);
    
    // Enable change of WDE and prescaler
    WDTCSR = (1 << WDCE) | (1 << WDE);
    WDTCSR = (1 << WDE); // 16ms timeout, reset MCU
    
    // Enable global interrupts
    sei();

    while(1);
}

void PowerMgr::init()
{
    delay(2000); // Allow bootloader to take control
    // Disable interrupts temporarily
    cli();
    
    // Clear watchdog reset flag
    MCUSR &= ~(1 << WDRF);
    
    // Enable change of WDE and prescaler
    WDTCSR = (1 << WDCE) | (1 << WDE);
    
    // Set Watchdog Timer to interrupt every 8s
    WDTCSR = (1 << WDIE) | (1 << WDP3) | (1 << WDP0); // 8s timeout, interrupt only
    
    // Enable global interrupts
    sei();
    
    powerOffPeripherals();
    powerOnPeripherals();

    resetPins();

#if defined(MF_BUTTON_IS_EINT1)
    enableInt1AsLevelChange();
#endif

    USBDevice.detach();

#ifdef DEBUG
    DbgSerial.begin(kCfgDebugBaudRate);
#endif
}

void PowerMgr::powerOffPeripherals()
{
    power_all_disable();
}

void PowerMgr::powerOnPeripherals()
{
#ifdef DEBUG
    power_usart0_enable();
    power_usart1_enable();
    power_usb_enable();
#endif
    power_spi_enable();    
    power_timer0_enable(); // Used by Arduino for delay()    
    // SPI registers need to be manually reset following power_spi_disable()
    // or power_all_disable()
    SPCR = 0;
    SPSR = 0;
}

void PowerMgr::busyPinChangeISR()
{
    // TODO: Enable or disable this interrupt depending on EPD power state
    //dbgPrintln("<<CHANGE>>");
    //dbgPrintln(digitalRead(BUSY_PIN));
}

void PowerMgr::__incrementSleepTicks()
{
    dbgPrintln("<<WDT>>");
    ++PowerMgr::sNSleepTicks;
}

void PowerMgr::enterSleepInternal()
{
    const bool deep = mPendingSleepRequest.bIsDeepSleep;
    dbgPrintln("Going to sleep");
#ifdef DEBUG
    DbgSerial.flush();
#endif
    if(deep)
    {
        dbgPrintln("<DEEP SLEEP>");
        SPI.end();
#if defined(DEBUG)
        DbgSerial.end();
#endif
        resetPins();
    }

    if(deep)
	    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    else
        set_sleep_mode(SLEEP_MODE_PWR_SAVE);
    
    cli();
	sleep_enable();
    
    if(deep)
        powerOffPeripherals();

    sei();

	// Now enter sleep mode.
	sleep_mode();

	if(deep)
    {
        powerOnPeripherals();

#ifdef DEBUG
        //USBDevice.attach();
        DbgSerial.begin(kCfgDebugBaudRate);
#endif
    }
    
    dbgPrintln("Coming back from sleep!");
    // Clear pending request
    mPendingSleepRequest.bWantsToSleep = false;
}

void PowerMgr::resetPins()
{
    for (int i = 0; i < 20; ++i)
    {
        pinMode(i, 
            (i == kCfgMFButtonPin)
                ? INPUT_PULLUP : INPUT
            );
    }
}

void PowerMgr::evalEnterSleep()
{
    if(mPowerRetainCount == 0 && mPendingSleepRequest.bWantsToSleep)
        enterSleepInternal();
}

void PowerMgr::update()
{
    evalEnterSleep();
}

void PowerMgr::enterSleep(bool deep)
{
    mPendingSleepRequest.bWantsToSleep = true;
    mPendingSleepRequest.bIsDeepSleep = deep;
    evalEnterSleep();
}

void PowerMgr::powerDownSD()
{
    pinMode(kCfgSDGatePin, INPUT);
}

void PowerMgr::powerUpSD()
{
    pinMode(kCfgSDChipSelect, OUTPUT);
    digitalWrite(kCfgSDChipSelect, HIGH);
    pinMode(kCfgSDGatePin, OUTPUT);
    digitalWrite(kCfgSDGatePin, HIGH);
    delay(300);    
}

void PowerMgr::enableInt1AsLevelChange()
{
    EICRA |= (1 << ISC10);     // ISC11 = 0, ISC10 = 1 => CHANGE
    EICRA &= ~(1 << ISC11);
    EIMSK |= (1 << INT1);      // Enable INT1    
}

void PowerMgr::disableInt1AsLevelChange()
{    
    EIMSK &= ~(1 << INT1);
}

void PowerMgr::enableInt0AsLevelChange()
{
    EICRA |= (1 << ISC00);     // ISC11 = 0, ISC10 = 1 => CHANGE
    EICRA &= ~(1 << ISC01);
    EIMSK |= (1 << INT0);      // Enable INT1    
}

void PowerMgr::disableInt0AsLevelChange()
{    
    EIMSK &= ~(1 << INT0);
}

void PowerMgr::powerDownEP()
{
    disableInt0AsLevelChange();
    pinMode(kCfgEPDGatePin, INPUT);
}

void PowerMgr::powerUpEP()
{
    pinMode(kCfgEPDGatePin, OUTPUT);
    digitalWrite(kCfgEPDGatePin, HIGH);
    delay(300);
    pinMode(BUSY_PIN, INPUT);
    enableInt0AsLevelChange();
    delay(300);
}

void PowerMgr::wdtISR()
{
    PowerMgr::__incrementSleepTicks();
}


int32_t PowerMgr::readVcc() const
{
    int32_t agg = 0;
    int32_t result;    //voltage number reading in counted loop; also used later for calculation of voltage

    power_adc_enable();
    delay(100);

    // Read 1.1V reference against AVcc
    // set the reference to Vcc and the measurement to the internal 1.1V reference
    //choose the right register settings for the processor in use
    #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
        ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
    #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
        ADMUX = _BV(MUX5) | _BV(MUX0);
    #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
        ADMUX = _BV(MUX3) | _BV(MUX2);
    #else
        ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
    #endif

    for (int count=0; count < kCfgBattReadSamples; count++){
        delay(2); // Wait for Vref to settle
        ADCSRA |= _BV(ADSC); // Start conversion
        while (bit_is_set(ADCSRA,ADSC)); // measuring
        // read it a second time
        ADCSRA |= _BV(ADSC); // Start conversion
        while (bit_is_set(ADCSRA,ADSC)); // measuring

        uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
        uint8_t high = ADCH; // unlocks both

        result = (high<<8) | low;
        agg = agg + result;  //collect and add up nsamples readings
        delay(10);
    }

    power_adc_disable();

    // we dont find the average here, it gives better resolution to multiply by the calibration constant first
    // result = agg / nsamples  

    result = 1115702L * kCfgBattReadSamples / agg; // 1115702 = 1.0896 * 1024 * 1000  - calibration constant, different for each processor
    return result; // Vcc in millivolts
  }

  void PowerMgr::retainPower()
  {
    assert(mPowerRetainCount < 255);
    ++ mPowerRetainCount;
    dbgPrint("Retain pwr: ");
    dbgPrintln(mPowerRetainCount);
  }

  void PowerMgr::releasePower()
  {
    if(mPowerRetainCount > 0)
        -- mPowerRetainCount;
    dbgPrint("Release pwr: ");
    dbgPrintln(mPowerRetainCount);
  }

  bool PowerMgr::isBattLow(int32_t voltage_mv)
  {
    return voltage_mv <= kCfgBattLowmv;
  }

  uint32_t PowerMgr::elapsedSleepTicks(uint32_t lastSleepTicks)
  {
    uint32_t sleepTicks = PowerMgr::getCurrentSleepTicks();
    int64_t diff = (int64_t)sleepTicks - (int64_t)lastSleepTicks;

    return (
        diff < 0 ?
        (((uint32_t)-1) - lastSleepTicks) + sleepTicks : // Overflow
        (uint32_t)diff
    );
  }

  uint32_t PowerMgr::getCurrentSleepTicks()
  {
    cli();
    const uint32_t sleepTicks = PowerMgr::sNSleepTicks;
    sei();

    return sleepTicks;
  }
