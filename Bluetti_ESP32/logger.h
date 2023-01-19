#ifndef LOGGER_H
#define LOGGER_H
#include <Arduino.h>

// #include <stdarg.h>
// #include <stdio.h>
#include <SPIFFS.h>

// Base on https://community.platformio.org/t/is-it-possible-to-output-logs-to-a-spiffs-file-thats-inside-the-esp32/6999/4
// esp_logging docs
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html

extern void initLog();

extern void clearLog();

extern String getLog();

#endif