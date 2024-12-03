#include <Arduino.h>
#include "openchessboard.h"

/* BOARD SETUP/CONFIGURATION */
#define SENSE_THRS 100 // Increase if pieces are not recognized, decrease if Board starts blinking randomly
bool connect_flipstate = true;

//define if board orientation has power plug at top; if not defined board is oriented with plug at right side
#define PLUG_AT_TOP 
#define HW_DEBUG   // 

#define DEBUG false  // set to true for debug output, false for no debug output
#define DEBUG_SERIAL if(DEBUG)Serial

/* HW GPIO configuration */
#ifdef ARDUINO_ARCH_ESP32
int LED_DATA_PIN = D2; //D2, 5
int LED_OE_N_PIN = D3; // D3, 6 
int LED_MR_N_PIN = D4; //D4, 
int LED_LATCH_PIN = D5; // D5, 8
int LED_CLOCK_PIN = D6; //D6, 9

int HALL_OUT_S0 = D10;  //D10, 17
int HALL_OUT_S1 = D9;  //D09, 18
int HALL_OUT_S2 = D8;  //D08, 21 

int HALL_ROW_S0 = A7; //D24, 14, A7
int HALL_ROW_S1 = A6; //D23, 13, A6
int HALL_ROW_S2 = A5; //D22, 12, A5
int HALL_SENSE = A3;  //A3

#elif ARDUINO_ARCH_SAMD
int LED_MR_N_PIN = 4; // RESET, D4
int LED_CLOCK_PIN = 6; //SHCP, D5
int LED_LATCH_PIN = 5; //STCP, D6 
int LED_OE_N_PIN = 3; // D3 
int LED_DATA_PIN = 2; //D2

int HALL_OUT_S0 = 10; //D10
int HALL_OUT_S1 = 9; //D9
int HALL_OUT_S2 = 8; //D8

int HALL_ROW_S0 = A7;  //A7/D21
int HALL_ROW_S1 = A6;  //A6/D20
int HALL_ROW_S2 = A5;  //A5/D19

int HALL_SENSE = A3;  //A3
#endif

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
  pinMode(LED_DATA_PIN, OUTPUT);

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

  digitalWrite(LED_DATA_PIN, 0);
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
      
      digitalWrite(LED_DATA_PIN, pinStateEN);
      digitalWrite(LED_CLOCK_PIN, 1);
      digitalWrite(LED_DATA_PIN, 0);
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
    delayMicroseconds(500);
    for (int col_index = 0; col_index < 8; col_index++) {
        bool bit0 = ((byte)col_index & (1 << 0)) != 0;
        bool bit1 = ((byte)col_index & (1 << 1)) != 0;
        bool bit2 = ((byte)col_index & (1 << 2)) != 0;
        digitalWrite(HALL_OUT_S0, bit0);
        digitalWrite(HALL_OUT_S1, bit1);
        digitalWrite(HALL_OUT_S2, bit2);

      delayMicroseconds(300);
      hall_val = analogRead(HALL_SENSE);
      delayMicroseconds(300);


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
  digitalWrite(LED_OE_N_PIN , 0);


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
  digitalWrite(LED_OE_N_PIN , 0);
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
  digitalWrite(LED_OE_N_PIN , 1);
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
  ledBoardState[col1] |= 1UL << (7-row1);
  ledBoardState[col2] |= 1UL << (7-row2);
#endif
  digitalWrite(LED_OE_N_PIN , 1);
  digitalWrite(LED_MR_N_PIN, 0);
  digitalWrite(LED_MR_N_PIN, 1);
  digitalWrite(LED_LATCH_PIN, 0);
  shiftOut(ledBoardState);
  digitalWrite(LED_LATCH_PIN, 1);
  digitalWrite(LED_OE_N_PIN , 0);


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
  digitalWrite(LED_OE_N_PIN , 0);
  connect_flipstate = !connect_flipstate;
  delay(300);
}

