#ifndef DEVICE_EB3A_H
#define DEVICE_EB3A_H
#include "Arduino.h"
#include "DeviceType.h"

/* Not implemented yet
enum output_mode {
    STOP = 0,
    INVERTER_OUTPUT = 1,
    BYPASS_OUTPUT_C = 2,
    BYPASS_OUTPUT_D = 3,
    LOAD_MATCHING = 4
};

enum ups_mode {
    CUSTOMIZED = 1,
    PV_PRIORITY = 2,
    STANDARD = 3,
    TIME_CONTROl = 4  
};

enum auto_sleep_mode {
  THIRTY_SECONDS = 2,
  ONE_MINNUTE = 3,
  FIVE_MINUTES = 4,
  NEVER = 5  
};
*/

//Enum types will be decoded to string with the following f_enum
//f_enum = 1
enum led_mode_enum{
   LED_LOW = 1,
   LED_HIGH = 2,
   LED_SOS = 3,
   LED_OFF = 4
};

//f_enum = 2
enum eco_shutdown_enum{
  SHUTDOWN_ONE_HOUR = 1,
  SHUTDOWN_TWO_HOURS = 2,
  SHUTDOWN_THREE_HOURS = 3,
  SHUTDOWN_FOUR_HOURS = 4
};

//f_enum = 3
enum charging_mode_enum{
   STANDARD_CHARGING = 0,
   SILENT_CHARGING = 1,
   TURBO_CHARGING = 2
};

// { FIELD_NAME, PAGE, OFFSET, SIZE, SCALE (if scale is needed e.g. decimal value, defaults to 0) , ENUM (if data is enum, defaults to 0) , FIELD_TYPE, STRING_VALUE }
static device_field_data_t bluetti_device_state[] = {
  /*Page 0x00 Core */
  {DEVICE_TYPE,           0x00, 0x0A, 7, 0, 0, STRING_FIELD,""},
  {SERIAL_NUMBER,         0x00, 0x11, 4, 0 ,0, SN_FIELD,""},
  {ARM_VERSION,           0x00, 0x17, 2, 0, 0, VERSION_FIELD,""},
  {DSP_VERSION,           0x00, 0x19, 2, 0, 0, VERSION_FIELD,""},
  {DC_INPUT_POWER,        0x00, 0x24, 1, 0, 0, UINT_FIELD,""},
  {AC_INPUT_POWER,        0x00, 0x25, 1, 0, 0, UINT_FIELD,""},
  {AC_OUTPUT_POWER,       0x00, 0x26, 1, 0, 0, UINT_FIELD,""},
  {DC_OUTPUT_POWER,       0x00, 0x27, 1, 0, 0, UINT_FIELD,""},
  {TOTAL_BATTERY_PERCENT, 0x00, 0x2B, 1, 0, 0, UINT_FIELD,""},
  {POWER_GENERATION,      0x00, 0x29, 1, 1, 0, DECIMAL_FIELD,""},
  {AC_OUTPUT_ON,          0x00, 0x30, 1, 0, 0, BOOL_FIELD,""},
  {DC_OUTPUT_ON,          0x00, 0x31, 1, 0, 0, BOOL_FIELD,""},

  /*Page 0x00 Details */ 
  {INTERNAL_AC_VOLTAGE,   0x00, 0x4D, 1, 1, 0, DECIMAL_FIELD},
  {INTERNAL_CURRENT_ONE,  0x00, 0x56, 2, 1, 0, DECIMAL_FIELD},

  /*Page 0x00 Battery Details*/
  {PACK_NUM_MAX,          0x00, 0x5B, 1, 0, 0, UINT_FIELD }
  
};

static device_field_data_t bluetti_device_command[] = {
  /*Page 0x0B Controls */
  {AC_OUTPUT_ON,      0x0B, 0xBF, 1, 0, 0, BOOL_FIELD}, 
  {DC_OUTPUT_ON,      0x0B, 0xC0, 1, 0, 0, BOOL_FIELD},
  {LED_MODE,          0x0B, 0xDA, 1, 0, 1, ENUM_FIELD},
  {POWER_OFF,         0x0B, 0xF4, 1, 0, 0, BOOL_FIELD},
  {ECO_ON,            0x0B, 0xF8, 1, 0, 2, ENUM_FIELD},
  {CHARGING_MODE,     0x0B, 0xF9, 1, 0, 3, ENUM_FIELD},
  {POWER_LIFTING_ON,  0x0B, 0xFA, 1, 0, 0, BOOL_FIELD}
};

static device_field_data_t bluetti_polling_command[] = {
  {FIELD_UNDEFINED, 0x00, 0x0A, 0x28 ,0 , 0, TYPE_UNDEFINED},
  {FIELD_UNDEFINED, 0x00, 0x46, 0x15 ,0 , 0, TYPE_UNDEFINED},
  {FIELD_UNDEFINED, 0x0B, 0xB9, 0x3D ,0 , 0, TYPE_UNDEFINED},
  {FIELD_UNDEFINED, 0x0B, 0xF4, 0x07 ,0 , 0, TYPE_UNDEFINED}
};

static device_field_data_t bluetti_logging_command[] = {
  {FIELD_UNDEFINED, 0x00, 0x0A, 0x35 ,0 , 0, TYPE_UNDEFINED},
  {FIELD_UNDEFINED, 0x00, 0x46, 0x42 ,0 , 0, TYPE_UNDEFINED},
  {FIELD_UNDEFINED, 0x00, 0x88, 0x4A ,0 , 0, TYPE_UNDEFINED},
  {FIELD_UNDEFINED, 0x0B, 0xB8, 0x43 ,0 , 0, TYPE_UNDEFINED}
};

#endif
