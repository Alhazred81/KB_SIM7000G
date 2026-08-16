//sensors.h 

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include "config.h"

// ════════════════════════════════════════════════════════════
//  Szenzor modul - RS485 szel, SHT ho/para, eso, MPU6050 (I2C1),
//  AHT20+BMP280+LTR390 (I2C2)
// ════════════════════════════════════════════════════════════
// Minden szenzor sajat konnyu (konyvtar-fuggetlen) implementaciot
// hasznal, mert a pontos hardver-modellek/regiszter-terkepek
// gyartonkent elterhetnek - igy konnyebb utolagosan finomhangolni
// egy konkret modulhoz, ha a nyers ertekek nem stimmelnek.

// ─── Allapot strukturak ──────────────────────────────────────
// Szelsebesseg es szelirany gyakran KET KULON Modbus-eszkoz egy RS485
// buszon, sajat cimmel - ezert kulon allapot/cim mindegyiknek, nem egy
// kombinalt szenzorkent kezelve.
struct WindSpeedState {
  bool    enabled     = false;
  bool    lastReadOk  = false;
  float   speedMs     = 0;
  uint8_t modbusAddr  = 2;      // allithato, DFRobot SEN0483 alapertelmezes pl. 2
  unsigned long lastPoll = 0;
  unsigned long lastGoodRead = 0;
  String  lastError   = "";
  String  rawHex      = "";
};

struct WindDirState {
  bool    enabled     = false;
  bool    lastReadOk  = false;
  float   directionDeg= 0;
  uint8_t modbusAddr  = 3;      // allithato, DFRobot SEN0482 alapertelmezes pl. 3
  unsigned long lastPoll = 0;
  unsigned long lastGoodRead = 0;
  String  lastError   = "";
  String  rawHex      = "";
};

// SHT57: RS485/Modbus-os ho-/paramero (nem 1-wire digitalis - javitva
// a korabbi verziohoz kepest, mert a cimezhetoseg RS485-re utal).
struct ShtSensorState {
  bool    enabled     = false;
  bool    lastReadOk  = false;
  float   tempC       = 0;
  float   humidityPct = 0;
  uint8_t modbusAddr  = 4;      // allithato
  unsigned long lastPoll = 0;
  unsigned long lastGoodRead = 0;
  String  lastError   = "";
  String  rawHex      = "";
};

struct RainSensorState {
  bool    enabled     = false;
  int     rawValue    = 0;      // 0-4095 (ESP32 ADC), VAGY Modbus nyers ertek
  int     percentWet  = 0;      // 0=szaraz, 100=csuromvizes (kalibralando)
  bool    isRaining    = false; // egyszerusitett kuszob-alapu jelzes
  bool    isModbus     = false; // true = RS485/Modbus eso-erzekelo, false = analog bemenet
  uint8_t modbusAddr   = 5;     // csak ha isModbus=true
  unsigned long lastPoll = 0;
  String  lastError    = "";
  String  rawHex       = "";
};

struct Mpu6050State {
  bool    enabled     = false;
  bool    lastReadOk  = false;
  float   accelX = 0, accelY = 0, accelZ = 0; // g
  float   gyroX = 0, gyroY = 0, gyroZ = 0;    // deg/s
  float   tempC = 0;
  unsigned long lastPoll = 0;
  unsigned long lastGoodRead = 0;
  String  lastError = "";
};

struct Aht20Bmp280State {
  bool    enabled     = false;
  bool    lastReadOk  = false;
  bool    ahtOk       = false;
  bool    bmpOk       = false;
  float   ahtTempC    = 0;
  float   ahtHumidityPct = 0;
  float   bmpTempC    = 0;
  float   bmpPressureHpa = 0;
  unsigned long lastPoll = 0;
  unsigned long lastGoodRead = 0;
  String  lastError   = "";
};

struct Ltr390State {
  bool    enabled     = false;
  bool    lastReadOk  = false;
  uint32_t uvRaw       = 0;
  float   uvIndex      = 0;
  unsigned long lastPoll = 0;
  unsigned long lastGoodRead = 0;
  String  lastError   = "";
};

extern WindSpeedState   gWindSpeed;
extern WindDirState     gWindDir;
extern ShtSensorState    gSht;
extern RainSensorState   gRain;
extern Mpu6050State      gMpu;
extern Aht20Bmp280State  gAhtBmp;
extern Ltr390State       gLtr;

