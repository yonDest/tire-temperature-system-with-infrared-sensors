# ATmega328P Wiring Reference

## I²C Bus — Sensor Side

| ATmega328P Pin | AVR Name | Signal | Connects To             | Note                         |
|----------------|----------|--------|-------------------------|------------------------------|
| 27             | PC4      | SDA    | MLX90614 SDA (all 4)    | 4.7kΩ pull-up to **3.3V**   |
| 28             | PC5      | SCL    | MLX90614 SCL (all 4)    | 4.7kΩ pull-up to **3.3V**   |

> The MLX90614 runs at 3.3V. I²C is compatible at 5V logic because SDA/SCL
> are open-drain — they are pulled to 3.3V, never driven high by the MCU.
> Do **not** pull up to 5V or you will damage the sensors.

## Address / Mux Select

| ATmega328P Pin | AVR Name | Signal     | Connects To       |
|----------------|----------|------------|-------------------|
| 23             | PC0      | ADDR_SEL0  | 74HC138 A0        |
| 24             | PC1      | ADDR_SEL1  | 74HC138 A1        |
| 25             | PC2      | ADDR_SEL2  | 74HC138 A2        |
| 26             | PC3      | ADDR_SEL3  | 74HC138 A3        |

The 74HC138 3-to-8 decoder drives each sensor's `VDD` enable line so only
one sensor is powered at a time, forcing unique addressing on the shared I²C
bus even before address reprogramming.

## SPI Bus — MCP2515 CAN Controller

| ATmega328P Pin | AVR Name   | Signal | Connects To     |
|----------------|------------|--------|-----------------|
| 16             | PB2        | SS̄     | MCP2515 CS̄     |
| 17             | PB3        | MOSI   | MCP2515 SI      |
| 18             | PB4        | MISO   | MCP2515 SO      |
| 19             | PB5        | SCK    | MCP2515 SCK     |

The MCP2515 INT pin can optionally connect to PD2 (INT0) for interrupt-driven
TX confirmation — not used in current firmware.

## Power

| ATmega328P Pin | Signal | Source              | Bypass Caps          |
|----------------|--------|---------------------|----------------------|
| 7              | VCC    | LM7805 +5V output   | 100nF + 10µF         |
| 20             | AVCC   | LM7805 +5V (LC filtered) | 10µH + 100nF    |
| 8, 22          | GND    | Chassis star point  | —                    |

MLX90614 VDD: MCP1700 3.3V LDO. Each sensor draws ~1.5mA typical.

## Crystal

| ATmega328P Pin | Signal | Component           |
|----------------|--------|---------------------|
| 9              | XTAL1  | 16 MHz crystal      |
| 10             | XTAL2  | 16 MHz crystal      |

Load capacitors: 22pF to GND on each pin.

## Reset

| ATmega328P Pin | Signal | Connection              |
|----------------|--------|-------------------------|
| 1              | RESET̄  | 10kΩ to VCC, 100nF to GND |

## ISP Header (6-pin, 2×3, 2.54mm)

| Pin | Signal | ATmega328P |
|-----|--------|------------|
| 1   | MISO   | PB4        |
| 2   | VCC    | +5V        |
| 3   | SCK    | PB5        |
| 4   | MOSI   | PB3        |
| 5   | RESET̄  | Pin 1      |
| 6   | GND    | GND        |

## Fuse Settings

| Fuse  | Value  | Meaning                          |
|-------|--------|----------------------------------|
| Low   | `0xFF` | External crystal, full swing     |
| High  | `0xD9` | SPI enabled, JTAG disabled       |
| Ext   | `0xFF` | Brown-out at 4.3V                |

Flash command:
```bash
avrdude -p m328p -c usbasp -U lfuse:w:0xFF:m -U hfuse:w:0xD9:m -U efuse:w:0xFF:m
avrdude -p m328p -c usbasp -U flash:w:tire_temp_4wheel.hex:i
```
