#ifndef UTILS_H
#define UTILS_H
#include "Arduino.h"
#include "DeviceType.h"

#define MAX 100

typedef struct{ 
    uint8_t myarr[MAX];
    int mysize;
} wrapper;

extern uint16_t swap_bytes(uint16_t number);
extern wrapper slice(const uint8_t* arr, int size, uint8_t include, uint8_t exclude);
extern uint16_t modbus_crc(uint8_t buf[], int len);
extern String map_field_name(enum field_names f_name);
extern String convertMilliSecondsToHHMMSS (int value);
extern void copyArray(device_field_data_t* src, device_field_data_t* dst, int len);

#endif