// ─── Globalis szenzor-konfig (bekapcsolt allapotok + RS485 baud) ─
uint8_t gSensEnableMask = 0;
uint32_t gSensRs485Baud = SENS_RS485_BAUD_DEFAULT;
unsigned long gLastSensorPoll = 0;
uint8_t gSensorPollStep = 0;

bool sensEnabled(uint8_t bit) { return (gSensEnableMask >> bit) & 1; }
void sensSetEnabled(uint8_t bit, bool en) {
  if(en) gSensEnableMask |= (1 << bit);
  else   gSensEnableMask &= ~(1 << bit);
}

// Modbus-cimek mentese/betoltese - a WindSensorState-bol WindSpeed/Dir-re
// valtas miatt most 4 kulon RS485-eszkoz cimet taroljuk (szel-sebesseg,
// szel-irany, SHT, eso), plusz az esoszenzor uzemmodjat (analog/Modbus).
void saveSensorConfig() {
  EEPROM.write(ADDR_SENS_FLAG, MAGIC_BYTE);
  EEPROM.write(ADDR_SENS_ENABLE_MASK, gSensEnableMask);
  EEPROM.put(ADDR_SENS_RS485_BAUD, gSensRs485Baud);
  EEPROM.write(ADDR_SENS_ADDR_WINDSPEED, gWindSpeed.modbusAddr);
  EEPROM.write(ADDR_SENS_ADDR_WINDDIR,   gWindDir.modbusAddr);
  EEPROM.write(ADDR_SENS_ADDR_SHT,       gSht.modbusAddr);
  EEPROM.write(ADDR_SENS_ADDR_RAIN,      gRain.modbusAddr);
  EEPROM.write(ADDR_SENS_RAIN_MODE,      gRain.isModbus ? 1 : 0);
  EEPROM.commit();
}

void loadSensorConfig() {
  if(EEPROM.read(ADDR_SENS_FLAG) != MAGIC_BYTE) {
    gSensEnableMask = 0; // alapbol minden szenzor kikapcsolva
    gSensRs485Baud = SENS_RS485_BAUD_DEFAULT;
    return;
  }
  gSensEnableMask = EEPROM.read(ADDR_SENS_ENABLE_MASK);
  EEPROM.get(ADDR_SENS_RS485_BAUD, gSensRs485Baud);
  if(gSensRs485Baud == 0 || gSensRs485Baud > 921600) gSensRs485Baud = SENS_RS485_BAUD_DEFAULT;

  uint8_t v;
  v = EEPROM.read(ADDR_SENS_ADDR_WINDSPEED); if(v >= 1 && v <= 247) gWindSpeed.modbusAddr = v;
  v = EEPROM.read(ADDR_SENS_ADDR_WINDDIR);   if(v >= 1 && v <= 247) gWindDir.modbusAddr = v;
  v = EEPROM.read(ADDR_SENS_ADDR_SHT);       if(v >= 1 && v <= 247) gSht.modbusAddr = v;
  v = EEPROM.read(ADDR_SENS_ADDR_RAIN);      if(v >= 1 && v <= 247) gRain.modbusAddr = v;
  gRain.isModbus = (EEPROM.read(ADDR_SENS_RAIN_MODE) == 1);
}

// ════════════════════════════════════════════════════════════
//  RS485 (Modbus RTU) - szelsebesseg + -irany
// ════════════════════════════════════════════════════════════
HardwareSerial rs485Serial(2); // UART2 - a modem mar UART1-et hasznalja
bool gRs485Initialized = false;

// CRC16 Modbus szamitas
uint16_t modbusCrc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for(size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for(int b = 0; b < 8; b++) {
      if(crc & 1) { crc >>= 1; crc ^= 0xA001; }
      else crc >>= 1;
    }
  }
  return crc;
}

void rs485Init() {
  rs485Serial.begin(gSensRs485Baud, SERIAL_8N1, SENS_RS485_RX_DEFAULT, SENS_RS485_TX_DEFAULT);
  pinMode(SENS_RS485_DE_DEFAULT, OUTPUT);
  digitalWrite(SENS_RS485_DE_DEFAULT, LOW); // alapbol vetel-mod
  gRs485Initialized = true;
}

