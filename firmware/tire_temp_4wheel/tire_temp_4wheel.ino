/*
 * tire_temp_4wheel.ino
 * FSAE Tire Temperature System — ATmega328P + 4× MLX90614
 *
 * Wiring:
 *   PC4 (SDA) ─── 4.7kΩ ─── 3.3V
 *   PC5 (SCL) ─── 4.7kΩ ─── 3.3V
 *   PB2  → MCP2515 CS
 *   PB3  → MCP2515 MOSI
 *   PB4  → MCP2515 MISO
 *   PB5  → MCP2515 SCK
 *   PC0-3 → 74HC138 address select (one sensor active per read)
 *
 * Sensor I2C addresses (set with change_address_v2.ino):
 *   FL = 0x5A,  FR = 0x5B,  RL = 0x5C,  RR = 0x5D
 */

#include "I2C.h"
#include <SPI.h>

// ── Sensor addresses (7-bit) ───────────────────────────────────────────────
#define ADDR_FL   0x5A
#define ADDR_FR   0x5B
#define ADDR_RL   0x5C
#define ADDR_RR   0x5D

// ── MLX90614 RAM registers ─────────────────────────────────────────────────
#define MLX_REG_TOBJ1   0x07   // object temperature channel 1
#define MLX_REG_TA      0x06   // ambient temperature

// ── MCP2515 SPI CAN controller ────────────────────────────────────────────
#define CAN_CS_PIN      10     // PB2 on ATmega328P
#define CAN_BASE_ID     0x600  // 0x600=FL, 0x601=FR, 0x602=RL, 0x603=RR

// MCP2515 SPI instruction bytes
#define MCP_RESET       0xC0
#define MCP_WRITE       0x02
#define MCP_READ        0x03
#define MCP_RTS_TXB0    0x81   // request-to-send TXB0
#define MCP_LOAD_TXB0   0x40   // load TX buffer 0 starting at SIDH

// MCP2515 register addresses
#define MCP_CNF1        0x2A
#define MCP_CNF2        0x29
#define MCP_CNF3        0x28
#define MCP_CANCTRL     0x0F
#define MCP_CANSTAT     0x0E
#define MCP_TXB0CTRL    0x30

// ── Thermal alert thresholds (°C) ────────────────────────────────────────
#define TEMP_OPTIMAL_LOW   80.0f
#define TEMP_OPTIMAL_HIGH  95.0f
#define TEMP_WARN_HIGH    105.0f
#define TEMP_CRIT_HIGH    115.0f

// ── Status flag bits (Byte 7 in CAN frame) ───────────────────────────────
#define FLAG_OVERTEMP   0x80
#define FLAG_SENSOR_ERR 0x40
#define FLAG_CRC_FAIL   0x20

// ── Data structure for one corner ────────────────────────────────────────
struct TireCorner {
  byte    addr;          // 7-bit I2C address
  float   outer;         // outer zone °C
  float   mid;           // mid   zone °C
  float   inner;         // inner zone °C
  float   ambient;       // Ta °C
  byte    status;        // flag byte for CAN
  bool    valid;         // false if last read failed
};

TireCorner corners[4] = {
  { ADDR_FL, 0, 0, 0, 0, 0, false },
  { ADDR_FR, 0, 0, 0, 0, 0, false },
  { ADDR_RL, 0, 0, 0, 0, 0, false },
  { ADDR_RR, 0, 0, 0, 0, 0, false },
};
const char* cornerNames[4] = { "FL", "FR", "RL", "RR" };

// ── CRC-8 SMBus (polynomial 0x07) ────────────────────────────────────────
// The MLX90614 appends a PEC byte to every read; verify it.
static uint8_t crc8_smbus(uint8_t crc, uint8_t data) {
  for (uint8_t i = 0; i < 8; i++) {
    if ((crc ^ data) & 0x80) crc = (crc << 1) ^ 0x07;
    else                      crc <<= 1;
    data <<= 1;
  }
  return crc;
}

static bool verify_pec(byte addr, byte reg, byte lo, byte hi, byte pec) {
  uint8_t crc = 0;
  crc = crc8_smbus(crc, addr << 1);       // SLA+W
  crc = crc8_smbus(crc, reg);             // register
  crc = crc8_smbus(crc, (addr << 1) | 1); // SLA+R
  crc = crc8_smbus(crc, lo);
  crc = crc8_smbus(crc, hi);
  return (crc == pec);
}