void displayFrame(byte frame[8]) {
  digitalWrite(LED_OE_N_PIN , 1);
  digitalWrite(LED_MR_N_PIN, 0);
  digitalWrite(LED_MR_N_PIN, 1);
  digitalWrite(LED_LATCH_PIN, 0);

  shiftOut(frame);
  digitalWrite(LED_LATCH_PIN, 1);
    //digitalWrite(LED_OE_N_PIN , 0);
  analogWrite(LED_OE_N_PIN , 150);
  delay(100);
  }

void displayNewGame(void) {
  byte step1[8] = {0b00000000, 
                   0b00000000, 
                   0b00000000, 
                   0b00011000, 
                   0b00011000, 
                   0b00000000, 
                   0b00000000, 
                   0b00000000};

  byte step2[8] = {0b00000000, 
                   0b00000000, 
                   0b00000000, 
                   0b00000000, 
                   0b00000000, 
                   0b00000000, 
                   0b00000000, 
                   0b00000000};

  displayFrame(step1);
  delay(80);
  displayFrame(step2);
  delay(80);
  displayFrame(step1);
  delay(80);
  displayFrame(step2);
  delay(80);
  clearDisplay();

}

void displayWaitForGame(void) {
  byte step1[8] = {0b00000000, 
                   0b00000000, 
                   0b00000000, 
                   0b00010000, 
                   0b00011000, 
                   0b00000000, 
                   0b00000000, 
                   0b00000000};

  byte step2[8] = {0b00000000, 
                   0b00000000, 
                   0b00000000, 
                   0b00011000, 
                   0b00010000, 
                   0b00000000, 
                   0b00000000, 
                   0b00000000};

  byte step3[8] = {0b00000000, 
                   0b00000000, 
                   0b00000000, 
                   0b00011000, 
                   0b00001000, 
                   0b00000000, 
                   0b00000000, 
                   0b00000000};

  byte step4[8] = {0b00000000, 
                   0b00000000, 
                   0b00000000, 
                   0b00001000, 
                   0b00011000, 
                   0b00000000, 
                   0b00000000, 
                   0b00000000};
          
  displayFrame(step4);
  delay(80);
  displayFrame(step3);
  delay(80);
  displayFrame(step2);
  delay(80);
  displayFrame(step1);
  delay(80);
  clearDisplay();
}

byte swapBits(byte b) {
    byte swapped = 0;
    for (int i = 0; i < 8; i++) {
        swapped |= ((b >> i) & 0x01) << (7 - i);
    }
    return swapped;
}

void hw_test(void){
    byte hallBoardStateInit[8];
  for (int k = 0; k < 8; k++) {
    hallBoardStateInit[k] = 0x00;
  }
  readHall(hallBoardStateInit);
  
  for (int i = 0; i < 4; i++) {
      // Swap each byte with its counterpart on the opposite side
      byte temp = hallBoardStateInit[i];
      hallBoardStateInit[i] = swapBits(hallBoardStateInit[7 - i]);
      hallBoardStateInit[7 - i] = swapBits(temp);
  }

  for (int i = 0; i < 8; i++) {
      for (int j = 7; j >= 0; j--) {
          // Extract each bit from the byte and print it

          DEBUG_SERIAL.print((hallBoardStateInit[i] >> j) & 0x01);
          DEBUG_SERIAL.print(" ");
      }
      DEBUG_SERIAL.println();  // Move to the next line after each byte
  }
  DEBUG_SERIAL.println(); 


  displayFrame(hallBoardStateInit);

}

