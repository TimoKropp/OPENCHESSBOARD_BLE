#include <Arduino.h>
#include "openchessboard.h"

#define HW_DEBUG   // 

#define DEVICE_NAME "OCB" // max name size with 128 bit uuid is 11
#define DEBUG false  // set to true for debug output, false for no debug output
#define DEBUG_SERIAL if(DEBUG)Serial

void setup() {
  initHW();

  DEBUG_SERIAL.begin(115200);
#if DEBUG
  while(!Serial);
#endif
}

void loop() {
  puzzle_test();

}
