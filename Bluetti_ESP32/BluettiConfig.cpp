#include "Arduino.h"
#include "PowerStation.h"
#include "config.h"

#if POWER_STATION(AC300)
#include "Device_AC300.h"
#elif POWER_STATION(AC200M)
#include "Device_AC200M.h"
#elif POWER_STATION(EP500)
#include "Device_EP500.h"
#elif POWER_STATION(EB3A)
#include "Device_EB3A.h"
#elif POWER_STATION(EP500P)
#include "Device_EP500P.h"
#elif POWER_STATION(AC500)
#include "Device_AC500.h"
#endif

volatile device_field_data_t bluetti_state_data[sizeof(bluetti_device_state) / sizeof(device_field_data_t)];