void highlightAttackedSquares(byte hallBoardState[], byte frame[]) {
  // Initialize the frame with no attacked squares
  for (int i = 0; i < 8; i++) {
    frame[i] = 0x00;  // Clear all positions in the frame
  }

  // Loop through the hallBoardState to find the queens' positions
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (hallBoardState[row] & (1 << col)) {  // Check if there's a queen in this position
        // Mark all squares in the same row
        frame[row] = 0xFF;  // All squares in the row are attacked

        // Mark all squares in the same column
        for (int r = 0; r < 8; r++) {
          frame[r] |= (1 << col);  // Set the bit for this column in all rows
        }

        // Mark diagonals
        // Main diagonal (top-left to bottom-right)
        for (int i = 1; i < 8; i++) {
          if (row + i < 8 && col + i < 8) {
            frame[row + i] |= (1 << (col + i));  // Bottom-right diagonal
          }
          if (row - i >= 0 && col - i >= 0) {
            frame[row - i] |= (1 << (col - i));  // Top-left diagonal
          }
        }

        // Anti-diagonal (top-right to bottom-left)
        for (int i = 1; i < 8; i++) {
          if (row + i < 8 && col - i >= 0) {
            frame[row + i] |= (1 << (col - i));  // Bottom-left diagonal
          }
          if (row - i >= 0 && col + i < 8) {
            frame[row - i] |= (1 << (col + i));  // Top-right diagonal
          }
        }
      }
    }
  }
  for (int i = 0; i < 4; i++) {
    // Swap each byte with its counterpart on the opposite side
    byte temp = frame[i];
    frame[i] = swapBits(frame[7 - i]);
    frame[7 - i] = swapBits(temp);
  }
}

bool isFrameEmpty(byte frame[]) {
  for (int i = 0; i < 8; i++) {
    if (frame[i] != 0) {
      return false;  // Frame is not empty
    }
  }
  return true;  // Frame is empty
}

bool isFrameFull(byte frame[]) {
  for (int i = 0; i < 8; i++) {
    if (frame[i] != 0xFF) {
      return false;  // Frame is not fully occupied
    }
  }
  return true;  // Frame is fully occupied
}

// Function to highlight the connection between two pieces
void highlightConnection(byte frame[], int startRow, int startCol, int endRow, int endCol) {
    int rowStep = (endRow > startRow) ? 1 : (endRow < startRow) ? -1 : 0;
    int colStep = (endCol > startCol) ? 1 : (endCol < startCol) ? -1 : 0;

    int row = startRow;
    int col = startCol;

    // Highlight the squares along the path
    while (row != endRow || col != endCol) {
        frame[row] |= (1 << col);  // Highlight the current square

        // Move towards the target position
        if (row != endRow) row += rowStep;
        if (col != endCol) col += colStep;
    }

    // Highlight the end square (the piece being attacked)
    frame[endRow] |= (1 << endCol);
}

void highlightAttackedPieces(byte hallBoardState[], byte frame[]) {
    // Initialize the frame with no highlighted pieces
    for (int i = 0; i < 8; i++) {
        frame[i] = 0x00;  // Clear all positions in the frame
    }

    // Find all queens' positions
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (hallBoardState[row] & (1 << col)) {  // There's a queen at (row, col)

                // Check for other queens in the same row
                for (int c = 0; c < 8; c++) {
                    if (c != col && (hallBoardState[row] & (1 << c))) {  // Another queen in the same row
                        highlightConnection(frame, row, col, row, c);
                    }
                }

                // Check for other queens in the same column
                for (int r = 0; r < 8; r++) {
                    if (r != row && (hallBoardState[r] & (1 << col))) {  // Another queen in the same column
                        highlightConnection(frame, row, col, r, col);
                    }
                }

                // Check for other queens in diagonals (main diagonal and anti-diagonal)
                for (int i = 1; i < 8; i++) {
                    // Main diagonal (top-left to bottom-right)
                    if (row + i < 8 && col + i < 8 && (hallBoardState[row + i] & (1 << (col + i)))) {
                        highlightConnection(frame, row, col, row + i, col + i);
                    }
                    if (row - i >= 0 && col - i >= 0 && (hallBoardState[row - i] & (1 << (col - i)))) {
                        highlightConnection(frame, row, col, row - i, col - i);
                    }

                    // Anti-diagonal (top-right to bottom-left)
                    if (row + i < 8 && col - i >= 0 && (hallBoardState[row + i] & (1 << (col - i)))) {
                        highlightConnection(frame, row, col, row + i, col - i);
                    }
                    if (row - i >= 0 && col + i < 8 && (hallBoardState[row - i] & (1 << (col + i)))) {
                        highlightConnection(frame, row, col, row - i, col + i);
                    }
                }
            }
        }
    }

    // Optional: Rotate frame if needed
    for (int i = 0; i < 4; i++) {
        // Swap each byte with its counterpart on the opposite side
        byte temp = frame[i];
        frame[i] = swapBits(frame[7 - i]);
        frame[7 - i] = swapBits(temp);
    }
}


