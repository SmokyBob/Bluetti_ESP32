#include "BluettiConfig.h"
#include "PayloadParser.h"
#include "IFTTT.h"
#include "utils.h"

uint16_t parse_uint_field(uint8_t data[])
{
  return ((uint16_t)data[0] << 8) | (uint16_t)data[1];
}

bool parse_bool_field(uint8_t data[])
{
  return (data[1]) == 1;
}

float parse_decimal_field(uint8_t data[], uint8_t scale)
{
  uint16_t raw_value = ((uint16_t)data[0] << 8) | (uint16_t)data[1];
  return (raw_value) / pow(10, scale);
}

float parse_version_field(uint8_t data[])
{

  uint16_t low = ((uint16_t)data[0] << 8) | (uint16_t)data[1];
  uint16_t high = ((uint16_t)data[2] << 8) | (uint16_t)data[3];
  long val = (low) | (high << 16);

  return (float)val / 100;
}

uint64_t parse_serial_field(uint8_t data[])
{

  uint16_t val1 = ((uint16_t)data[0] << 8) | (uint16_t)data[1];
  uint16_t val2 = ((uint16_t)data[2] << 8) | (uint16_t)data[3];
  uint16_t val3 = ((uint16_t)data[4] << 8) | (uint16_t)data[5];
  uint16_t val4 = ((uint16_t)data[6] << 8) | (uint16_t)data[7];

  uint64_t sn = ((((uint64_t)val1) | ((uint64_t)val2 << 16)) | ((uint64_t)val3 << 32)) | ((uint64_t)val4 << 48);

  return sn;
}

String parse_string_field(uint8_t data[])
{
  return String((char *)data);
}

String parse_enum_field(uint8_t data[], int8_t f_enum)
{
  String toRet = "";
  switch (f_enum)
  {
  case 1: // led_mode_enum
    switch (parse_uint_field(data))
    {
    case LED_LOW:
      toRet = "LED_LOW";
      break;
    case LED_HIGH:
      toRet = "LED_HIGH";
      break;
    case LED_SOS:
      toRet = "LED_SOS";
      break;
    case LED_OFF:
      toRet = "LED_OFF";
      break;

    default:
      break;
    }
    break;

  case 2: // eco_shutdown_enum
    switch (parse_uint_field(data))
    {
    case SHUTDOWN_ONE_HOUR:
      toRet = "SHUTDOWN_ONE_HOUR";
      break;
    case SHUTDOWN_TWO_HOURS:
      toRet = "SHUTDOWN_TWO_HOURS";
      break;
    case SHUTDOWN_THREE_HOURS:
      toRet = "SHUTDOWN_THREE_HOURS";
      break;
    case SHUTDOWN_FOUR_HOURS:
      toRet = "SHUTDOWN_FOUR_HOURS";
      break;

    default:
      break;
    }
    break;

  case 3: // charging_mode_enum
    switch (parse_uint_field(data))
    {
    case STANDARD_CHARGING:
      toRet = "STANDARD_CHARGING";
      break;
    case SILENT_CHARGING:
      toRet = "SILENT_CHARGING";
      break;
    case TURBO_CHARGING:
      toRet = "TURBO_CHARGING";
      break;

    default:
      break;
    }
    break;

  default:
    break;
  }

  return toRet;
}

uint16_t curr_TOTAL_BATTERY_PERCENT = 0;
uint16_t curr_AC_INPUT_POWER = 0;

