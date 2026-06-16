# Pitagram

An ultra-low-power digital photo frame built around a 7-color e-paper panel
and an 8 MHz ATmega32U4 with **2.5 KB of SRAM**. The whole image processing
pipeline runs offline on a PC; the microcontroller only streams pre-paletized
binary frames from an SD card to the panel, then goes back to sleep for
the rest of the day.

- One image rotation per day, on a 24 h watchdog tick.
- Aggressive power gating: SD and EPD are physically cut from VCC via
  MOSFETs whenever they are not in use.
- Single multi-function button: next / previous / pause-rotation / standby.
- Powered by a recycled Nokia BL-5C-style cell with a USB charging board.
- Measured idle current: **~0.3 mA**.

> **Status:** functional, in daily use. The repo is published as-is from
> a working build; some rough edges remain (see [Known issues](#known-issues)).

---

## Hardware

| Part                 | Notes                                                                 |
| -------------------- | --------------------------------------------------------------------- |
| MCU board            | SparkFun Pro Micro **3.3 V / 8 MHz** (ATmega32U4)                     |
| Display              | Waveshare 5.65" 7-color ACeP e-paper, model **5.65inch e-Paper (F)**, 600 × 448 px |
| Storage              | microSD card (FAT16/FAT32), wired to the same SPI bus as the EPD      |
| SD power gate        | P-MOSFET driven by MCU pin **D6** (high-side switch on SD VCC)        |
| EPD power gate       | P-MOSFET driven by MCU pin **D7** (high-side switch on EPD VCC)       |
| Button               | Momentary push-button to GND on **INT1 / D3** (with internal pull-up) |
| Battery              | Nokia BL-5C-class Li-ion cell (~1000 mAh, 3.7 V nominal)              |
| Charger              | Any TP4056-style USB Li-ion charger module                            |

The SD card and the EPD share the AVR's hardware SPI bus. Only one peripheral
is powered at a time, so chip-select arbitration is implicit — when SD is up,
EPD is dark, and vice versa.

Datasheets for the panel are **not included** in this repository
(Waveshare copyright). You can get them from the official wiki:
<https://www.waveshare.com/wiki/5.65inch_e-Paper_Module_(F)>

---

## Repository layout

```
pitagram/
├── assets/                          # Source files for icons (GIMP .xcf)
├── firmware/
│   └── platformio/pitagram/         # PlatformIO project
│       ├── platformio.ini
│       └── src/
│           ├── main.cpp             # setup() / loop() — thin entry point
│           ├── Pitagram.{cpp,h}     # Application FSM, image rotation
│           ├── PowerMgr.{cpp,h}     # Sleep, WDT, MOSFET gating, VCC sense
│           ├── MFButtonHandler.*    # Multi-click + long-press button
│           ├── ISR.cpp              # AVR vector wiring (WDT, INT0, INT1)
│           ├── epd5in65f.{cpp,h}    # Waveshare EPD driver (extended)
│           ├── epdif.{cpp,h}        # Waveshare SPI HAL (extended)
│           ├── Config.h             # Pins, timings, buffer size
│           └── ImgFormatDefs.h      # PTG binary header
├── tools/
│   ├── convert.py                   # JPEG/PNG → 4bpp .bin with Floyd-Steinberg
│   ├── hash.py                      # Rename outputs to 8.3-safe hashes
│   ├── fixspaces.py                 # Strip spaces from filenames
│   └── requirements.txt
├── Makefile                         # Drives the offline pipeline
├── LICENSE                          # MIT (this project) + third-party notices
└── README.md
```

Photo directories (`source_pictures/`, `converted_pictures/`,
`hashed_pictures/`) are gitignored. Drop your own images into
`source_pictures/` and run `make`.

---

## Offline image pipeline

The MCU never sees JPEGs. Everything is paletized and dithered on a PC
ahead of time so the firmware only has to memcpy bytes from SD to the EPD.

```
source_pictures/*.jpg
        │
        ▼
   tools/convert.py        # EXIF rotate → autocontrast → boxblur → gamma →
        │                  # Lanczos resize → center-crop → Floyd-Steinberg
        │                  # quantize against the 7-color panel palette
        ▼
converted_pictures/*.bin   # 16-byte PTG header + raw 4bpp linear pixels
        │
        ▼
    tools/hash.py          # Optional: rename to 8-char MD5 hash so the
        │                  # AVR SdFat build (no LFN) can read them
        ▼
hashed_pictures/*.bin      # → Copy these to the SD card
```

### Setup

```bash
cd tools
./install_requirements.sh   # creates a venv, installs Pillow + numpy
```

### Run

```bash
make            # build all converted_pictures/*.bin
make hashed     # also build hashed copies for SdFat short-filename mode
make clean      # remove all generated bins
```

### Binary format (`ImgFormatDefs.h` / `convert.py`)

| Offset | Size  | Field   | Value                              |
| -----: | ----: | ------- | ---------------------------------- |
|      0 |     3 | magic   | ASCII `"PTG"`                      |
|      3 |    13 | reserved| zero-filled                        |
|     16 | W·H/2 | pixels  | 4 bpp, two pixels per byte, MSB first, row-major |

The 7-color palette (indices 0–6 used, 7 reserved for the panel's "clean"
color) is the standard Waveshare ACeP mapping:

| Index | Color  | RGB                |
| ----: | ------ | ------------------ |
|     0 | Black  | (0, 0, 0)          |
|     1 | White  | (255, 255, 255)    |
|     2 | Green  | (67, 138, 28)      |
|     3 | Blue   | (100, 64, 255)     |
|     4 | Red    | (191, 0, 0)        |
|     5 | Yellow | (255, 243, 56)     |
|     6 | Orange | (232, 126, 0)      |

---

## Firmware

Built with [PlatformIO](https://platformio.org). Target environment:
`sparkfun_promicro8` (ATmega32U4, 8 MHz). Single dependency: `greiman/SdFat`,
configured for FAT16/FAT32 only and minimal cache to fit in 2.5 KB of SRAM.

### Build & flash

```bash
cd firmware/platformio/pitagram
pio run                          # build
pio run --target upload          # flash via the Pro Micro USB bootloader
pio device monitor               # optional, 115200 baud
```

### State machine

```
              ┌────────────────────┐
              │  STATE_CHANGE_IMG  │  Power up SD → pick next file →
              └─────────┬──────────┘  power up EPD → stream → power both off
                        │
              success   │   error
                        ▼
              ┌────────────────────┐      ┌────────────────────┐
              │ STATE_WAIT_FOR_NEXT│◀────▶│   STATE_WAIT_RETRY │
              └─────────┬──────────┘      └──────────┬─────────┘
                        │                            │
       long press / low │ batt              retry    │
                        ▼                            │
              ┌────────────────────┐                 │
              │  STATE_STANDBY     │                 │
              │  / STATE_LOW_BATT  │                 │
              └────────────────────┘                 │
                        ▲                            │
                        └────────────────────────────┘
```

- Every state ends with the MCU in `SLEEP_MODE_PWR_DOWN`.
- The watchdog timer is the **only periodic wake source** — it fires every
  ~8 s, increments a tick counter and goes back to sleep. All time-based
  logic (24 h image rotation, button debounce) is counted in WDT ticks.
- The button uses external interrupt `INT1` (D3) to wake the MCU
  immediately on user input.
- During EPD operations, the firmware sleeps between BUSY polls instead of
  busy-waiting, so the Ready-state current is essentially the EPD's own.

### Power gating sequence

```
powerUpSD():    set D6 HIGH → P-MOSFET on  → wait settle → SdFat.begin()
powerDownSD():  SdFat.end() → set D6 INPUT → P-MOSFET off (floating gate
                                              pulled up by external R)
```

Identical pattern on D7 for the EPD. The SPI bus pins are returned to
INPUT between transactions so leakage through the (now unpowered)
peripheral does not back-feed VCC through the protection diodes.

### Streaming, not buffering

Frames are 600 × 448 / 2 = **134,400 bytes** — that does not fit in 2.5 KB
of SRAM, obviously. `Epd::EPD_5IN65F_Display_ext_full()` takes a
`RequestData` callback and pumps a **256-byte chunk** at a time:

```
SD file ──► 256 B buffer ──► SPI ──► EPD
```

The EPD driver decides when the next chunk is needed; the callback fills
it straight from the open `SdFat::File`. The whole image lives on SD,
never in RAM.

---

## Usage

Single button on D3:

| Gesture        | Action                                           |
| -------------- | ------------------------------------------------ |
| 1 click        | Next image                                       |
| 2 clicks       | Previous image                                   |
| 3 clicks       | Reset the 24 h rotation timer (keep current longer) |
| Long press     | Standby — clear screen to white and deep sleep   |
| Click in standby | Wake, redisplay current image                  |

A persistent "marker" file on the SD card remembers which image was last
shown, so the rotation survives power cycles and battery swaps.

---

## Known issues

- `tools/convert.py` uses `screen_resolution = (600, 480)` but the panel
  is **600 × 448**. The extra 32 rows produce slightly oversized `.bin`
  files; the firmware ignores them, but cropping with the wrong aspect
  ratio shifts framing. Will be fixed once verified against a few sample
  images.
- `firmware/.../src/main.cpp` keeps a verbatim SdFat example in the
  `#else` branch as a reference snippet. It never compiles. Will either
  get attributed or removed.
- Periodic full-screen "clean" pass to mitigate ghosting is not yet
  scheduled — the panel's specification calls for one every N images.
- Battery-level indicator on screen is not implemented; the firmware only
  uses the bandgap-measured VCC to gate into `STATE_LOW_BATTERY`.

---

## License

Project code: **MIT** — see [LICENSE](./LICENSE).

Bundled third-party drivers retain their original Waveshare MIT-style
headers (see `firmware/.../src/epd5in65f.*` and `epdif.*`). The single
runtime dependency, [greiman/SdFat](https://github.com/greiman/SdFat),
is also MIT.

## Acknowledgements

- **Waveshare** for the 5.65" 7-color e-paper module and the Arduino
  reference driver this project's EPD layer is derived from.
- **Bill Greiman** for SdFat, without which 2.5 KB of SRAM would not be
  enough.
