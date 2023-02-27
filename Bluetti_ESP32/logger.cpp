#include "logger.h"

static char log_print_buffer[512];
static char log_bt_data[512];

int vprintf_into_spiffs(const char *szFormat, va_list args)
{

  // write evaluated format string into buffer
  int ret = vsnprintf(log_print_buffer, sizeof(log_print_buffer), szFormat, args);
  if (wifiConfig.useDbgFilelog)
  {
    // output is now in buffer. write to file.
    if (ret >= 0)
    {
      File spiffsLogFile = SPIFFS.open(F("/debug_log.txt"), FILE_APPEND);
      // debug output
      // printf("[Writing to SPIFFS] %.*s", ret, log_print_buffer);
      spiffsLogFile.write((uint8_t *)log_print_buffer, (size_t)ret);
      // to be safe in case of crashes: flush the output
      spiffsLogFile.flush();
      spiffsLogFile.close();
    }
  }
  return ret;
}

void initLog()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println(F("An Error has occurred while mounting SPIFFS"));
    return;
  }

  // install new logging function
  esp_log_set_vprintf(&vprintf_into_spiffs);

  // trigger some text
  esp_log_level_set("DBG", ESP_LOG_DEBUG);
}

void clearLog()
{
  // Remove the log file so that a new one will be creates the next time a log is stored
  SPIFFS.remove(F("/debug_log.txt"));
}

void clearSpiffLog(){
  SPIFFS.remove(F("/debug_log.txt"));
  SPIFFS.remove(F("/bluetti_data.json"));
}

void writeLog(String message)
{
  esp_log_write(ESP_LOG_DEBUG, "DBG", (getLocalTimeISO() + " " + message + "\n").c_str());
}

void saveBTData(String message)
{
  if (wifiConfig._useBTFilelog)
  {
    message = message + "\n"; // Add new line each file save
    va_list args;
    // write evaluated format string into buffer
    int ret = vsnprintf(log_bt_data, sizeof(log_bt_data), message.c_str(), args);

    // output is now in buffer. write to file.
    if (ret >= 0)
    {
      File spiffsBTDataFile = SPIFFS.open(F("/bluetti_data.json"), FILE_APPEND);
      // debug output
      // printf("[Writing to SPIFFS] %.*s", ret, log_bt_data);
      spiffsBTDataFile.write((uint8_t *)log_bt_data, (size_t)ret);
      // to be safe in case of crashes: flush the output
      spiffsBTDataFile.flush();
      spiffsBTDataFile.close();
    }
  }
}

void clearBtData()
{
  // Remove the log file so that a new one will be creates the next time a log is stored
  SPIFFS.remove(F("/bluetti_data.json"));
}