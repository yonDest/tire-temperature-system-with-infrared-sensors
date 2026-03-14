# Wiring Guide

## Bus Architecture

```
                        3.3V
                         │
                       4.7kΩ  4.7kΩ
                         │      │
ATmega328P              SDA    SCL
  PC4 ──────────────────┴──────┘────── MLX90614 FL  (0x5A)
  PC5 ──────────────────────────────── MLX90614 FR  (0x5B)
                                  └─── MLX90614 RL  (0x5C)
                                  └─── MLX90614 RR  (0x5D)
```

All four sensors share the same SDA/SCL bus. They are distinguished by their
unique 7-bit I²C addresses, which must be programmed before deployment using
`change_address_v2.ino`.

## I²C Pull-up Resistors

| Parameter    | Value    | Notes                                       |
|--------------|----------|---------------------------------------------|
| SDA pull-up  | 4.7kΩ    | To **3.3V** — not 5V                        |
| SCL pull-up  | 4.7kΩ    | To **3.3V** — not 5V                        |
| Bus voltage  | 3.3V     | MLX90614 VDD max = 3.6V                     |
| MCU logic    | 5V       | Safe: SDA/SCL are open-drain, pulled to 3.3V |

> Using 5V pull-ups will overvoltage the MLX90614 I/O pins and damage the
> sensor. Always pull to the sensor's VDD rail (3.3V).

On long cable runs (>100mm) reduce to 3.3kΩ. For very short PCB traces,
4.7kΩ is ideal for 100 kHz operation.

## Sensor Connector Wiring (Deutsch DT04-4P)

Each sensor uses a 4-pin Deutsch connector at the wheel end:

| Pin | Signal | Wire Color (recommendation) |
|-----|--------|-----------------------------|
| 1   | VDD    | Red                          |
| 2   | GND    | Black                        |
| 3   | SDA    | White                        |
| 4   | SCL    | Green                        |

Run SDA and SCL as a twisted pair inside the loom. Shield the cable and
ground the shield at the PCB end only (single-point ground).

## CAN Bus Wiring

| Signal  | Wire Color (SAE J1939) | Connects To         |
|---------|------------------------|---------------------|
| CAN-H   | Yellow                 | MoTeC CAN1-H        |
| CAN-L   | Green                  | MoTeC CAN1-L        |
| GND     | Black                  | Common chassis star |

- Twisted pair, 120Ω characteristic impedance
- 120Ω termination at **each end** of the bus
- Maximum stub length from main bus to device: 300mm

## Power Input

| Signal | Voltage | Source           | Protection       |
|--------|---------|------------------|------------------|
| VIN    | 12V nom | Car power rail   | P6SMB15A TVS + P-FET polarity |
| +5V    | 5.0V    | LM7805 output    | 100µF in + out   |
| +3.3V  | 3.3V    | MCP1700 LDO      | 100nF + 10µF     |

The 12V rail in an FSAE car can have transients up to 40V during alternator
load dump. The TVS clamps at 15V. The P-channel FET protects against reversed
battery connections.

## Grounding

- Run a single star-point ground near the PCB power connector
- AGND and DGND planes joined at one point — do not connect at multiple locations
- Chassis ground connections: only at the star point, nowhere else on the PCB
- Sensor GND pins: connect to DGND (not chassis directly)
