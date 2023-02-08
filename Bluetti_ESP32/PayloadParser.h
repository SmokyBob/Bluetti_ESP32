#ifndef PAYLOAD_PARSER_H
#define PAYLOAD_PARSER_H
#include "Arduino.h"
#include "utils.h"

#define HEADER_SIZE 4
#define CHECKSUM_SIZE 2

extern uint16_t parse_uint_field(uint8_t data[]);
extern bool parse_bool_field(uint8_t data[]);
extern float pase_decimal_field(uint8_t data[], uint8_t scale);
extern uint64_t parse_serial_field(uint8_t data[]);
extern float parse_version_field(uint8_t data[]);
extern String parse_string_field(uint8_t data[]);
extern String parse_enum_field(uint8_t data[]);

extern void parse_bluetooth_data(uint8_t page, uint8_t offset, uint8_t* pData, size_t length);

#endif