void rs485SetDirection(bool transmit) {
  digitalWrite(SENS_RS485_DE_DEFAULT, transmit ? HIGH : LOW);
  delayMicroseconds(50); // az adapter atkapcsolasi ideje
}

// Modbus "Read Holding Registers" (function 0x03) kerese es valasz-olvasas.
// Visszaadja a nyers regiszter-erteket (uint16_t tombben), vagy false-t hiba eseten.
bool modbusReadHoldingRegisters(uint8_t slaveAddr, uint16_t startReg, uint8_t count,
                                 uint16_t* outValues, String& rawHexOut, String& errOut) {
  if(!gRs485Initialized) rs485Init();

  uint8_t req[8];
  req[0] = slaveAddr;
  req[1] = 0x03;
  req[2] = (startReg >> 8) & 0xFF;
  req[3] = startReg & 0xFF;
  req[4] = (count >> 8) & 0xFF;
  req[5] = count & 0xFF;
  uint16_t crc = modbusCrc16(req, 6);
  req[6] = crc & 0xFF;
  req[7] = (crc >> 8) & 0xFF;

  // Kimeno puffer ürítése, majd kérés küldése
  while(rs485Serial.available()) rs485Serial.read();
  rs485SetDirection(true);
  rs485Serial.write(req, 8);
  rs485Serial.flush();
  rs485SetDirection(false);

  // Válasz várása (max 500ms - RS485 szenzorok tipikusan gyorsak)
  unsigned long start = millis();
  uint8_t resp[64]; int respLen = 0;
  while(millis() - start < 500 && respLen < 64) {
    if(rs485Serial.available()) {
      resp[respLen++] = rs485Serial.read();
    }
    yield();
  }

  rawHexOut = "";
  for(int i = 0; i < respLen; i++) {
    if(resp[i] < 16) rawHexOut += "0";
    rawHexOut += String(resp[i], HEX);
    rawHexOut += " ";
  }

  if(respLen < 5) {
    errOut = "Nincs/hianyos valasz (" + String(respLen) + " byte erkezett). "
             "Ellenorizd a bekotest (A/B vezetek, DE/RE), a baudrate-et es a Modbus-cimet.";
    return false;
  }
  if(resp[0] != slaveAddr) {
    errOut = "Valasz mas cimrol erkezett (vart: " + String(slaveAddr) + ", kapott: " + String(resp[0]) + ").";
    return false;
  }
  if(resp[1] != 0x03) {
    errOut = "A modem hibat jelzett (function code " + String(resp[1], HEX) + " - Modbus exception).";
    return false;
  }
  uint8_t byteCount = resp[2];
  if(respLen < 3 + byteCount + 2) {
    errOut = "Csonka valasz (vart " + String(3 + byteCount + 2) + " byte, kapott " + String(respLen) + ").";
    return false;
  }
  for(int i = 0; i < count && i < byteCount / 2; i++) {
    outValues[i] = (resp[3 + i*2] << 8) | resp[4 + i*2];
  }
  return true;
}

// ─── Szélsebesség lekérdezés (önálló Modbus-eszköz) ─────────
// Leggyakoribb regiszter: 0x0000 = sebesseg, x10-es skalazassal (m/s).
void windSpeedPoll() {
  if(!gWindSpeed.enabled) return;
  gWindSpeed.lastPoll = millis();

  uint16_t vals[1];
  String err;
  bool ok = modbusReadHoldingRegisters(gWindSpeed.modbusAddr, 0x0000, 1, vals, gWindSpeed.rawHex, err);

  if(!ok) {
    gWindSpeed.lastReadOk = false;
    gWindSpeed.lastError = err;
    return;
  }
  gWindSpeed.speedMs = vals[0] / 10.0f;
  gWindSpeed.lastReadOk = true;
  gWindSpeed.lastGoodRead = millis();
  gWindSpeed.lastError = "";
}

// ─── Szélirány lekérdezés (önálló Modbus-eszköz) ─────────────
// Leggyakoribb regiszter: 0x0000 = irany fokban, x10-es skalazassal.
void windDirPoll() {
  if(!gWindDir.enabled) return;
  gWindDir.lastPoll = millis();

  uint16_t vals[1];
  String err;
  bool ok = modbusReadHoldingRegisters(gWindDir.modbusAddr, 0x0000, 1, vals, gWindDir.rawHex, err);

  if(!ok) {
    gWindDir.lastReadOk = false;
    gWindDir.lastError = err;
    return;
  }
  gWindDir.directionDeg = vals[0] / 10.0f;
  gWindDir.lastReadOk = true;
  gWindDir.lastGoodRead = millis();
  gWindDir.lastError = "";
}

