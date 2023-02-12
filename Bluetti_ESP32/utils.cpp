#include "utils.h"
#include "crc16.h"

uint16_t swap_bytes(uint16_t number)
{
  return (number << 8) | (number >> 8);
}

uint16_t modbus_crc(uint8_t buf[], int len)
{
  unsigned int crc = 0xFFFF;
  for (unsigned int i = 0; i < len; i++)
  {
    crc = crc16_update(crc, buf[i]);
  }

  return crc;
}

wrapper slice(const uint8_t *arr, int size, uint8_t include, uint8_t exclude)
{
  wrapper result = {.myarr = {0}, .mysize = 0};
  if (include >= 0 && exclude <= size)
  {
    int count = 0;
    for (int i = include; i < exclude; i++)
    {
      result.myarr[count] = arr[i];
      count++;
    }
    result.mysize = exclude - include;
    return result;
  }
  else
  {
    printf("Array index out-of-bounds\n");
    result.mysize = -1;
    return result;
  }
}

String map_field_name(enum field_names f_name)
{
  switch (f_name)
  {
  case DC_OUTPUT_POWER:
    return "dc_output_power";
    break;
  case AC_OUTPUT_POWER:
    return "ac_output_power";
    break;
  case DC_OUTPUT_ON:
    return "dc_output_on";
    break;
  case AC_OUTPUT_ON:
    return "ac_output_on";
    break;
  case POWER_GENERATION:
    return "power_generation";
    break;
  case TOTAL_BATTERY_PERCENT:
    return "total_battery_percent";
    break;
  case DC_INPUT_POWER:
    return "dc_input_power";
    break;
  case AC_INPUT_POWER:
    return "ac_input_power";
    break;
  case PACK_VOLTAGE:
    return "pack_voltage";
    break;
  case SERIAL_NUMBER:
    return "serial_number";
    break;
  case ARM_VERSION:
    return "arm_version";
    break;
  case DSP_VERSION:
    return "dsp_version";
    break;
  case DEVICE_TYPE:
    return "device_type";
    break;
  case UPS_MODE:
    return "ups_mode";
    break;
  case AUTO_SLEEP_MODE:
    return "auto_sleep_mode";
    break;
  case GRID_CHANGE_ON:
    return "grid_change_on";
    break;
  case INTERNAL_AC_VOLTAGE:
    return "internal_ac_voltage";
    break;
  case INTERNAL_CURRENT_ONE:
    return "internal_current_one";
    break;
  case PACK_NUM_MAX:
    return "pack_max_num";
    break;
  default:
    return "unknown";
    break;
  }
}

//There is no reflection to do string to enum
//There are a couple of ways to work aroung it... but basically are just "case" statements
//Wapped them in a fuction
String map_command_value(String command_name, String value){
  String toRet = value;
  value.toUpperCase();
  command_name.toUpperCase(); //force case indipendence

  //on / off commands
  if(command_name == "POWER_OFF" || command_name == "AC_OUTPUT_ON" || command_name == "DC_OUTPUT_ON" || command_name == "ECO_ON" || command_name == "POWER_LIFTING_ON") {
    if (value == "ON") {
      toRet = "1";
    }
    if (value == "OFF") {
      toRet = "0";
    }
  }

  //See DEVICE_EB3A enums
  if(command_name == "LED_MODE"){
    if (value == "LED_LOW") {
      toRet = "1";
    }
    if (value == "LED_HIGH") {
      toRet = "2";
    }
    if (value == "LED_SOS") {
      toRet = "3";
    }
    if (value == "LED_OFF") {
      toRet = "4";
    }
  }

  //See DEVICE_EB3A enums
  if(command_name == "ECO_SHUTDOWN"){
    if (value == "ONE_HOUR") {
      toRet = "1";
    }
    if (value == "TWO_HOURS") {
      toRet = "2";
    }
    if (value == "THREE_HOURS") {
      toRet = "3";
    }
    if (value == "FOUR_HOURS") {
      toRet = "4";
    }
  }

  //See DEVICE_EB3A enums
  if(command_name == "CHARGING_MODE"){
    if (value == "STANDARD") {
      toRet = "0";
    }
    if (value == "SILENT") {
      toRet = "1";
    }
    if (value == "TURBO") {
      toRet = "2";
    }
  }


  return toRet;
}

String convertMilliSecondsToHHMMSS(unsigned long value)
{

  unsigned long valSec = value / 1000;
  // compute h, m, s
  unsigned long h = (valSec / 3600);
  unsigned long m = ((valSec % 3600) / 60);
  unsigned long s = (valSec % 60);
  // add leading zero if needed
  String hh = (h < 10) ? ("0" + String(h)) : String(h);
  String mm = (m < 10) ? ("0" + String(m)) : String(m);
  String ss = (s < 10) ? ("0" + String(s)) : String(s);

  return hh + ':' + mm + ":" + ss;
}

void copyArray(device_field_data_t *src, device_field_data_t *dst, int len)
{
  for (int i = 0; i < len; i++)
  {
    *dst++ = *src++;
  }
}

String getLocalTimeISO()
{
  // Get Time to write before the log
  // Full param list
  // https://cplusplus.com/reference/ctime/strftime/

  char buffer[80];
  time_t tt = time(0);
  strftime(buffer, 80, "%F %T", localtime(&tt));

  return String(buffer);
}

String htmlEntities(String str, bool whitespace)
{
  str.replace("&", "&amp;");
  str.replace("<", "&lt;");
  str.replace(">", "&gt;");
  str.replace("'", "&#39;");
  if (whitespace)
    str.replace(" ", "&#160;");
  // str.replace("-","&ndash;");
  // str.replace("\"","&quot;");
  // str.replace("/": "&#x2F;");
  // str.replace("`": "&#x60;");
  // str.replace("=": "&#x3D;");
  return str;
}