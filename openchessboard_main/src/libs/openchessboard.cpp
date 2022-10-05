#include <Arduino.h>
#include "openchessboard.h"
#include "ArduinoBleChess.h"

/* BOARD SETUP/CONFIGURATION */
#define LED_BRIGHTNESS 100 // LED brightess in percent
#define SENSE_THRS 200 // Reduce if pieces are not recognized, increase if Board starts blinking randomly

#define LED_DIM_VAL (255 - LED_BRIGHTNESS/100 * 255)  

bool connect_flipstate = true;

//define if board orientation has power plug at top; if not defined board is oriented with plug at right side
//#define PLUG_AT_TOP 

/* HW GPIO configuration */
#define LED_MR_N_PIN  4 // RESET, D4
#define LED_CLOCK_PIN 6 //SHCP, D5
#define LED_LATCH_PIN 5 //STCP, D6 
#define LED_OE_N_PIN 3 // D3 
#define LED_PIN 2 //D2

#define HALL_OUT_S0 10 //D10
#define HALL_OUT_S1  9 //D9
#define HALL_OUT_S2 8 //D8

#define HALL_ROW_S0 A7  //A7/D21
#define HALL_ROW_S1 A6  //A6/D20
#define HALL_ROW_S2 A5  //A5/D19

#define HALL_SENSE A3  //A3

