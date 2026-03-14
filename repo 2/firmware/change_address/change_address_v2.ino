/*
 * change_address_v2.ino
 * MLX90614 I2C Address Change Utility — Fixed CRC-8 Implementation
 *
 * Usage:
 *  1. Connect ONE sensor at a time (default address 0x5A, or universal 0x00)
 *  2. Set NEW_ADDR below to your target address (0x5A–0x7F)
 *  3. Upload, open Serial Monitor at 115200 baud
 *  4. When prompted, power-cycle the sensor's VDD (not the Arduino)
 *  5. Sensor will answer on the new address from then on
 *
 * FSAE corner assignment:
 *   FL = 0x5A  (factory default — no change needed)
 *   FR = 0x5B
 *   RL = 0x5C
 *   RR = 0x5D
 */

#include "I2C.h"

// ── Set the target address here ───────────────────────────────────────────
#define NEW_ADDR  0x5B           // change this for each sensor

// ── MLX EEPROM register for slave address ─────────────────────────────────
#define MLX_EEPROM_ADDR  0x2E

// ── CRC-8 SMBus (polynomial 0x07) ────────────────────────────────────────
// Used to compute the PEC byte for EEPROM write transactions.
// The MLX validates this — if wrong, it silently ignores the write.
static uint8_t crc8_smbus(uint8_t crc, uint8_t data) {
  for (uint8_t i = 0; i < 8; i++) {
    if ((crc ^ data) & 0x80) crc = (crc << 1) ^ 0x07;
    else                      crc <<= 1;
    data <<= 1;
  }
  return crc;
}

// Build the PEC for an EEPROM write transaction:
//   [SLA+W] [reg] [lo] [hi]
static uint8_t compute_pec_write(byte sla, byte reg, byte lo, byte hi) {
  uint8_t crc = 0;
  crc = crc8_smbus(crc, sla << 1);   // SLA+W
  crc = crc8_smbus(crc, reg);
  crc = crc8_smbus(crc, lo);
  crc = crc8_smbus(crc, hi);
  return crc;
}

// ── Read current address from EEPROM 0x2E ─────────────────────────────────
byte read_eeprom_addr(byte sla) {
  uint8_t status = I2c.read(sla, MLX_EEPROM_ADDR, 3);
  if (status != 0) {
    Serial.print(F("  I2C read failed, status="));
    Serial.println(status);
    return 0xFF;
  }
  byte lo  = I2c.receive();
  byte hi  = I2c.receive();
  // byte pec = I2c.receive();  // could verify here too
  I2c.receive();
  Serial.print(F("  EEPROM 0x2E → lo=0x")); Serial.print(lo, HEX);
  Serial.print(F("  hi=0x")); Serial.println(hi, HEX);
  return lo;                    // bits 6:0 are the 7-bit slave address
}

// ── Write one word to MLX EEPROM with correct PEC ─────────────────────────
// Two-phase: erase (write 0x00/0x00), then write new value.
// Delay of ≥ 5ms required between erase and write (EEPROM NVM cycle).
bool write_eeprom_word(byte sla, byte reg, byte lo, byte hi) {
  // Phase 1: erase — write 0x0000 with correct PEC
  byte pec_erase = compute_pec_write(sla, reg, 0x00, 0x00);

  byte buf_erase[3] = { 0x00, 0x00, pec_erase };
  uint8_t status = I2c.write(sla, reg, buf_erase, 3);
  if (status != 0) {
    Serial.print(F("  Erase failed, status="));
    Serial.println(status);
    return false;
  }
  delay(10);                    // NVM write cycle

  // Phase 2: write new address with correct PEC
  byte pec_write = compute_pec_write(sla, reg, lo, hi);
  byte buf_write[3] = { lo, hi, pec_write };
  status = I2c.write(sla, reg, buf_write, 3);
  if (status != 0) {
    Serial.print(F("  Write failed, status="));
    Serial.println(status);
    return false;
  }
  delay(10);                    // NVM write cycle
  return true;
}

// ─────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n=== MLX90614 Address Change Utility v2 ==="));

  I2c.begin();
  I2c.timeOut(100);
  delay(1000);

  // Safety check
  if (NEW_ADDR == 0x00) {
    Serial.println(F("ERROR: 0x00 is the broadcast address — refused."));
    return;
  }
  if (NEW_ADDR > 0x7F) {
    Serial.println(F("ERROR: Address must be 0x01–0x7F."));
    return;
  }

  // ── Step 1: Scan bus to find sensor ──────────────────────────────────
  Serial.println(F("\n[1] Scanning I2C bus..."));
  I2c.scan();                   // prints all responding addresses

  // ── Step 2: Read current address using universal address 0x00 ────────
  Serial.println(F("\n[2] Reading current address via universal address 0x00..."));
  byte current = read_eeprom_addr(0x00);
  if (current == 0xFF) {
    Serial.println(F("  Could not reach sensor — check wiring."));
    return;
  }
  Serial.print(F("  Current address: 0x")); Serial.println(current, HEX);

  if (current == NEW_ADDR) {
    Serial.print(F("  Sensor already at 0x")); Serial.print(NEW_ADDR, HEX);
    Serial.println(F(" — nothing to do."));
    return;
  }

  // ── Step 3: Write new address ─────────────────────────────────────────
  Serial.print(F("\n[3] Writing new address 0x")); Serial.println(NEW_ADDR, HEX);
  // MLX EEPROM 0x2E layout: bits 6:0 = slave address, hi byte = 0x00
  bool ok = write_eeprom_word(0x00, MLX_EEPROM_ADDR, NEW_ADDR, 0x00);
  if (!ok) {
    Serial.println(F("  EEPROM write failed — check pull-up resistors."));
    return;
  }
  Serial.println(F("  Write command sent."));

  // ── Step 4: Power-cycle prompt ────────────────────────────────────────
  Serial.println(F("\n[4] >>> POWER-CYCLE THE SENSOR VDD NOW (you have 10 seconds) <<<"));
  Serial.println(F("    (Remove and reconnect the sensor's VDD wire only)"));
  delay(10000);

  // ── Step 5: Verify on new address ────────────────────────────────────
  Serial.print(F("\n[5] Verifying address 0x")); Serial.println(NEW_ADDR, HEX);
  byte confirmed = read_eeprom_addr(NEW_ADDR);
  if (confirmed == NEW_ADDR) {
    Serial.println(F("  SUCCESS — new address confirmed!"));
  } else {
    Serial.println(F("  WARNING — address mismatch. Try power-cycling again or check wiring."));
  }

  Serial.println(F("\n=== Done. Sensor ready. ==="));
}

void loop() {
  // Print temperature on new address every 2 s for live confirmation
  delay(2000);

  uint8_t status = I2c.read(NEW_ADDR, 0x07, 3);
  if (status != 0) {
    Serial.println(F("Read error — sensor may not be on bus at new address yet"));
    return;
  }
  byte lo  = I2c.receive();
  byte hi  = I2c.receive();
  I2c.receive();                             // discard PEC here

  if (hi & 0x80) {
    Serial.println(F("Sensor error bit set"));
    return;
  }
  float temp = ((uint16_t)((hi & 0x7F) << 8 | lo)) * 0.02f - 273.15f;
  Serial.print(F("Temp @ 0x")); Serial.print(NEW_ADDR, HEX);
  Serial.print(F(": ")); Serial.print(temp, 1); Serial.println(F(" C"));
}
