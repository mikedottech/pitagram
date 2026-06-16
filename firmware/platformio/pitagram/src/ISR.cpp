#include <Arduino.h>
#include "PowerMgr.h"
#include "Pitagram.h"
#include "Config.h"
#include "Debug.h"

ISR(WDT_vect)
{
    PowerMgr::wdtISR();
}

ISR(INT1_vect) {
    Pitagram::mfButtonChangeISR();    
}

ISR(INT0_vect) {    
}