/* ---------------------------------------
 *  Function to initiate GPIOs.
 *  Defines GPIOs input and output states. 
 *  Depends on Arduino HW (adapt HW GPIO configuration to match Arduino Board)
 *  @params[in] void
 *  @return void
*/   
void initHW(void) {
  pinMode(LED_MR_N_PIN, OUTPUT);
  pinMode(LED_OE_N_PIN, OUTPUT);
  digitalWrite(LED_MR_N_PIN, 0);
  digitalWrite(LED_OE_N_PIN, 1);
  
  pinMode(LED_CLOCK_PIN, OUTPUT);
  pinMode(LED_LATCH_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  pinMode(HALL_OUT_S0, OUTPUT);
  pinMode(HALL_OUT_S1, OUTPUT);
  pinMode(HALL_OUT_S2, OUTPUT);

  pinMode(HALL_ROW_S0, OUTPUT);
  pinMode(HALL_ROW_S1, OUTPUT);
  pinMode(HALL_ROW_S2, OUTPUT);

  pinMode(HALL_SENSE, INPUT);

}

/* ---------------------------------------
 *  Function to write ledBoardState to LED shift registers.
 *  Activates LEDs immediately.
 *  @params[in] byte array (max size 8 bytes)
 *  @return void
*/   
void shiftOut(byte ledBoardState[]) {

  bool pinStateEN;

  digitalWrite(LED_PIN, 0);
  digitalWrite(LED_MR_N_PIN, 0);
  delay(1);
  digitalWrite(LED_MR_N_PIN, 1);

  for (int i = 0; i < 8; i++)  {
    for (int k = 0; k < 8; k++) {

      digitalWrite(LED_CLOCK_PIN, 0);

      if (ledBoardState[i] & (1 << k)) {
        pinStateEN = 1;
      }
      else {
        pinStateEN = 0;
      }
      
      digitalWrite(LED_PIN, pinStateEN);
      digitalWrite(LED_CLOCK_PIN, 1);
      digitalWrite(LED_PIN, 0);
    }
  }
  digitalWrite(LED_CLOCK_PIN, 0);

}


/* ---------------------------------------
 *  Function to ready Hall sensors states to array.
 *  Multiplexing all sensors. Sets 0 or 1 in array if threshold is exceeded.
 *  @params[in] byte array (max size 8 bytes)
 *  @return void
*/   
void readHall(byte hallBoardState[]) {

  int hall_val = 0;

  for (int k = 0; k < 8; k++) {
    hallBoardState[k] = 0x00;
  }

  for (int row_index = 0; row_index < 8; row_index++)
  {
    bool bit0 = ((byte)row_index & (1 << 0)) != 0;
    bool bit1 = ((byte)row_index & (1 << 1)) != 0;
    bool bit2 = ((byte)row_index & (1 << 2)) != 0;
    digitalWrite(HALL_ROW_S0, bit0);
    digitalWrite(HALL_ROW_S1, bit1);
    digitalWrite(HALL_ROW_S2, bit2);

    for (int col_index = 0; col_index < 8; col_index++) {
        bool bit0 = ((byte)col_index & (1 << 0)) != 0;
        bool bit1 = ((byte)col_index & (1 << 1)) != 0;
        bool bit2 = ((byte)col_index & (1 << 2)) != 0;
        digitalWrite(HALL_OUT_S0, bit0);
        digitalWrite(HALL_OUT_S1, bit1);
        digitalWrite(HALL_OUT_S2, bit2);

      //delayMicroseconds(10);
      hall_val = analogRead(HALL_SENSE);
      //delayMicroseconds(100);


      if (hall_val < SENSE_THRS) {
        hallBoardState[row_index] |= 1UL << (col_index);
      }
    }

    }

}


/* ---------------------------------------
 *  Function that waits for a move input.
 *  Waits for a move input (blocking, but can be exited by isr if game is set to be not running) 
 *  and returns move string.
 *  Example move: e2e4(piece moves from e2 to e4)
 *  @params[in] void
 *  @return String mvInput
*/  
String getMoveInput(void) {
  const char columns[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};

  String mvInput;

  byte hallBoardStateInit[8];
  byte hallBoardState1[8];
  byte hallBoardState2[8];
  byte hallBoardState3[8];
  byte ledBoardState[8];

  for (int k = 0; k < 8; k++) {
    hallBoardStateInit[k] = 0x00;
    hallBoardState1[k] = 0x00;
    hallBoardState2[k] = 0x00;
    hallBoardState3[k] = 0x00;
    ledBoardState[k] = 0x00;
  }

  bool mvStarted = false;
  bool mvFinished = false;


  // get inital position
  readHall(hallBoardStateInit);

// wait for Start move event
  while (!mvStarted) {
    BLE.poll();
    readHall(hallBoardState1);

    for (int row_index = 0; row_index < 8; row_index++) {
      for (int col_index = 0; col_index < 8; col_index++) {

        int state1 = bitRead(hallBoardStateInit[row_index], col_index);
        int state2 = bitRead(hallBoardState1[row_index], col_index);
        if (state1  != state2) {
          ledBoardState[7 - row_index] |= 1UL << (7 - col_index);
          #ifdef PLUG_AT_TOP
          mvInput = mvInput + (String)columns[7 - col_index] + (String)(7 - row_index + 1);
          #else
          mvInput = mvInput + (String)columns[7-row_index] + (String)(col_index + 1);
          #endif
          mvStarted = true;
          break;
        }
      }
    }
  }

  digitalWrite(LED_LATCH_PIN, 0);
  shiftOut(ledBoardState);
  digitalWrite(LED_LATCH_PIN, 1);
  //digitalWrite(LED_OE_N_PIN , 0);
  analogWrite(LED_OE_N_PIN , LED_DIM_VAL);

// wait for end move event
  while (!mvFinished ) {
    readHall(hallBoardState2);
    delay(100);
    readHall(hallBoardState3);
    delay(100);

    for (int row_index = 0; row_index < 8; row_index++) {
      for (int col_index = 0; col_index < 8; col_index++) {

        int state_prev = bitRead(hallBoardState1[row_index], col_index);

        int hallBoardState1 = bitRead(hallBoardState2[row_index], col_index);
        int hallBoardState2 = bitRead(hallBoardState3[row_index], col_index);

        if ((hallBoardState1 != state_prev) && (hallBoardState2 != state_prev)) {
          if (hallBoardState1  == hallBoardState2) {
            mvFinished = true;
            ledBoardState[7 - row_index] |= 1UL << (7 - col_index);

            #ifdef PLUG_AT_TOP
            mvInput = mvInput + (String)columns[7 - col_index] + (String)(7 - row_index + 1);
            #else
            mvInput = mvInput + (String)columns[7-row_index] + (String)(col_index + 1);
            #endif
          }
        }
      }
    }
  }
  digitalWrite(LED_LATCH_PIN, 0);
  shiftOut(ledBoardState);
  digitalWrite(LED_LATCH_PIN, 1);
  //digitalWrite(LED_OE_N_PIN , 0);
  analogWrite(LED_OE_N_PIN , LED_DIM_VAL);
  delay(300);
  
  return mvInput;
  
}


/* ---------------------------------------
 *  Function that clears all LED states.
 * Writes 0 to shift registers for all LEDs.
 *  @params[in] void
 *  @return void
*/  
void clearDisplay(void) {
  byte ledBoardState[8];
  
  for (int k = 0; k < 8; k++) {
    ledBoardState[k] = 0x00;
  }
  digitalWrite(LED_MR_N_PIN, 0);
  digitalWrite(LED_OE_N_PIN , 1);
  digitalWrite(LED_LATCH_PIN, 0);
  shiftOut(ledBoardState);
  digitalWrite(LED_LATCH_PIN, 1);
  //digitalWrite(LED_OE_N_PIN , 1);
  analogWrite(LED_OE_N_PIN , LED_DIM_VAL);
}


/* ---------------------------------------
 *  Function that displays move.
 *  Writes to specific shift registers and show start and end position of piece movement.
 *  @params[in] string move
 *  @return void
*/  
void displayMove(String mv) {

  byte ledBoardState[8] = {0};

  const char columns[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
  const char rows[] = {'1', '2', '3', '4', '5', '6', '7', '8'};

  int row1 = 0;
  int col1 = 0;
  int row2 = 0;
  int col2 = 0;

  for (int k = 0; k < 8; k++)
  {
    if (columns[k] == mv.charAt(0)) {
      col1 = k;
    }
    if (columns[k] == mv.charAt(2)) {
      col2 = k;
    }
  }

  for (int k = 0; k < 8; k++) {
    if (rows[k] == mv.charAt(1)) {
      row1 = k;
    }
    if (rows[k] == mv.charAt(3)) {
      row2 = k;
    }
  }
#ifdef PLUG_AT_TOP
  ledBoardState[row1] |= 1UL << col1;
  ledBoardState[row2] |= 1UL << col2;
#else
  ledBoardState[col1] |= 1UL << 7-row1;
  ledBoardState[col2] |= 1UL << 7-row2;
#endif
  digitalWrite(LED_OE_N_PIN , 1);
  digitalWrite(LED_MR_N_PIN, 0);
  digitalWrite(LED_MR_N_PIN, 1);
  digitalWrite(LED_LATCH_PIN, 0);
  shiftOut(ledBoardState);
  digitalWrite(LED_LATCH_PIN, 1);
  //digitalWrite(LED_OE_N_PIN , 0);
  analogWrite(LED_OE_N_PIN , LED_DIM_VAL);
}


void displayConnectWait(void) {
  byte connect_led_array[8] = {0};

  if (connect_flipstate) {
    connect_led_array[3] = 0x10;
    connect_led_array[4] = 0x08;
  }
  else {
    connect_led_array[4] = 0x10;
    connect_led_array[3] = 0x08;
  }
  digitalWrite(LED_OE_N_PIN , 1);
  digitalWrite(LED_MR_N_PIN, 0);
  digitalWrite(LED_MR_N_PIN, 1);
  digitalWrite(LED_LATCH_PIN, 0);
  shiftOut(connect_led_array);
  digitalWrite(LED_LATCH_PIN, 1);
  //digitalWrite(LED_OE_N_PIN , 0);
  analogWrite(LED_OE_N_PIN , LED_DIM_VAL);
  connect_flipstate = !connect_flipstate;
  delay(300);
}


void displayBtoPlay(void) {
  byte led_array[8] = {0};

  led_array[0] = 0x01;
  led_array[1] = 0x01;
  led_array[2] = 0x01;
  led_array[3] = 0x01;
  led_array[4] = 0x01;
  led_array[5] = 0x01;
  led_array[6] = 0x01;
  led_array[7] = 0x01;


  digitalWrite(LED_OE_N_PIN , 1);
  digitalWrite(LED_MR_N_PIN, 0);
  digitalWrite(LED_MR_N_PIN, 1);
  digitalWrite(LED_LATCH_PIN, 0);
  shiftOut(led_array);
  digitalWrite(LED_LATCH_PIN, 1);
  analogWrite(LED_OE_N_PIN , LED_DIM_VAL);

}

void displayWtoPlay(void) {
  byte led_array[8] = {0};

  led_array[0] = 0x80;
  led_array[1] = 0x80;
  led_array[2] = 0x80;
  led_array[3] = 0x80;
  led_array[4] = 0x80;
  led_array[5] = 0x80;
  led_array[6] = 0x80;
  led_array[7] = 0x80;

  digitalWrite(LED_OE_N_PIN , 1);
  digitalWrite(LED_MR_N_PIN, 0);
  digitalWrite(LED_MR_N_PIN, 1);
  digitalWrite(LED_LATCH_PIN, 0);
  shiftOut(led_array);
  digitalWrite(LED_LATCH_PIN, 1);
  analogWrite(LED_OE_N_PIN , LED_DIM_VAL);
}
