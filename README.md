# FSAE Tire Temperature System

A Melexis IR Sensor Program and Setup Guide

**University of Colorado Formula SAE**

---

## Introduction

Every object emits a wavelength in the infrared spectrum. According to the Stefan–Boltzmann law, the wavelength emitted from an object is proportional to its temperature. Infrared sensors read the infrared energy being emitted from the object, produce an electrical signal, and send it as data to be processed. The Melexis MLX90614 Infrared Thermometer requires I²C protocol to communicate. The MLX90614 has two interfaces — I²C and PWM (pulse width modulation) — and will operate with either a 3.3V or 5V connection. The sensor contains a 17-bit ADC and a custom DSP chip to achieve high accuracy and resolution of objects being measured.

This system extends the single-sensor reference design to four corners (FL / FR / RL / RR) with CAN bus output to a MoTeC C125 logger or M1 ECU at 10 Hz.

```
MLX90614 ×4 ──I²C──► ATmega328P ──SPI──► MCP2515 ──CAN──► MoTeC C125 / M1
                       (16 MHz)            (500 kbps)
```

---

## Repository Structure

```
fsae-tire-temp/
├── firmware/
│   ├── tire_temp_4wheel/       # Main acquisition sketch (upload to car)
│   │   └── tire_temp_4wheel.ino
│   ├── change_address/         # One-time utility: set each sensor's I²C address
│   │   └── change_address_v2.ino
│   └── lib/
│       └── I2C/                # I²C Master Library (Wayne Truchsess, Rev 5.0)
│           ├── I2C.h
│           ├── I2C.cpp
│           └── keywords.txt
├── hardware/
│   ├── bom.md                  # Bill of materials
│   └── pinout.md               # ATmega328P wiring reference
├── docs/
│   ├── wiring.md               # Full wiring guide with pull-up notes
│   ├── motec_setup.md          # MoTeC C125 / M1 channel definitions and i2 setup
│   └── calibration.md          # Emissivity and thermal offset calibration
├── tools/
│   └── can_verify.py           # Python script to verify CAN frames on bench
├── .gitignore
└── LICENSE
```

---

## Steps

### 1 — Install the I²C Library

The first step in communication with the MLX90614 sensor is to initialise I²C — setting up start/stop conditions and configuring the TWCR, TWSR, and TWBR registers for the correct bit rate. A dedicated library handles all of this and can be downloaded from GitHub.

Copy `firmware/lib/I2C/` into your Arduino libraries folder:

| OS      | Path |
|---------|------|
| Windows | `Documents\Arduino\libraries\I2C\` |
| macOS   | `~/Documents/Arduino/libraries/I2C/` |
| Linux   | `~/Arduino/libraries/I2C/` |

Restart the Arduino IDE after copying.

> Source: [I²C Master Library — Wayne Truchsess](https://github.com/DSSCircuits/I2C-Master-Library)

---

### 2 — Connect the Sensors

After installing the library, place the sensors on a breadboard and connect them to power. Locate the SDA (data) and SCL (clock) pins on the Arduino — they are clearly labelled on most boards.

**Pull-up resistors of 4.7kΩ are required on both SDA and SCL, connected to the 3.3V rail** (not 5V — the MLX90614 I/O pins are 3.3V and will be damaged by 5V pull-ups).

Pay attention to the MLX90614 sensor pinout — the tab on the TO-39 package faces up in the diagram below:

![MLX90614 pinout](https://user-images.githubusercontent.com/50503074/109747960-4604df00-7b95-11eb-944d-aaedaedec1a3.png)

Arduino UNO board pinout (other boards may vary):

![Arduino UNO pinout](https://user-images.githubusercontent.com/50503074/109747971-4bfac000-7b95-11eb-88d3-668b2723917e.png)

<sup>Patarroyo, Jaime. (2012, July 16). [digital image]. [Retrieved from wiring.co](http://wiki.wiring.co/wiki/Connecting_Infrared_Thermometer_MLX90614_to_Wiring#Download_and_Install_I.C2.B2Cmaster_library)</sup>

![image](https://user-images.githubusercontent.com/50503074/109747823-11912300-7b95-11eb-90e9-e010f6335865.png)<br>

See [`hardware/pinout.md`](hardware/pinout.md) for the complete ATmega328P wiring table and [`docs/wiring.md`](docs/wiring.md) for connector and harness details.

---

### 3 — Set Sensor Addresses

Every MLX90614 ships with the default I²C address `0x5A`. Because all four sensors share the same bus, three of them must be reprogrammed to unique addresses before being connected together. You may use any address in the range `0x01`–`0x7F` listed in the MLX90614 datasheet. The addresses below are the ones this firmware expects.

| Corner | Address | Action |
|--------|---------|--------|
| FL     | `0x5A`  | Factory default — no change needed |
| FR     | `0x5B`  | Reprogram using steps below |
| RL     | `0x5C`  | Reprogram using steps below |
| RR     | `0x5D`  | Reprogram using steps below |

**For each sensor that needs a new address — connect one at a time:**

1. Connect the single sensor to the Arduino (disconnect all others)
2. Open [`firmware/change_address/change_address_v2.ino`](firmware/change_address/change_address_v2.ino)
3. Set `#define NEW_ADDR` to the target address (e.g. `0x5B`)
4. Upload the sketch, then open Serial Monitor at **115200 baud** (so you can see debug messages/prompts in the Serial Monitor on your computer)
5. When prompted, power-cycle the sensor by removing and reconnecting its VDD wire only (leave the Arduino powered)
6. The sketch reads back the stored address to confirm — check this before moving to the next sensor