// ════════════════════════════════════════════════════════════
//  SHT57 - RS485/Modbus-os ho-/paramero
// ════════════════════════════════════════════════════════════
// Leggyakoribb regiszter-elrendezes ipari RS485 ho/para szenzoroknal:
// 0x0000 = paratartalom (x10 %RH), 0x0001 = homerseklet (x10 C).
// Ha a te konkret moduled mas sorrendet/skalazast hasznal, itt kell
// finomitani - a nyers valasz a diagnosztikan mindig lathato lesz.
void shtSensorPoll() {
  if(!gSht.enabled) return;
  gSht.lastPoll = millis();

  uint16_t vals[2];
  String err;
  bool ok = modbusReadHoldingRegisters(gSht.modbusAddr, 0x0000, 2, vals, gSht.rawHex, err);

  if(!ok) {
    gSht.lastReadOk = false;
    gSht.lastError = err;
    return;
  }
  gSht.humidityPct = vals[0] / 10.0f;
  gSht.tempC = vals[1] / 10.0f;
  gSht.lastReadOk = true;
  gSht.lastGoodRead = millis();
  gSht.lastError = "";
}

// ════════════════════════════════════════════════════════════
//  Esőszenzor - analóg bemenet VAGY RS485/Modbus (allithato)
// ════════════════════════════════════════════════════════════
void rainSensorPoll() {
  if(!gRain.enabled) return;
  gRain.lastPoll = millis();

  if(gRain.isModbus) {
    uint16_t vals[1];
    String err;
    bool ok = modbusReadHoldingRegisters(gRain.modbusAddr, 0x0000, 1, vals, gRain.rawHex, err);
    if(!ok) {
      gRain.lastError = err;
      return;
    }
    gRain.rawValue = vals[0];
    // Sok RS485 eso-erzekelo 0-100-as skalan vagy 0-1023-on adja - ha
    // a tied maskepp skalazodik, itt kell igazitani.
    gRain.percentWet = constrain(vals[0], 0, 100);
    gRain.lastError = "";
  } else {
    gRain.rawValue = analogRead(SENS_RAIN_PIN_DEFAULT);
    // ESP32 ADC: 0 (nedves/rovidre zart) .. 4095 (szaraz) - a legtobb
    // olcso esoszenzor-modul eseten forditott logika (kisebb ellenallas
    // = tobb viz = kisebb feszultseg). Ha a tied forditva viselkedik,
    // egyszeruen cserold fel a % szamitast.
    gRain.percentWet = map(gRain.rawValue, 4095, 0, 0, 100);
    if(gRain.percentWet < 0) gRain.percentWet = 0;
    if(gRain.percentWet > 100) gRain.percentWet = 100;
  }
  gRain.isRaining = gRain.percentWet > 20; // egyszeru kuszob, kalibralhato
}

// ════════════════════════════════════════════════════════════
//  I2C1 - MPU6050 (gyorsulasmero + gyroszkop)
// ════════════════════════════════════════════════════════════
#define MPU6050_ADDR 0x68

bool mpu6050WriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

void mpu6050Init() {
  Wire.begin(SENS_I2C1_SDA_DEFAULT, SENS_I2C1_SCL_DEFAULT);
  mpu6050WriteReg(0x6B, 0x00); // PWR_MGMT_1 - ébresztés alvó módból
}

