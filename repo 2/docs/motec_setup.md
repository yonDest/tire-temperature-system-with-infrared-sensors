# MoTeC Integration Guide

## CAN Bus Settings

| Parameter    | Value              |
|--------------|--------------------|
| Baud rate    | 500 kbps           |
| Frame type   | CAN 2.0B, 11-bit ID |
| Termination  | 120Ω at each end   |
| MoTeC bus    | CAN Bus 1          |
| Cycle time   | 100ms (10 Hz)      |
| DLC          | 8 bytes            |
| Byte order   | Little-endian (Intel) |

## CAN Frame Map

Frames are transmitted at 10 Hz. Temperature values are INT16 little-endian
with factor 0.02, offset 0 → degrees Celsius.

```
Byte:   0    1    2    3    4    5    6    7
       [OUT LSB][OUT MSB][MID LSB][MID MSB][INN LSB][INN MSB][ AMB ][ FLAGS]
```

| CAN ID  | Corner |
|---------|--------|
| `0x600` | FL     |
| `0x601` | FR     |
| `0x602` | RL     |
| `0x603` | RR     |

Ambient temperature (Byte 6): UINT8, offset −40 → range −40 to +85°C.

Status flags (Byte 7):

| Bit | Mask   | Meaning                        |
|-----|--------|--------------------------------|
| 7   | `0x80` | Over-temperature (mid > 105°C) |
| 6   | `0x40` | Sensor I²C error               |
| 5   | `0x20` | CRC-8 PEC failure              |
| 1:0 | `0x03` | Wheel index (0=FL … 3=RR)     |

## Channel Definitions

Enter these in the CAN Template (M1 Tune or C125 Manager):

| Channel Name       | CAN ID  | Start Byte | Type     | Factor | Offset | Unit |
|--------------------|---------|------------|----------|--------|--------|------|
| TyreTempFLOuter    | `0x600` | 0          | INT16 LE | 0.02   | 0      | °C   |
| TyreTempFLMid      | `0x600` | 2          | INT16 LE | 0.02   | 0      | °C   |
| TyreTempFLInner    | `0x600` | 4          | INT16 LE | 0.02   | 0      | °C   |
| TyreTempFROuter    | `0x601` | 0          | INT16 LE | 0.02   | 0      | °C   |
| TyreTempFRMid      | `0x601` | 2          | INT16 LE | 0.02   | 0      | °C   |
| TyreTempFRInner    | `0x601` | 4          | INT16 LE | 0.02   | 0      | °C   |
| TyreTempRLOuter    | `0x602` | 0          | INT16 LE | 0.02   | 0      | °C   |
| TyreTempRLMid      | `0x602` | 2          | INT16 LE | 0.02   | 0      | °C   |
| TyreTempRLInner    | `0x602` | 4          | INT16 LE | 0.02   | 0      | °C   |
| TyreTempRROuter    | `0x603` | 0          | INT16 LE | 0.02   | 0      | °C   |
| TyreTempRRMid      | `0x603` | 2          | INT16 LE | 0.02   | 0      | °C   |
| TyreTempRRInner    | `0x603` | 4          | INT16 LE | 0.02   | 0      | °C   |
| TyreTempFLAmb      | `0x600` | 6          | UINT8    | 1.0    | −40    | °C   |
| TyreSensorStatus   | `0x600` | 7          | UINT8    | 1      | 0      | —    |

## M1 Tune / C125 Setup Steps

1. **M1 Tune → CAN Bus → CAN Bus 1 → Receive Frames**  
   Add a new frame: ID = `0x600`, Period = 100ms, DLC = 8.

2. **Map signals** — under the 0x600 frame add 3 signals:  
   Bytes 0–1 → `TyreTempFLOuter`, Bytes 2–3 → `TyreTempFLMid`, Bytes 4–5 → `TyreTempFLInner`.  
   Type = Signed 16, Factor = 0.02, Offset = 0, Unit = °C.

3. **Repeat** for frames `0x601`, `0x602`, `0x603` (FR / RL / RR).

4. **Log rate** — Devices → Logger → Channels → all `TyreTemp*` at **10 Hz**, Log on = Always.

5. **Verify** — i2 Pro → Live → Channel List. Filter `TyreTemp`. All 12 channels should show live values.

## Recommended Math Channels (i2 Pro)

```
TyreTempFLAvg       = (TyreTempFLOuter + TyreTempFLMid + TyreTempFLInner) / 3
TyreTempFRAvg       = (TyreTempFROuter + TyreTempFRMid + TyreTempFRInner) / 3
TyreTempRLAvg       = (TyreTempRLOuter + TyreTempRLMid + TyreTempRLInner) / 3
TyreTempRRAvg       = (TyreTempRROuter + TyreTempRRMid + TyreTempRRInner) / 3

TyreTempFLGradient  = TyreTempFLOuter - TyreTempFLInner   // camber indicator
TyreTempFRGradient  = TyreTempFROuter - TyreTempFRInner

TyreTempFrontAvg    = (TyreTempFLAvg + TyreTempFRAvg) / 2
TyreTempRearAvg     = (TyreTempRLAvg + TyreTempRRAvg) / 2
TyreTempDeltaFR     = TyreTempFrontAvg - TyreTempRearAvg  // balance indicator

TyreTempMax         = Max(TyreTempFLMid, TyreTempFRMid, TyreTempRLMid, TyreTempRRMid)
TyreTempSymmetry    = Abs(TyreTempFLAvg - TyreTempFRAvg)  // left-right delta
```

## Alarm Thresholds

Set these in i2 Pro → Channel Properties → Alarms:

| Condition         | Range       | Colour  |
|-------------------|-------------|---------|
| Cold              | < 65°C      | Blue    |
| Graining risk     | 65–79°C     | Orange  |
| Optimal           | 80–95°C     | Green   |
| Overheating       | 96–109°C    | Orange  |
| Critical          | > 110°C     | Red     |
