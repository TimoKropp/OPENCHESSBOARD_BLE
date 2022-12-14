#include <Arduino.h>
#include "libs\ArduinoBleChess.h"
#include "libs\openchessboard.h"

#define DEBUG false  //set to true for debug output, false for no debug output
#define DEBUG_SERIAL if(DEBUG)Serial

bool skip_next_send = false;
bool my_castling_rights = true;
bool opp_castling_rights = true;

bool game_running = false;

bool checkCastling(String move_input) {
  //check if last move was king move from castling
  if(((move_input == "e1g1") || (move_input == "e1c1") ||  (move_input == "e8g8") || (move_input == "e8c8")) ){
    
    if (my_castling_rights)
    {
      my_castling_rights = false;
      DEBUG_SERIAL.println("my castle move...");
      return true;
    }
    if (opp_castling_rights)
    {
      opp_castling_rights = false;
      DEBUG_SERIAL.print("opponent castle move...");
      return true;
    }
  }  
}

class MyBleChessDevice : public BleChessDevice
{
public:
  void onNewGame(const Ble::String& fen) {
    clearDisplay();
    displayWtoPlay();
    
    game_running = true;
    DEBUG_SERIAL.print("new game: ");
    DEBUG_SERIAL.println(fen.c_str());
  }
  void askDeviceMakeMove() {
    DEBUG_SERIAL.println("please move: ");
  }

  void askDeviceStopMove() {
    clearDisplay();
    game_running = false;
    skip_next_send = false;
    my_castling_rights = true;
    opp_castling_rights = true;
    DEBUG_SERIAL.println("stop move: ");
  }

  void onMove(const Ble::String& mv) {
    clearDisplay();
    if (game_running){
      DEBUG_SERIAL.print("moved from phone: ");
      DEBUG_SERIAL.println(mv.c_str());
      displayMove(mv.c_str());
      skip_next_send = true;
    }
  }

  void onDeviceMoveRejected(const Ble::String& mv) {
    if (game_running){
      DEBUG_SERIAL.print("move rejected: ");
      DEBUG_SERIAL.println(mv.c_str());
      for (int k = 0; k < 3; k++){
        clearDisplay();
        delay(200);
        displayMove(mv.c_str());
        delay(200); 
      }
      skip_next_send = true; /* give one try to reverse move, before sending next move to central */
    }
  }

  void onDeviceMovePromoted(const Ble::String& mv) {
    DEBUG_SERIAL.print("promoted on phone screen: ");
    DEBUG_SERIAL.println(mv.c_str());
  }
  
  void checkDeviceMove() {
    
      Ble::String move;
      move = getMoveInput();

      if(checkCastling(move)){
        getMoveInput(); /*get second move from castling but do not send it: send king move only after second input */
      }

      DEBUG_SERIAL.print("moved from board: ");
      DEBUG_SERIAL.println(move.c_str());
      
      clearDisplay();
      if (!skip_next_send){
        deviceMove(move);
      }
      skip_next_send = false; /*skip only once, then allow new move send */
  }
};
MyBleChessDevice device{};


void setup() {
  initHW();

  DEBUG_SERIAL.begin(115200);
#if DEBUG
  while(!Serial);
#endif
  DEBUG_SERIAL.println("BLE init: OPENCHESSBOARD");
  if (!ArduinoBleChess.begin("OPENCHESSBOARD", device)){
    DEBUG_SERIAL.println("BLE initialization error");
  }
  DEBUG_SERIAL.println("start BLE polling...");

}


void loop() {

  while (true){
    #if defined(BLE_PULL_REQUIRED)
      BLE.poll();

    #endif
      device.checkDeviceMove();
  }
}