void mpu6050Poll() {
  if(!gMpu.enabled) return;
  gMpu.lastPoll = millis();

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B); // ACCEL_XOUT_H - innentől 14 byte az összes adat
  if(Wire.endTransmission(false) != 0) {
    gMpu.lastReadOk = false;
    gMpu.lastError = "I2C1 nem valaszol (nincs eszkoz a 0x68 cimen?).";
    return;
  }
  uint8_t n = Wire.requestFrom((int)MPU6050_ADDR, 14);
  if(n < 14) {
    gMpu.lastReadOk = false;
    gMpu.lastError = "Hianyos I2C1 valasz (" + String(n) + "/14 byte).";
    return;
  }

  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  int16_t temp = (Wire.read() << 8) | Wire.read();
  int16_t gx = (Wire.read() << 8) | Wire.read();
  int16_t gy = (Wire.read() << 8) | Wire.read();
  int16_t gz = (Wire.read() << 8) | Wire.read();

  gMpu.accelX = ax / 16384.0f; // alapertelmezett +-2g erzekenyseg
  gMpu.accelY = ay / 16384.0f;
  gMpu.accelZ = az / 16384.0f;
  gMpu.gyroX  = gx / 131.0f;   // alapertelmezett +-250 deg/s erzekenyseg
  gMpu.gyroY  = gy / 131.0f;
  gMpu.gyroZ  = gz / 131.0f;
  gMpu.tempC  = temp / 340.0f + 36.53f;

  gMpu.lastReadOk = true;
  gMpu.lastGoodRead = millis();
  gMpu.lastError = "";
}

// ════════════════════════════════════════════════════════════
//  I2C2 - AHT20 + BMP280 + LTR390
// ════════════════════════════════════════════════════════════
#define AHT20_ADDR   0x38
#define BMP280_ADDR  0x76
#define LTR390_ADDR  0x53

void i2c2Init() {
  Wire1.begin(SENS_I2C2_SDA_DEFAULT, SENS_I2C2_SCL_DEFAULT);
  // AHT20 inicializalas
  Wire1.beginTransmission(AHT20_ADDR);
  Wire1.write(0xBE); Wire1.write(0x08); Wire1.write(0x00);
  Wire1.endTransmission();
  delay(10);
  // LTR390 bekapcsolas + UVS mod
  Wire1.beginTransmission(LTR390_ADDR);
  Wire1.write(0x00); Wire1.write(0x0A); // MAIN_CTRL: enable + UVS mode
  Wire1.endTransmission();
}

