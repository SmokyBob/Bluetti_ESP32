#include "Arduino.h"
#include "config.h"
#ifdef IFTTT
extern void makeIFTTTRequest(String event, uint16_t battPerc);
#endif