void parse_bluetooth_data(uint8_t page, uint8_t offset, uint8_t *pData, size_t length)
{
  device_field_data_t return_data[sizeof(bluetti_device_state) / sizeof(device_field_data_t)];
  // Array Copy
  copyArray(bluetti_device_state, return_data, (sizeof(bluetti_device_state) / sizeof(device_field_data_t)));

  switch (pData[1])
  {
  // range request
  // TODO: manage other device params / states
  // device_state
  case 0x03:

    for (int i = 0; i < sizeof(bluetti_device_state) / sizeof(device_field_data_t); i++)
    {
      // filter fields not in range
      if (return_data[i].f_page == page &&
          return_data[i].f_offset >= offset &&
          return_data[i].f_offset <= (offset + length) / 2 &&
          return_data[i].f_offset + return_data[i].f_size - 1 >= offset &&
          return_data[i].f_offset + return_data[i].f_size - 1 <= (offset + length) / 2)
      {

        uint8_t data_start = (2 * ((int)return_data[i].f_offset - (int)offset)) + HEADER_SIZE;
        uint8_t data_end = (data_start + 2 * return_data[i].f_size);
        uint8_t data_payload_field[data_end - data_start];

        int p_index = 0;
        for (int i = data_start; i <= data_end; i++)
        {
          data_payload_field[p_index] = pData[i - 1];
          p_index++;
        }

        switch (return_data[i].f_type)
        {

        case UINT_FIELD:

          return_data[i].f_value = String(parse_uint_field(data_payload_field));
          break;

        case BOOL_FIELD:
          return_data[i].f_value = String((int)parse_bool_field(data_payload_field));
          break;

        case DECIMAL_FIELD:
          return_data[i].f_value = String(parse_decimal_field(data_payload_field, return_data[i].f_scale), 2);
          break;

        case SN_FIELD:
          char sn[16];
          sprintf(sn, "%lld", parse_serial_field(data_payload_field));
          return_data[i].f_value = String(sn);
          break;

        case VERSION_FIELD:
          return_data[i].f_value = String(parse_version_field(data_payload_field), 2);
          break;

        case STRING_FIELD:
          return_data[i].f_value = parse_string_field(data_payload_field);
          break;

        case ENUM_FIELD:
          return_data[i].f_value = parse_enum_field(data_payload_field, return_data[i].f_enum);
          break;
        default:
          break;
        }

#if DEBUG <= 4
        Serial.println(map_field_name(return_data[i].f_name).c_str());
        Serial.println(return_data[i].f_value);
        Serial.println("----------------------");
#endif
      }
    }

    break;
  }
  if (pData[1] == 0x03)
  {
// New data received
#if DEBUG <= 4
    Serial.println("New data received");
#endif

    curr_TOTAL_BATTERY_PERCENT = return_data[TOTAL_BATTERY_PERCENT].f_value.toInt();
    curr_AC_INPUT_POWER = return_data[AC_INPUT_POWER].f_value.toInt();

    // Queue data only if useful data is received
    if (!(curr_TOTAL_BATTERY_PERCENT == 0 && curr_AC_INPUT_POWER == 0))
    {
#if DEBUG <= 4
      Serial.println("TOTAL_BATTERY_PERCENT: " + return_data[TOTAL_BATTERY_PERCENT].f_value);
      Serial.println("AC_INPUT_POWER: " + return_data[AC_INPUT_POWER].f_value);
#endif
#if DEBUG <= 5
      writeLog("Bluetti data received");
#endif
      // Queue used to exchange data with the WebServer, but data is fetched only when the page is reloaded
      xQueueReset(bluetti_data_queue); // free up the queue
      if (xQueueSend(bluetti_data_queue, &return_data, 1000) == pdTRUE)
      {
#if DEBUG <= 5
        Serial.println("bluetti_data saved in queue ");
#endif
        String jsonString; // N.B. in this case is faster and simpler than using ArduinoJSON
        jsonString = "{\"timestamp\":\"" + getLocalTimeISO() + "\",";
        for (int i = 0; i < sizeof(return_data) / sizeof(device_field_data_t); i++)
        {
          jsonString = jsonString + "\"" + map_field_name(return_data[i].f_name).c_str() + "\": " +
                       " \"" + return_data[i].f_value + "\",";
        }
        // Remove last ","
        jsonString = jsonString.substring(0, jsonString.length() - 1);
        jsonString = jsonString + "}";

        // Save data to file for later use
        saveBTData(jsonString);
      }

      if (wifiConfig.IFTT_low_bl != 0)
      {
        // Low Battery Notification (not charging)
        if (curr_TOTAL_BATTERY_PERCENT <= wifiConfig.IFTT_low_bl && curr_AC_INPUT_POWER <= 1)
        {
          makeIFTTTRequest("low");
        }
      }

      if (wifiConfig.IFTT_high_bl != 0)
      {
        // Battery Charged (and charging)
        if (curr_TOTAL_BATTERY_PERCENT >= wifiConfig.IFTT_high_bl && curr_AC_INPUT_POWER > 0)
        {
          makeIFTTTRequest("high");
        }
      }
    }
  }

#if DEBUG <= 4
  Serial.print("parse_bluetooth_data() running on core ");
  Serial.println(xPortGetCoreID());
  Serial.println("parse_bluetooth_data() pData[1] =" + String(pData[1]));
#endif
}