// ── MLX90614 16-bit register read ────────────────────────────────────────
// Returns temperature in °C, or -999 on error.
// Sets CRC_FAIL / SENSOR_ERR bits in *flagsOut.
static float mlx_read_temp(byte addr, byte reg, byte *flagsOut) {
  uint8_t status = I2c.read(addr, reg, 3); // reads lo, hi, pec into buffer
  if (status != 0) {
    *flagsOut |= FLAG_SENSOR_ERR;
    return -999.0f;
  }

  byte lo  = I2c.receive();
  byte hi  = I2c.receive();
  byte pec = I2c.receive();

  if (!verify_pec(addr, reg, lo, hi, pec)) {
    *flagsOut |= FLAG_CRC_FAIL;
    // still return the temperature but flag the CRC error
  }

  if (hi & 0x80) {                         // error bit set by sensor
    *flagsOut |= FLAG_SENSOR_ERR;
    return -999.0f;
  }

  uint16_t raw = ((uint16_t)(hi & 0x7F) << 8) | lo;
  return (raw * 0.02f) - 273.15f;          // fixed: 273.15, not 273.16
}

// ── Mux select: drive PC0-3 to activate one sensor via 74HC138 ───────────
static void select_sensor(uint8_t index) {
  PORTC = (PORTC & 0xF0) | (index & 0x0F);
  delayMicroseconds(150);                  // allow bus to settle
}

// ─────────────────────────────────────────────────────────────────────────
// MCP2515 CAN CONTROLLER
// ─────────────────────────────────────────────────────────────────────────

static void mcp_write_reg(byte reg, byte val) {
  digitalWrite(CAN_CS_PIN, LOW);
  SPI.transfer(MCP_WRITE);
  SPI.transfer(reg);
  SPI.transfer(val);
  digitalWrite(CAN_CS_PIN, HIGH);
}

static byte mcp_read_reg(byte reg) {
  digitalWrite(CAN_CS_PIN, LOW);
  SPI.transfer(MCP_READ);
  SPI.transfer(reg);
  byte val = SPI.transfer(0x00);
  digitalWrite(CAN_CS_PIN, HIGH);
  return val;
}

// Initialise MCP2515 for 500 kbps at 16 MHz crystal
// CNF values from MCP2515 baud rate table (16 MHz, 500kbps):
//   CNF1 = 0x00, CNF2 = 0x90, CNF3 = 0x02
static bool can_init() {
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV4);     // 4 MHz SPI
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  pinMode(CAN_CS_PIN, OUTPUT);
  digitalWrite(CAN_CS_PIN, HIGH);

  // Reset MCP2515
  digitalWrite(CAN_CS_PIN, LOW);
  SPI.transfer(MCP_RESET);
  digitalWrite(CAN_CS_PIN, HIGH);
  delay(10);

  // Enter config mode, set baud rate
  mcp_write_reg(MCP_CANCTRL, 0x80);        // request config mode
  mcp_write_reg(MCP_CNF1, 0x00);
  mcp_write_reg(MCP_CNF2, 0x90);
  mcp_write_reg(MCP_CNF3, 0x02);

  // Normal mode
  mcp_write_reg(MCP_CANCTRL, 0x00);
  delay(5);

  byte stat = mcp_read_reg(MCP_CANSTAT);
  if ((stat & 0xE0) != 0x00) {
    Serial.println(F("ERROR: MCP2515 did not enter normal mode"));
    return false;
  }
  return true;
}

