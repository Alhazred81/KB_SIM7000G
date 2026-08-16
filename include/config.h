//config.h

#pragma once

// ─── Pinout (TTGO T-SIM7000G) ───────────────────────────────
#define MODEM_TX       27
#define MODEM_RX       26
#define MODEM_PWRKEY    4
#define MODEM_RST      5
#define MODEM_DTR      25
#define MODEM_RI       33

// ─── LED / Panelverzió opciók ───────────────────────────────
// LED mód: 0=GPIO12 (V1.0), 1=GPIO13 (V1.1 alt.), 2=Egyeni GPIO, 3=AT halozati LED
#define LED_MODE_V10        0
#define LED_MODE_V11        1
#define LED_MODE_CUSTOM     2
#define LED_MODE_AT_NETLIGHT 3

#define LED_PIN_V10        12
#define LED_PIN_V11        13
#define DEFAULT_LED_MODE   LED_MODE_V10
#define DEFAULT_CUSTOM_PIN 2

// ─── WiFi AP ────────────────────────────────────────────────
#define AP_PREFIX        "KB-teszt-"
#define DEFAULT_AP_PASS  "12345678"
#define DEFAULT_CHANNEL  6

// ─── SMS ────────────────────────────────────────────────────
#define SMS_COOLDOWN_MS  30000UL
#define SMS_MAX_LEN      160

// ─── GNSS ───────────────────────────────────────────────────
#define GNSS_POLL_INTERVAL_MS  5000UL
#define GNSS_DEFAULT_ASSIST_LAT 47.514600f
#define GNSS_DEFAULT_ASSIST_LON 19.043500f

// ─── STA (kliens) WiFi mód ────────────────────────────────────
#define STA_CONNECT_TIMEOUT_MS   15000UL
#define STA_RETRY_INTERVAL_MS    30000UL   // sikertelen csatlakozás után ennyit vár, mielőtt AP-ra esik vissza

// ─── EEPROM layout (256 byte) ───────────────────────────────
//   0.. 31  – AP jelszó titkosítva       (32 byte, 4×XTEA blokk)
//  32.. 32  – WiFi csatorna              (1 byte)
//  33.. 33  – PIN flag                   (1 byte, 0xA5 = webről mentett)
//  34.. 41  – XTEA titkosított SIM PIN   (8 byte)
//  42.. 61  – SIM CCID                   (20 byte, null-term)
//  62.. 62  – CCID flag                  (1 byte, 0xA5 = van mentett CCID)
//  63.. 63  – LED mód                   (1 byte, 0-3, ld. fent)
//  64.. 64  – Egyéni GPIO pin szám       (1 byte, csak LED_MODE_CUSTOM esetén)
//  65.. 65  – STA flag                  (1 byte, 0xA5 = van mentett STA WiFi)
//  66..161  – STA SSID titkosítva       (96 byte, 12×XTEA blokk, max 95 char)
// 162..225  – STA jelszó titkosítva     (64 byte, 8×XTEA blokk, max 63 char)
// 226..234  – GNSS kiindulo koordinata  (flag+lat+lon, 9 byte)
// 235..235  – SMS inbox limit          (1 byte, 1-50, 0/0xFF = alapertelmezett)
// 236..255  – fenntartva
// 256..511  – Szenzor beallitasok (ld. lent, sensors.h dokumentacio)
#define EEPROM_SIZE       512
#define ADDR_AP_PASS        0   // 32 byte
#define ADDR_CHANNEL       32   //  1 byte
#define ADDR_PIN_FLAG      33   //  1 byte
#define ADDR_PIN           34   //  8 byte
#define ADDR_CCID          42   // 20 byte
#define ADDR_CCID_FLAG     62   //  1 byte
#define ADDR_LED_MODE      63   //  1 byte
#define ADDR_LED_CUSTOM    64   //  1 byte
#define ADDR_STA_FLAG      65   //  1 byte
#define ADDR_STA_SSID      66   // 96 byte
#define ADDR_STA_PASS     162   // 64 byte
#define ADDR_GNSS_ASSIST_FLAG 226 //  1 byte
#define ADDR_GNSS_ASSIST_LAT  227 //  4 byte float
#define ADDR_GNSS_ASSIST_LON  231 //  4 byte float
#define ADDR_SMS_INBOX_LIMIT  235 //  1 byte

// ─── Szenzor beallitasok EEPROM cimei (256-tol) ─────────────
// 256      – flag (0xA5 = mentve)
// 257      – bitmask: melyik szenzor van bekapcsolva (1 byte, 6 bit hasznalt)
// 258..261 – RS485 baudrate (4 byte, uint32_t)
// 262      – szelsebesseg Modbus cim (1 byte, 1-247)
// 263      – szelirany Modbus cim   (1 byte, 1-247)
// 264      – SHT57 Modbus cim       (1 byte, 1-247)
// 265      – esoszenzor Modbus cim  (1 byte, 1-247, csak ha Modbus modban)
// 266      – esoszenzor mod (1 byte, 0=analog bemenet, 1=Modbus/RS485)
#define ADDR_SENS_FLAG          256
#define ADDR_SENS_ENABLE_MASK   257
#define ADDR_SENS_RS485_BAUD    258
#define ADDR_SENS_ADDR_WINDSPEED 262
#define ADDR_SENS_ADDR_WINDDIR   263
#define ADDR_SENS_ADDR_SHT       264
#define ADDR_SENS_ADDR_RAIN      265
#define ADDR_SENS_RAIN_MODE      266

#define MAGIC_BYTE        0xA5

// SMS inbox: fordítás-idejű felső korlát (fix tömbméret), és
// futásidőben állítható tényleges limit (<=ez alá), amit EEPROM-ban
// tárolunk. Az alapértelmezett 10, a maximum 50.
#define SMS_INBOX_HARD_MAX 50
#define SMS_INBOX_DEFAULT_LIMIT 10

// ─── Szenzorok - alapertelmezett GPIO-kiosztas ──────────────
// Ezek nem utkoznek a modem/LED labakkal (4,5,12,13,25,26,27,33).
#define SENS_I2C1_SDA_DEFAULT   21   // MPU6050
#define SENS_I2C1_SCL_DEFAULT   22
#define SENS_I2C2_SDA_DEFAULT   32   // AHT20 + BMP280 + LTR390
#define SENS_I2C2_SCL_DEFAULT   14
#define SENS_RS485_RX_DEFAULT   16   // szelsebesseg/-irany (Modbus RTU)
#define SENS_RS485_TX_DEFAULT   17
#define SENS_RS485_DE_DEFAULT   15   // DE/RE iranyvezerlo (RS485 adapteren)
#define SENS_SHT_PIN_DEFAULT    18   // SHT57 ho-/paramero
#define SENS_RAIN_PIN_DEFAULT   34   // esoszenzor (analog bemenet)

#define SENS_RS485_BAUD_DEFAULT 9600UL
#define SENS_POLL_INTERVAL_MS   3000UL

// Szenzor bitmask pozicioi (melyik van bekapcsolva)
#define SENS_BIT_WINDSPEED   0
#define SENS_BIT_WINDDIR     1
#define SENS_BIT_SHT         2
#define SENS_BIT_RAIN        3
#define SENS_BIT_MPU6050     4
#define SENS_BIT_AHT20BMP280 5
#define SENS_BIT_LTR390      6
