# Building Pitagram

Two independent build flows: the **firmware** (PlatformIO) and the
**offline image pipeline** (Python). They share nothing at build time —
the only handoff is the SD card.

---

## Firmware

### Requirements

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) (or the VS Code extension)
- A SparkFun Pro Micro 3.3 V / 8 MHz, or another ATmega32U4 board with the
  same VCC and clock

### Build & flash

```bash
cd firmware/platformio/pitagram
pio run                          # compile
pio run --target upload          # flash via the Pro Micro USB bootloader
pio device monitor               # optional, 115200 baud
```

The first build will pull the only declared dependency, `greiman/SdFat`,
into `.pio/libdeps/`.

### Wiring

| Signal             | MCU pin | Notes                                             |
| ------------------ | ------- | ------------------------------------------------- |
| SPI MOSI/MISO/SCK  | D16 / D14 / D15 | Hardware SPI, shared between SD and EPD   |
| SD CS              | D4         | Active during SD transactions only             |
| EPD CS             | D10        | Active during EPD transactions only            |
| EPD DC             | D9         | Data / command select                          |
| EPD RST            | D8         | Active-low reset                               |
| EPD BUSY           | D3 / INT0  | Wakes the MCU from sleep while the panel works |
| SD VCC gate        | D6         | High-side P-MOSFET                             |
| EPD VCC gate       | D7         | High-side P-MOSFET                             |
| Button             | D2 / INT1  | To GND, internal pull-up                       |
| Battery sense      | A0         | Via 1:2 divider, read against the 1.1 V bandgap |

Exact assignments live in `firmware/platformio/pitagram/src/Config.h`.

---

## Image pipeline

### Requirements

- Python 3.10+
- [Pillow](https://pillow.readthedocs.io/) and NumPy (installed by the
  helper script below)

### Setup

```bash
cd tools
./install_requirements.sh        # creates a venv, installs Pillow + NumPy
```

### Run

From the repository root, with source images in `source_pictures/`:

```bash
make                # build all converted_pictures/*.bin
make hashed         # also build hashed copies for SdFat short-filename mode
make clean          # remove all generated bins
```

### What ends up on the SD card

Copy the contents of `hashed_pictures/` to the root of a FAT16/FAT32 SD
card. The firmware iterates files alphabetically and remembers the last
displayed name in a marker file at the root.

The hashed step exists because the firmware builds SdFat without long
filename support to save SRAM — the 8.3 hashed name is what the AVR sees.
