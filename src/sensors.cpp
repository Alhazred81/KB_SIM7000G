//sensors.cpp

#include "sensors.h"

// ─── Globális változók definíciója ───────────────────────────
uint8_t gSensEnableMask = 0;
uint32_t gSensRs485Baud = SENS_RS485_BAUD_DEFAULT;
unsigned long gLastSensorPoll = 0;
uint8_t gSensorPollStep = 0;

HardwareSerial rs485Serial(2); // UART2 - a modem mar UART1-et hasznalja
bool gRs485Initialized = false;

bool gI2c1Initialized = false;
bool gI2c2Initialized = false;

String gLastSensTestResult = "";
bool   gLastSensTestOk     = false;
String gLastSensTestRaw    = "";

// ─── I2C Címek ───────────────────────────────────────────────
#define MPU6050_ADDR 0x68
#define AHT20_ADDR   0x38
#define BMP280_ADDR  0x76
#define LTR390_ADDR  0x53


// ─── Függvények megvalósítása ────────────────────────────────

bool sensEnabled(uint8_t bit) { 
  return (gSensEnableMask >> bit) & 1; 
}

void sensSetEnabled(uint8_t bit, bool en) {
  if(en) gSensEnableMask |= (1 << bit);
  else   gSensEnableMask &= ~(1 << bit);
}

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
    gSensEnableMask = 0; 
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
  digitalWrite(SENS_RS485_DE_DEFAULT, LOW); 
  gRs485Initialized = true;
}

void rs485SetDirection(bool transmit) {
  digitalWrite(SENS_RS485_DE_DEFAULT, transmit ? HIGH : LOW);
  delayMicroseconds(50); 
}

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

  while(rs485Serial.available()) rs485Serial.read();
  rs485SetDirection(true);
  rs485Serial.write(req, 8);
  rs485Serial.flush();
  rs485SetDirection(false);

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
    gRain.percentWet = constrain(vals[0], 0, 100);
    gRain.lastError = "";
  } else {
    gRain.rawValue = analogRead(SENS_RAIN_PIN_DEFAULT);
    gRain.percentWet = map(gRain.rawValue, 4095, 0, 0, 100);
    if(gRain.percentWet < 0) gRain.percentWet = 0;
    if(gRain.percentWet > 100) gRain.percentWet = 100;
  }
  gRain.isRaining = gRain.percentWet > 20; 
}

bool mpu6050WriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

void mpu6050Init() {
  Wire.begin(SENS_I2C1_SDA_DEFAULT, SENS_I2C1_SCL_DEFAULT);
  mpu6050WriteReg(0x6B, 0x00); 
}

void mpu6050Poll() {
  if(!gMpu.enabled) return;
  gMpu.lastPoll = millis();

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B); 
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

  gMpu.accelX = ax / 16384.0f; 
  gMpu.accelY = ay / 16384.0f;
  gMpu.accelZ = az / 16384.0f;
  gMpu.gyroX  = gx / 131.0f;   
  gMpu.gyroY  = gy / 131.0f;
  gMpu.gyroZ  = gz / 131.0f;
  gMpu.tempC  = temp / 340.0f + 36.53f;

  gMpu.lastReadOk = true;
  gMpu.lastGoodRead = millis();
  gMpu.lastError = "";
}

void i2c2Init() {
  Wire1.begin(SENS_I2C2_SDA_DEFAULT, SENS_I2C2_SCL_DEFAULT);
  Wire1.beginTransmission(AHT20_ADDR);
  Wire1.write(0xBE); Wire1.write(0x08); Wire1.write(0x00);
  Wire1.endTransmission();
  delay(10);
  Wire1.beginTransmission(LTR390_ADDR);
  Wire1.write(0x00); Wire1.write(0x0A); 
  Wire1.endTransmission();
}

void ahtBmpPoll() {
  if(!gAhtBmp.enabled) return;
  gAhtBmp.lastPoll = millis();

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

  Wire1.beginTransmission(BMP280_ADDR);
  Wire1.write(0xF4); Wire1.write(0x27); 
  bool bOk = (Wire1.endTransmission() == 0);
  delay(10);

  if(bOk) {
    Wire1.beginTransmission(BMP280_ADDR);
    Wire1.write(0xF7); 
    if(Wire1.endTransmission(false) == 0) {
      uint8_t n = Wire1.requestFrom((int)BMP280_ADDR, 6);
      if(n >= 6) {
        uint8_t d[6];
        for(int i = 0; i < 6; i++) d[i] = Wire1.read();
        int32_t rawPress = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
        int32_t rawTemp  = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
        gAhtBmp.bmpTempC = rawTemp / 5120.0f; 
        gAhtBmp.bmpPressureHpa = rawPress / 256.0f / 100.0f; 
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
  Wire1.write(0x10); 
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
  gLtr.uvIndex = gLtr.uvRaw / 2300.0f;
  gLtr.lastReadOk = true;
  gLtr.lastGoodRead = millis();
  gLtr.lastError = "";
}

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