void winningTreeAnimation(byte hallBoardState[], byte frame[]) {

  
  for (int i = 0; i < 4; i++) {
      // Swap each byte with its counterpart on the opposite side
      byte temp = hallBoardState[i];
      hallBoardState[i] = swapBits(hallBoardState[7 - i]);
      hallBoardState[7 - i] = swapBits(temp);
  }

  // Step 1: Find all placed pieces
  struct Position {
    int row;
    int col;
  };

  Position pieces[8];  // Assuming at most 8 pieces on the board
  int pieceCount = 0;

  // Find the positions of all the pieces on the board
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (hallBoardState[row] & (1 << col)) {  // There's a piece at (row, col)
        pieces[pieceCount++] = {row, col};     // Store its position
      }
    }
  }

  // Step 2: Start from the first piece as the root
  if (pieceCount == 0) return;  // No pieces to connect

  Position root = pieces[0];  // Root of the tree (start point)
  for (int k = 0; k < 8; k++) {
    frame[k] = 0x00;  // Clear the frame
  }
  // Step 3: Create connections to all other pieces
  for (int i = 1; i < pieceCount; i++) {
    Position current = pieces[i];  // Current piece to connect to the root
    Position start = root;
       // Clear the frame before drawing a new path

    // Draw the path step by step without clearing the frame
    int rowStep = (start.row < current.row) ? 1 : (start.row > current.row) ? -1 : 0;
    int colStep = (start.col < current.col) ? 1 : (start.col > current.col) ? -1 : 0;

    int row = start.row;
    int col = start.col;

    // Step through each square between start and current
    while (row != current.row || col != current.col) {
      frame[row] |= (1 << col);  // Highlight the current square
      displayFrame(frame);       // Display the updated frame
      delay(5);                // Adjust delay for better visibility

      // Move towards the target position
      if (row != current.row) row += rowStep;
      if (col != current.col) col += colStep;
    }

    // Highlight the current position (final step of the path)
    frame[current.row] |= (1 << current.col);
    displayFrame(frame);
    delay(50);  // Delay after reaching each piece

    // Set the root to the current piece for the next connection (tree-like structure)
    root = current;
  }
}
void flickeringAnimation(byte frame[]) {
    // Number of flickers to display
    const int flickerCount = 10; // Change this to increase or decrease flicker speed
    const int delayTime = 1; // Delay between flickers in milliseconds

    for (int i = 0; i < flickerCount; i++) {
        // Generate a random pattern for the frame
        for (int k = 0; k < 8; k++) {
            frame[k] = random(0x00, 0xFF); // Random byte (0-255) for each row
        }

        displayFrame(frame); // Display the random pattern
        delay(delayTime);    // Delay to create flickering effect
    }

    // Clear the frame after flickering
    for (int k = 0; k < 8; k++) {
        frame[k] = 0x00; // Clear the frame
    }

    displayFrame(frame); // Display the cleared frame
}
void puzzle_test(void) {
    static bool flickeringShown = false; // Static variable to track if flickering has been shown
    byte hallBoardState[8];  // To store the state of the hall sensor board
    byte frame[8];  // To store the display frame

    // Read the hall sensor board state
    readHall(hallBoardState);

    // First, try to highlight only the pieces that are attacking or being attacked
    highlightAttackedPieces(hallBoardState, frame);

    // Check if the frame is empty (no attacking pieces found)
    if (isFrameEmpty(frame)) {
        // If no pieces are attacking each other, highlight all attacked squares
        highlightAttackedSquares(hallBoardState, frame);
    }

    // Check if the frame is full
    if (isFrameFull(frame)) {
        if (!flickeringShown) { // Only show flickering if it hasn't been shown yet
            flickeringAnimation(frame);
            flickeringShown = true; // Mark that flickering has been shown
        }
    } else {
        flickeringShown = false; // Reset the flickeringShown flag if frame is not full
    }

    // Display the frame (either attacked pieces or all attacked squares)
    displayFrame(frame);
}