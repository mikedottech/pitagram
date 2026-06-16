#pragma once

#include <inttypes.h>

enum class SleepInterval : uint8_t
{
    SLEEP_1SEC = 1,
    SLEEP_2SEC = 2,
    SLEEP_4SEC = 4,
    SLEEP_8SEC = 8
};

constexpr uint32_t kSleepInterval = static_cast<uint32_t>(SleepInterval::SLEEP_8SEC);

// ====================== PIN ASSIGNMENTS ======================
constexpr uint8_t kCfgEPDGatePin                    = 7U;
constexpr uint8_t kCfgSDGatePin                     = 6U;
constexpr uint8_t kCfgSDChipSelect                  = 4U;
constexpr uint8_t kCfgMFButtonPin                   = 2U;

constexpr uint8_t kCfgEPDRSTPin                     = 8U;
constexpr uint8_t kCfgEPDDCPin                      = 9U;
constexpr uint8_t kCfgEPDCSPin                      = 10U;
constexpr uint8_t kCfgEPDBusyPin                    = 3U;

constexpr uint8_t kCfgSPIMISOPin                    = 14U; // (IN)  -> SD MISO
constexpr uint8_t kCfgSPIMOSIPin                    = 16U; // (OUT) -> SD MOSI + EPD DIN
constexpr uint8_t kCfgSPISCLKPin                    = 15U; // (CLK) -> SD CLK + EPD CLK


constexpr uint8_t kCfgBattReadSamples               = 64U;      // Number of samples to aggregate
constexpr int32_t kCfgBattLowmv                     = 3050;    // Millivolts

// ========================== TIMERS ===========================
constexpr uint32_t kCfgImageChangeIntervalSeconds   = 60UL*60UL*24UL;
constexpr uint32_t kCfgRetryIntervalSeconds         = 8U;

constexpr uint32_t kCfgImageChangeIntervalTicks     = kCfgImageChangeIntervalSeconds / kSleepInterval;
constexpr uint32_t kCfgRetryIntervalTicks           = kCfgRetryIntervalSeconds / kSleepInterval;
constexpr uint32_t kCfgBattIntervalTicks            = 4U;
constexpr uint32_t kCfgButtonDoubleClickTime        = 500U;
constexpr uint32_t kCfgButtonLongClickTime          = 1000U;
constexpr uint32_t kCfgButtonDebounceTime           = 30U;

// =========================== MISC ============================
constexpr uint8_t  kCfgBattLowCounts                = 3U;
constexpr uint32_t kCfgBufferSize                   = 256U;

constexpr uint32_t kCfgDebugBaudRate                = 115200U;


#define MF_BUTTON_IS_EINT1