void ahtBmpPoll() {
  if(!gAhtBmp.enabled) return;
  gAhtBmp.lastPoll = millis();

  // ── AHT20 mérés indítása ──
  Wire1.beginTransmission(AHT20_ADDR);
  Wire1.write(0xAC); Wire1.write(0x33); Wire1.write(0x00);
  bool aOk = (Wire1.endTransmission() == 0);
  delay(80);

  if(aOk) {
    uint8_t n = Wire1.requestFrom((int)AHT20_ADDR, 6);
    if(n >= 6) {
      uint8_t d[6];
      for(int i = 0; i < 6; i++) d[i] = Wire1.read();
      uint32_t rawHum = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
      uint32_t rawTemp = (((uint32_t)d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];
      gAhtBmp.ahtHumidityPct = (rawHum / 1048576.0f) * 100.0f;
      gAhtBmp.ahtTempC = (rawTemp / 1048576.0f) * 200.0f - 50.0f;
      gAhtBmp.ahtOk = true;
    } else {
      gAhtBmp.ahtOk = false;
    }
  } else {
    gAhtBmp.ahtOk = false;
  }

  // ── BMP280 mérés (nyers, kalibráció nélküli közelítés) ──
  // Megjegyzés: a BMP280 pontos hőmérséklet/nyomás számításához a
  // gyári kalibrációs regisztereket (0x88-0xA1) is ki kellene olvasni
  // és a Bosch-féle kompenzációs képletet alkalmazni - ez a leegyszerűsített
  // verzió csak a nyers ADC-értékeket olvassa ki jelzésértékű becslésként.
  // Ha pontos értékre van szükség, szólj és kiegészítjük a teljes
  // kalibrációs számítással.
  Wire1.beginTransmission(BMP280_ADDR);
  Wire1.write(0xF4); Wire1.write(0x27); // ctrl_meas: normal mode, oversampling x1
  bool bOk = (Wire1.endTransmission() == 0);
  delay(10);

  if(bOk) {
    Wire1.beginTransmission(BMP280_ADDR);
    Wire1.write(0xF7); // press_msb - innentől 6 byte (press+temp raw)
    if(Wire1.endTransmission(false) == 0) {
      uint8_t n = Wire1.requestFrom((int)BMP280_ADDR, 6);
      if(n >= 6) {
        uint8_t d[6];
        for(int i = 0; i < 6; i++) d[i] = Wire1.read();
        int32_t rawPress = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
        int32_t rawTemp  = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
        // Kalibráció nélküli közelítés - csak relatív változás követésére jó,
        // abszolút értéknek nem pontos.
        gAhtBmp.bmpTempC = rawTemp / 5120.0f; // durva közelítés
        gAhtBmp.bmpPressureHpa = rawPress / 256.0f / 100.0f; // durva közelítés
        gAhtBmp.bmpOk = true;
      } else {
        gAhtBmp.bmpOk = false;
      }
    } else {
      gAhtBmp.bmpOk = false;
    }
  } else {
    gAhtBmp.bmpOk = false;
  }

  gAhtBmp.lastReadOk = gAhtBmp.ahtOk || gAhtBmp.bmpOk;
  if(gAhtBmp.lastReadOk) gAhtBmp.lastGoodRead = millis();
  if(!gAhtBmp.ahtOk && !gAhtBmp.bmpOk) {
    gAhtBmp.lastError = "Sem az AHT20 (0x38), sem a BMP280 (0x76) nem valaszol I2C2-n.";
  } else if(!gAhtBmp.ahtOk) {
    gAhtBmp.lastError = "Az AHT20 (0x38) nem valaszol, a BMP280 igen.";
  } else if(!gAhtBmp.bmpOk) {
    gAhtBmp.lastError = "A BMP280 (0x76) nem valaszol, az AHT20 igen.";
  } else {
    gAhtBmp.lastError = "";
  }
}

void ltr390Poll() {
  if(!gLtr.enabled) return;
  gLtr.lastPoll = millis();

  Wire1.beginTransmission(LTR390_ADDR);
  Wire1.write(0x10); // UVSDATA regiszter kezdete (3 byte, LSB elsőként)
  if(Wire1.endTransmission(false) != 0) {
    gLtr.lastReadOk = false;
    gLtr.lastError = "I2C2 nem valaszol (nincs eszkoz a 0x53 cimen?).";
    return;
  }
  uint8_t n = Wire1.requestFrom((int)LTR390_ADDR, 3);
  if(n < 3) {
    gLtr.lastReadOk = false;
    gLtr.lastError = "Hianyos I2C2 valasz (" + String(n) + "/3 byte).";
    return;
  }
  uint8_t b0 = Wire1.read(), b1 = Wire1.read(), b2 = Wire1.read();
  gLtr.uvRaw = ((uint32_t)b2 << 16) | ((uint32_t)b1 << 8) | b0;
  // UV Index szamitas a gyari ajanlas szerinti egyszerusitett kepletttel
  // (UVS/UV szenzitivitas ~ 2300 count/UVI, alapertelmezett beallitasoknal)
  gLtr.uvIndex = gLtr.uvRaw / 2300.0f;
  gLtr.lastReadOk = true;
  gLtr.lastGoodRead = millis();
  gLtr.lastError = "";
}

// ════════════════════════════════════════════════════════════
//  Összes szenzor inicializálása/leállítása állapotváltáskor
// ════════════════════════════════════════════════════════════
bool gI2c1Initialized = false;
bool gI2c2Initialized = false;

void sensorsApplyEnabled() {
  bool anyRs485 = sensEnabled(SENS_BIT_WINDSPEED) || sensEnabled(SENS_BIT_WINDDIR) ||
                  sensEnabled(SENS_BIT_SHT) || (sensEnabled(SENS_BIT_RAIN) && gRain.isModbus);
  if(anyRs485 && !gRs485Initialized) rs485Init();
  if(sensEnabled(SENS_BIT_MPU6050) && !gI2c1Initialized) { mpu6050Init(); gI2c1Initialized = true; }
  if((sensEnabled(SENS_BIT_AHT20BMP280) || sensEnabled(SENS_BIT_LTR390)) && !gI2c2Initialized) {
    i2c2Init(); gI2c2Initialized = true;
  }
  gWindSpeed.enabled = sensEnabled(SENS_BIT_WINDSPEED);
  gWindDir.enabled   = sensEnabled(SENS_BIT_WINDDIR);
  gSht.enabled     = sensEnabled(SENS_BIT_SHT);
  gRain.enabled    = sensEnabled(SENS_BIT_RAIN);
  gMpu.enabled     = sensEnabled(SENS_BIT_MPU6050);
  gAhtBmp.enabled  = sensEnabled(SENS_BIT_AHT20BMP280);
  gLtr.enabled     = sensEnabled(SENS_BIT_LTR390);
}

// ─── Periodikus lekérdezési ciklus (loop-ból hívva) ──────────
// ─── Egyszeri, azonnali teszt-lekérdezés (a Sensor oldal "Teszt" gombjaihoz) ─
// Ezek NEM a normal periodikus sensorsLoop()-on keresztul futnak, hanem
// kozvetlenul, a felhasznalo kerese soran - igy akkor is kiprobalhato
// egy eszkoz, ha meg nincs "bekapcsolva" allapotban.
String gLastSensTestResult = "";
bool   gLastSensTestOk     = false;
String gLastSensTestRaw    = "";

void sensTestRun(const String& which) {
  gLastSensTestResult = "";
  gLastSensTestOk = false;
  gLastSensTestRaw = "";

  if(!gRs485Initialized) rs485Init();

  if(which == "windspeed") {
    uint16_t vals[1]; String err, raw;
    bool ok = modbusReadHoldingRegisters(gWindSpeed.modbusAddr, 0x0000, 1, vals, raw, err);
    gLastSensTestRaw = raw;
    if(ok) { gLastSensTestOk = true; gLastSensTestResult = "OK: " + String(vals[0]/10.0f,1) + " m/s (cim " + String(gWindSpeed.modbusAddr) + ")"; }
    else   { gLastSensTestResult = err; }
  }
  else if(which == "winddir") {
    uint16_t vals[1]; String err, raw;
    bool ok = modbusReadHoldingRegisters(gWindDir.modbusAddr, 0x0000, 1, vals, raw, err);
    gLastSensTestRaw = raw;
    if(ok) { gLastSensTestOk = true; gLastSensTestResult = "OK: " + String(vals[0]/10.0f,0) + " fok (cim " + String(gWindDir.modbusAddr) + ")"; }
    else   { gLastSensTestResult = err; }
  }
  else if(which == "sht") {
    uint16_t vals[2]; String err, raw;
    bool ok = modbusReadHoldingRegisters(gSht.modbusAddr, 0x0000, 2, vals, raw, err);
    gLastSensTestRaw = raw;
    if(ok) { gLastSensTestOk = true; gLastSensTestResult = "OK: " + String(vals[1]/10.0f,1) + " C, " + String(vals[0]/10.0f,0) + "% (cim " + String(gSht.modbusAddr) + ")"; }
    else   { gLastSensTestResult = err; }
  }
  else if(which == "rain") {
    if(gRain.isModbus) {
      uint16_t vals[1]; String err, raw;
      bool ok = modbusReadHoldingRegisters(gRain.modbusAddr, 0x0000, 1, vals, raw, err);
      gLastSensTestRaw = raw;
      if(ok) { gLastSensTestOk = true; gLastSensTestResult = "OK: nyers ertek " + String(vals[0]) + " (cim " + String(gRain.modbusAddr) + ")"; }
      else   { gLastSensTestResult = err; }
    } else {
      int raw = analogRead(SENS_RAIN_PIN_DEFAULT);
      gLastSensTestOk = true;
      gLastSensTestResult = "OK: analog nyers ertek " + String(raw) + " (GPIO" + String(SENS_RAIN_PIN_DEFAULT) + ")";
    }
  }
  else {
    gLastSensTestResult = "Ismeretlen szenzor: " + which;
  }
}

void sensorsLoop() {
  if(millis() - gLastSensorPoll < SENS_POLL_INTERVAL_MS) return;
  gLastSensorPoll = millis();

  for(uint8_t tries = 0; tries < 7; tries++) {
    uint8_t step = gSensorPollStep;
    gSensorPollStep = (gSensorPollStep + 1) % 7;

    if(step == 0 && gWindSpeed.enabled) { windSpeedPoll(); return; }
    if(step == 1 && gWindDir.enabled)   { windDirPoll(); return; }
    if(step == 2 && gSht.enabled)       { shtSensorPoll(); return; }
    if(step == 3 && gRain.enabled)      { rainSensorPoll(); return; }
    if(step == 4 && gMpu.enabled)       { mpu6050Poll(); return; }
    if(step == 5 && gAhtBmp.enabled)    { ahtBmpPoll(); return; }
    if(step == 6 && gLtr.enabled)       { ltr390Poll(); return; }
  }
}