> ⚠️ Never connect two sensors with the same address to the same I²C bus — readings will be corrupted and both sensors may lock up the bus.

---

### 4 — Wire All Four Sensors

With each sensor programmed to a unique address, connect all four to the shared SDA/SCL bus. The 4.7kΩ pull-up resistors only need to appear once on the bus (not one pair per sensor).

Quick reference:

```
ATmega328P          MLX90614 ×4 (shared bus)
──────────          ────────────────────────
PC4 (SDA) ──┬────── SDA    ← 4.7kΩ pull-up to 3.3V
PC5 (SCL) ──┴────── SCL    ← 4.7kΩ pull-up to 3.3V
PC0–PC3   ────────  74HC138 A0–A3  (sensor mux select)

ATmega328P          MCP2515 (CAN controller)
──────────          ───────────────────────
PB2 (SS̄)  ────────  CS̄
PB3 (MOSI)────────  SI
PB4 (MISO)────────  SO
PB5 (SCK) ────────  SCK
```

---

## Up and Running

### Upload the Address Change Utility

Compile and upload [`firmware/change_address/change_address_v2.ino`](firmware/change_address/change_address_v2.ino) to reprogram each sensor one at a time as described in Step 3. It is important to power cycle the sensor by removing and then reconnecting its VDD wire after each write — this stores the new address in EEPROM.

> The v2 utility fixes two bugs in the original: the CRC-8 PEC byte is now computed correctly (polynomial `0x07`) rather than brute-forced, and the EEPROM erase sequence matches the MLX90614 datasheet.

### Upload the Main Driver

After all four sensors have unique addresses, upload [`firmware/tire_temp_4wheel/tire_temp_4wheel.ino`](firmware/tire_temp_4wheel/tire_temp_4wheel.ino) to the Arduino with each sensor connected. Open Serial Monitor at **115200 baud** to receive each sensor's field-of-view temperature. Each line shows outer / mid / inner zone temperatures and any error flags:

```
FL | O: 84.3 M: 87.1 I: 82.6 Ta: 24.4 C
FR | O: 85.0 M: 88.4 I: 83.9 Ta: 24.3 C
RL | O: 76.2 M: 80.1 I: 78.8 Ta: 24.5 C
RR | O: 77.4 M: 81.3 I: 79.2 Ta: 24.4 C
```

CAN frames are transmitted simultaneously on IDs `0x600`–`0x603` at 10 Hz. Use [`tools/can_verify.py`](tools/can_verify.py) to verify the frames on the bench before installing on the car. See [`docs/motec_setup.md`](docs/motec_setup.md) for MoTeC C125 / M1 channel definitions, math channels, and i2 Pro display setup.

---

## CAN Frame Reference

All frames transmit at **10 Hz**, **500 kbps**, **DLC = 8**, standard 11-bit IDs.  
Encoding: `INT16 little-endian`, factor `0.02`, offset `0` → °C.

| CAN ID  | Corner | B0–B1    | B2–B3   | B4–B5    | B6     | B7     |
|---------|--------|----------|---------|----------|--------|--------|
| `0x600` | FL     | Outer °C | Mid °C  | Inner °C | Amb °C | Status |
| `0x601` | FR     | Outer °C | Mid °C  | Inner °C | Amb °C | Status |
| `0x602` | RL     | Outer °C | Mid °C  | Inner °C | Amb °C | Status |
| `0x603` | RR     | Outer °C | Mid °C  | Inner °C | Amb °C | Status |

**Status byte (B7) bit flags:**

| Bit | Mask   | Meaning                        |
|-----|--------|--------------------------------|
| 7   | `0x80` | Over-temperature (mid > 105°C) |
| 6   | `0x40` | Sensor I²C error               |
| 5   | `0x20` | CRC-8 PEC failure              |
| 1:0 | `0x03` | Wheel index (0=FL … 3=RR)     |

---

## Thermal Operating Windows

| State          | Range       | Action                       |
|----------------|-------------|------------------------------|
| Cold           | < 65°C      | Push harder, increase load   |
| Graining risk  | 65–79°C     | Below optimal, monitor       |
| **Optimal**    | **80–95°C** | Target window (Hoosier R25B) |
| Overheating    | 96–109°C    | Reduce load                  |
| Critical       | > 110°C     | Pit immediately              |

---

## Hardware

| Component       | Part Number      | Qty |
|-----------------|------------------|-----|
| IR Sensor       | MLX90614ESF-DAA  | 4   |
| Microcontroller | ATmega328P-AU    | 1   |
| CAN Controller  | MCP2515          | 1   |
| CAN Transceiver | TJA1050          | 1   |
| Address Decoder | 74HC138          | 1   |
| 5V Regulator    | LM7805           | 1   |
| 3.3V Regulator  | MCP1700-3302E    | 1   |

Full BOM with values, footprints, and suppliers: [`hardware/bom.md`](hardware/bom.md)

---

## Dependencies

- [I²C Master Library](https://github.com/DSSCircuits/I2C-Master-Library) — Wayne Truchsess, Rev 5.0 (included in `firmware/lib/I2C/`)
- Arduino IDE 1.8+ or Arduino CLI
- avr-gcc toolchain (bundled with Arduino IDE or other)
- [python-can](https://python-can.readthedocs.io/) — for `tools/can_verify.py` bench testing

---

## License

MIT License — see [`LICENSE`](LICENSE) for details.  
I²C Master Library is licensed under LGPL 2.1 (see `firmware/lib/I2C/`).
