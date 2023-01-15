#ifndef __DEVICE_TYPE_H__
#define __DEVICE_TYPE_H__
#include "Arduino.h"

enum field_types{
   UINT_FIELD,
   BOOL_FIELD,
   ENUM_FIELD,
   STRING_FIELD,
   DECIMAL_ARRAY_FIELD,
   DECIMAL_FIELD,
   VERSION_FIELD,
   SN_FIELD, 
   TYPE_UNDEFINED
};

//Same Order as the bluetti_device_state array for faster access
enum field_names {
  DEVICE_TYPE, 
  SERIAL_NUMBER,
  ARM_VERSION,
  DSP_VERSION,
  DC_INPUT_POWER,
  AC_INPUT_POWER,
  AC_OUTPUT_POWER,
  DC_OUTPUT_POWER,
  TOTAL_BATTERY_PERCENT, 
  POWER_GENERATION,
  AC_OUTPUT_ON,
  DC_OUTPUT_ON,
  INTERNAL_AC_VOLTAGE,
  INTERNAL_CURRENT_ONE,
  PACK_NUM_MAX,
  LED_MODE,
  POWER_OFF,
  ECO_ON,
  ECO_SHUTDOWN,
  CHARGING_MODE,
  POWER_LIFTING_ON,
  PACK_VOLTAGE,
  UPS_MODE,
  AUTO_SLEEP_MODE,
  GRID_CHANGE_ON,
  FIELD_UNDEFINED
};

typedef struct device_field_data {
  enum field_names f_name;
  uint8_t f_page;
  uint8_t f_offset;
  int8_t f_size;
  int8_t f_scale;
  int8_t f_enum;
  enum field_types f_type;
  String f_value;
} device_field_data_t;


#endif
