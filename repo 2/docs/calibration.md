# Calibration Guide

## Emissivity

The MLX90614 ships with emissivity set to 1.0 (blackbody). Racing rubber has
an emissivity of approximately 0.92–0.95. Writing the correct value improves
accuracy by 2–5°C in the 80–100°C range.

### Setting Emissivity

Emissivity is stored in MLX EEPROM register `0x24`.

| Emissivity | EEPROM Value | Use For                    |
|------------|--------------|----------------------------|
| 1.00       | `0xFFFF`     | Factory default (blackbody)|
| 0.95       | `0xF333`     | General rubber             |
| 0.92       | `0xEB85`     | Slick racing tyre (recommended) |
| 0.40       | `0x6666`     | Shiny metal (reference)    |

Formula: `EEPROM_value = round(ε × 65535)`

Write with the following sequence (from `change_address_v2.ino` pattern):

```cpp
// Erase first
write_eeprom_word(SENSOR_ADDR, 0x24, 0x00, 0x00);
delay(10);
// Write emissivity for ε = 0.92
write_eeprom_word(SENSOR_ADDR, 0x24, 0x85, 0xEB);
delay(10);
// Power-cycle the sensor to apply
```

> Always power-cycle the sensor after any EEPROM write.

### Verifying Emissivity

Read back register `0x24` and confirm the stored value matches what you wrote.
The `change_address_v2.ino` utility can be adapted for this read-back.

---

## Ambient Temperature Offset

At rest (car stationary, sensor cool), the MLX ambient temperature (Ta,
register `0x06`) should match a reference thermometer placed next to the
sensor. If it differs by more than 1°C, check:

1. Sensor is not being heated by brake rotor radiation at rest
2. VDD is stable at 3.3V ± 0.1V
3. Decoupling capacitors are in place on VDD

The firmware transmits Ta in Byte 6 of each CAN frame. Log
`TyreTempFLAmb` in MoTeC i2 and compare against ambient air temperature.

---

## Sensor Mounting

| Parameter        | Value              | Notes                                |
|------------------|--------------------|--------------------------------------|
| Standoff distance| 60–80 mm           | From sensor face to tyre surface     |
| FOV half-angle   | 35°                | MLX90614ESF-DAA                      |
| Spot diameter    | ~75 mm at 70mm dist| Covers roughly one third of tread width |
| Bracket material | Aluminium          | Low thermal mass, good conductivity  |

For 3-zone measurement without a servo, mount three sensors per corner at
different lateral angles (outer / centre / inner). For a single sensor with a
servo, use a NEMA 17 stepper or a micro servo on PB1 (OC1A) and sweep
between positions before each sample.

---

## Pre-Event Verification Checklist

- [ ] All 4 sensors respond on I²C scan (run `I2c.scan()` from setup)
- [ ] All sensors reading within ±2°C of a calibrated IR thermometer at ambient
- [ ] Emissivity confirmed at `0xEB85` via EEPROM readback on each sensor
- [ ] CAN frames visible in `tools/can_verify.py` at 500 kbps
- [ ] MoTeC i2 live view shows all 12 channels at 10 Hz
- [ ] Status byte reads `0x00`–`0x03` (no error flags) on all corners
- [ ] Sensors stable through 5-minute heat soak at 80°C (use heat gun)
