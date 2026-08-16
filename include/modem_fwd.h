modem_fwd.h

#pragma once

#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024

#include "modem_types.h"

extern HardwareSerial modemSerial;
extern TinyGsm modem;

extern ModemState gModem;