// Pack TireCorner into an 8-byte MoTeC CAN frame and transmit on TXB0
// INT16 little-endian, factor 0.02  →  raw = (°C / 0.02)
static void can_tx_corner(uint8_t index, const TireCorner &c) {
  uint16_t can_id = CAN_BASE_ID + index;
  byte buf[8];

  auto encode = [](float t) -> int16_t {
    if (t < -900) return -9999;            // error sentinel
    return (int16_t)(t / 0.02f);
  };

  int16_t out = encode(c.outer);
  int16_t mid = encode(c.mid);
  int16_t inn = encode(c.inner);

  // ambient: uint8, offset -40  (range -40…+85 → 0…125)
  byte amb = (c.ambient > -900) ? (byte)(c.ambient + 40.0f) : 0;

  buf[0] = (byte)(out & 0xFF);             // outer LSB
  buf[1] = (byte)((out >> 8) & 0xFF);      // outer MSB
  buf[2] = (byte)(mid & 0xFF);
  buf[3] = (byte)((mid >> 8) & 0xFF);
  buf[4] = (byte)(inn & 0xFF);
  buf[5] = (byte)((inn >> 8) & 0xFF);
  buf[6] = amb;
  buf[7] = c.status;

  // Load TX buffer 0
  digitalWrite(CAN_CS_PIN, LOW);
  SPI.transfer(MCP_LOAD_TXB0);             // load TXB0 start at SIDH
  SPI.transfer((can_id >> 3) & 0xFF);      // TXB0SIDH
  SPI.transfer((can_id & 0x07) << 5);      // TXB0SIDL (standard frame)
  SPI.transfer(0x00);                      // TXB0EID8
  SPI.transfer(0x00);                      // TXB0EID0
  SPI.transfer(8);                         // TXB0DLC = 8
  for (uint8_t i = 0; i < 8; i++) SPI.transfer(buf[i]);
  digitalWrite(CAN_CS_PIN, HIGH);

  // Request transmit
  digitalWrite(CAN_CS_PIN, LOW);
  SPI.transfer(MCP_RTS_TXB0);
  digitalWrite(CAN_CS_PIN, HIGH);
}

// ─────────────────────────────────────────────────────────────────────────
// ARDUINO SETUP / LOOP
// ─────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);                    // upgraded from 9600 for faster debug
  Serial.println(F("FSAE Tire Temp System v2.0"));

  // Configure PC0-3 as outputs for mux/address select
  DDRC  |= 0x0F;
  PORTC &= 0xF0;

  // I2C init with 50 ms timeout to prevent lockup on sensor fault
  I2c.begin();
  I2c.timeOut(50);

  // CAN init
  if (can_init()) {
    Serial.println(F("CAN OK — 500kbps"));
  }
}

void loop() {
  unsigned long t0 = millis();

  for (uint8_t i = 0; i < 4; i++) {
    TireCorner &c = corners[i];
    c.status = i & 0x03;                   // bits 1:0 = wheel index

    select_sensor(i);

    // For a single-zone sensor mount, outer/mid/inner come from
    // three sequential reads (servo sweep). Without a servo, all
    // three return the same FOV — replace with servo position code.
    byte flags = 0;
    c.outer   = mlx_read_temp(c.addr, MLX_REG_TOBJ1, &flags);
    c.mid     = mlx_read_temp(c.addr, MLX_REG_TOBJ1, &flags);
    c.inner   = mlx_read_temp(c.addr, MLX_REG_TOBJ1, &flags);
    c.ambient = mlx_read_temp(c.addr, MLX_REG_TA,    &flags);

    // Apply thermal alert flags
    if (c.mid > TEMP_WARN_HIGH)  flags |= FLAG_OVERTEMP;
    c.status |= flags;
    c.valid   = !(flags & FLAG_SENSOR_ERR);

    // Transmit CAN frame for this corner
    can_tx_corner(i, c);

    // Serial debug (disable in competition to save CPU)
    Serial.print(cornerNames[i]);
    Serial.print(F(" | O:"));  Serial.print(c.outer, 1);
    Serial.print(F(" M:"));    Serial.print(c.mid,   1);
    Serial.print(F(" I:"));    Serial.print(c.inner,  1);
    Serial.print(F(" Ta:"));   Serial.print(c.ambient,1);
    if (flags & FLAG_CRC_FAIL)   Serial.print(F(" [CRC!]"));
    if (flags & FLAG_SENSOR_ERR) Serial.print(F(" [ERR!]"));
    if (flags & FLAG_OVERTEMP)   Serial.print(F(" [HOT!]"));
    Serial.println(F(" C"));
  }

  // Hold 100 ms loop period (10 Hz)
  long elapsed = millis() - t0;
  if (elapsed < 100) delay(100 - elapsed);
}
