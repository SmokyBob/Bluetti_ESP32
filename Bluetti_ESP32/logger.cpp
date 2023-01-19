#include "logger.h"

static char log_print_buffer[512];

int vprintf_into_spiffs(const char *szFormat, va_list args)
{
  // write evaluated format string into buffer
  int ret = vsnprintf(log_print_buffer, sizeof(log_print_buffer), szFormat, args);

  // output is now in buffer. write to file.
  if (ret >= 0)
  {
    File spiffsLogFile = SPIFFS.open("/debug_log.txt", FILE_APPEND);
    // debug output
    // printf("[Writing to SPIFFS] %.*s", ret, log_print_buffer);
    spiffsLogFile.write((uint8_t *)log_print_buffer, (size_t)ret);
    // to be safe in case of crashes: flush the output
    spiffsLogFile.flush();
    spiffsLogFile.close();
  }
  return ret;
}

void initLog()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
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
  SPIFFS.remove("/debug_log.txt");
}

String getLog()
{
  File spiffsLogFile = SPIFFS.open("/debug_log.txt", FILE_READ);

  if (!spiffsLogFile)
  {
    Serial.println("Failed to open file for reading");
    return "Error Reading log File";
  }

  String toRet = spiffsLogFile.readString();

  spiffsLogFile.close();

  return toRet;
}