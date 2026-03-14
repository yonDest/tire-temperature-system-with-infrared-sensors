# Bill of Materials

| Ref  | Description               | Part Number         | Qty | Value / Rating        | Supplier       |
|------|---------------------------|---------------------|-----|-----------------------|----------------|
| U1   | 8-bit AVR MCU             | ATmega328P-AU       | 1   | TQFP-32               | Digi-Key       |
| U2   | SPI CAN Controller        | MCP2515-I/SO        | 1   | SOIC-18               | Digi-Key       |
| U3   | CAN Bus Transceiver       | TJA1050T            | 1   | SOIC-8                | Digi-Key       |
| U4   | Address Decoder           | 74HC138D            | 1   | SOIC-16               | Digi-Key       |
| U5   | 5V Linear Regulator       | LM7805CT            | 1   | TO-220, 1A            | Digi-Key       |
| U6   | 3.3V LDO Regulator        | MCP1700T-3302E/TT   | 1   | SOT-23, 250mA         | Digi-Key       |
| IR1–4| IR Thermometer            | MLX90614ESF-DAA     | 4   | TO-39, 3V, SMBus      | Digi-Key       |
| Y1   | Crystal                   | ABM8-16.000MHZ-B2   | 1   | 16 MHz, SMD           | Digi-Key       |
| C1,2 | Crystal Load Cap          | —                   | 2   | 22pF, 0402 MLCC       | Digi-Key       |
| C3,4 | MCU VCC Bypass            | —                   | 2   | 100nF, 0402 MLCC      | Digi-Key       |
| C5,6 | MCU VCC Bulk              | —                   | 2   | 10µF, 0805 MLCC       | Digi-Key       |
| C7   | AVCC Filter Cap           | —                   | 1   | 100nF, 0402 MLCC      | Digi-Key       |
| C8   | RESET Debounce            | —                   | 1   | 100nF, 0402 MLCC      | Digi-Key       |
| C9   | 3.3V Rail Bypass          | —                   | 1   | 100nF, 0402 MLCC      | Digi-Key       |
| C10  | 3.3V Rail Bulk            | —                   | 1   | 10µF, 0805 MLCC       | Digi-Key       |
| C11  | 5V Rail Bypass            | —                   | 1   | 100nF, 0402 MLCC      | Digi-Key       |
| C12  | 5V Rail Bulk (LM7805 out) | —                   | 1   | 100µF electrolytic    | Digi-Key       |
| C13  | 5V Rail Bulk (LM7805 in)  | —                   | 1   | 100µF electrolytic    | Digi-Key       |
| L1   | AVCC Filter Inductor      | —                   | 1   | 10µH, 0805            | Digi-Key       |
| R1   | RESET Pull-up             | —                   | 1   | 10kΩ, 0402            | Digi-Key       |
| R2   | SDA Pull-up               | —                   | 1   | 4.7kΩ, 0402           | Digi-Key       |
| R3   | SCL Pull-up               | —                   | 1   | 4.7kΩ, 0402           | Digi-Key       |
| R4   | CAN Bus Termination       | —                   | 1   | 120Ω, 0402            | Digi-Key       |
| D1   | TVS Diode (12V input)     | P6SMB15A            | 1   | 15V, SMB              | Digi-Key       |
| Q1   | Polarity Protection FET   | AO3401              | 1   | P-channel, SOT-23     | Digi-Key       |
| J1   | ISP Header                | —                   | 1   | 2×3, 2.54mm           | Digi-Key       |
| J2   | CAN Bus Connector         | Deutsch DT04-2P     | 1   | 2-pin                 | Digi-Key       |
| J3–6 | Sensor Connectors         | Deutsch DT04-4P     | 4   | 4-pin per sensor      | Digi-Key       |
| J7   | Power Input Connector     | Deutsch DT04-2P     | 1   | 12V input             | Digi-Key       |

## Notes

- All MLCC capacitors: X7R dielectric, ≥16V rating (50V preferred for supply pins)
- Place 100nF bypass caps within 1mm of every VCC/AVCC pin
- R2/R3 pull to **3.3V** (not 5V) — MLX90614 VDD is 3.3V
- CAN termination R4 placed at the sensor node end of the bus stub
- D1/Q1 on the 12V input protect against reverse polarity and automotive transients
