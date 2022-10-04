#include "CecpProtocol.h"
#include "ArduinoBleChess.h"
#include "BleChessDevice.h"
#include <Arduino.h>
#if defined(NIM_BLE_ARDUINO_LIB)
#include <regex>
#endif

namespace
{
#if defined(NIM_BLE_ARDUINO_LIB)
static const std::regex uci("[a-h,A-H][1-8][a-h,A-H][1-8][nbrqNBRQ]{0,1}");
#endif
}

#if 0

char incomingMessage[170];

byte getOddParity(byte value)
{
  value ^= value >> 4;
  value ^= value >> 2;
  value ^= value >> 1;
  return (~value) & 1;
}

void sendMessageToChessBoard(const char *message)
{
  Ble::String  codedMessage = message;
  char blockCode[3];

  Serial.println("Message to be sent from Board to Application: ");
  Serial.println(message);

    for (int i = 0; i < codedMessage.length(); i++)
    {
      if (getOddParity(codedMessage[i]))
      {
        codedMessage[i] += 128;
      }
    }
}

void assembleIncomingChesslinkMessage(char readChar)
{
  switch (readChar)
  {
  case 'R':
  case 'I':
  case 'L':
  case 'V':
  case 'W':
  case 'S':
  case 'X':
    incomingMessage[0] = readChar;
    incomingMessage[1] = 0;
    break;
  default:
    byte position = strlen(incomingMessage);
    incomingMessage[position] = readChar;
    incomingMessage[position + 1] = 0;
    break;
  }
}

void sendChesslinkAnswer(char *incomingMessage)
{
  if (strlen(incomingMessage) == 3 && (strcmp(incomingMessage, "V56") == 0))
  {
    Serial.print("Detected valid incoming Version Request Message V: ");
    Serial.print(incomingMessage);
    sendMessageToChessBoard("v0021");  // identify as Millennium Exclusive board
    return;
  }
  if (strlen(incomingMessage) == 5 && incomingMessage[0] == 'R')
  {
    Serial.print("Detected incoming Read EEPROM Message R: ");
    Serial.print(incomingMessage);
    incomingMessage[0] = 'r';
    sendMessageToChessBoard(incomingMessage);
    return;
  }
  if (strlen(incomingMessage) == 7 && incomingMessage[0] == 'W')
  {
    return;
  }
  if (strlen(incomingMessage) == 3 && (strcmp(incomingMessage, "S53") == 0))
  {
    Serial.print("Detected valid incoming Status request Message S: ");
    Serial.print(incomingMessage);
    //sendMessageToChessBoard(chessBoard.boardMessage);
    return;
  }
  if (strlen(incomingMessage) == 167 && incomingMessage[0] == 'L')
  {
    Serial.print("Detected incoming set LED Message L: ");
    Serial.print(incomingMessage);
    sendMessageToChessBoard("l");
    return;
  }
  if (strlen(incomingMessage) == 3 && incomingMessage[0] == 'I')
  {
    Serial.print("Detected incoming Message I: ");
    Serial.print(incomingMessage);
    sendMessageToChessBoard("i0055mm\n");  // Tournament 55 board
    return;
  }
  if (strlen(incomingMessage) == 3 && (strcmp(incomingMessage, "X58") == 0))
  {
    Serial.print("Detected valid incoming Message X: ");
    Serial.print(incomingMessage);
    sendMessageToChessBoard("x");
    return;
  }
}
#endif

void CecpProtocol::begin(BleChessDevice& device)
{
    this->device = &device;
}

void CecpProtocol::onMessage(const Ble::String& cmd)
{
  #if 0
    Serial.print(cmd);
    char readChar;

      for (int i = 0; i < cmd.length(); i++)
      {
        readChar = (cmd[i] & 127); // remove odd parity

        assembleIncomingChesslinkMessage(readChar);
      } 
      Serial.print(incomingMessage);
      sendChesslinkAnswer(incomingMessage);
  #endif  
  
    if (startsWith(cmd, "xboard") || startsWith(cmd, "accepted"))
    {
    }
    else if (startsWith(cmd, "protover"))
    {
        send("feature setboard=1");
    }
    else if (startsWith(cmd, "new"))
    {
        isForceMode = false;
        askDeviceStopMove();
    }
    else if (startsWith(cmd, "setboard"))
    {
        auto fen = getCmdParams(cmd);
        device->onNewGame(fen);
    }
    else if (startsWith(cmd, "go"))
    {
        isForceMode = false;
        askDeviceMakeMove();
    }
    else if (startsWith(cmd, "force"))
    {
        isForceMode = true;
        askDeviceStopMove();
    }
    else if (startsWith(cmd, "Illegal move (without promotion)"))
    {
        isForcedPromotion = true;
    }
    else if (startsWith(cmd, "Illegal move"))
    {
        device->onDeviceMoveRejected(getIllegalMove(cmd));
        askDeviceMakeMove();
    }
#if defined(NIM_BLE_ARDUINO_LIB)
    else if (std::regex_match(cmd, uci))
#else
    else
#endif
    {
        if (isForcedPromotion)
            device->onDeviceMovePromoted(cmd);
        else
            device->onMove(cmd);

        if (not isForceMode)
            askDeviceMakeMove();

        isForcedPromotion = false;
    }
}

void CecpProtocol::onDeviceMove(const Ble::String& mv)
{
    send("move " + mv);
}

void CecpProtocol::telluser(const Ble::String& text)
{
    send("telluser " + text);
}

void CecpProtocol::send(Ble::String str)
{
    ArduinoBleChess.send(str);
}

Ble::String CecpProtocol::getCmdParams(const Ble::String& cmd)
{
    return substring(cmd, indexOf(cmd, ' ') + 1);
}

Ble::String CecpProtocol::getIllegalMove(const Ble::String& cmd)
{
    return substring(cmd, indexOf(cmd, ": ") + 2);
}

void CecpProtocol::askDeviceMakeMove()
{
    if (!isDeviceMove)
        device->askDeviceMakeMove();
    isDeviceMove = true;
}

void CecpProtocol::askDeviceStopMove()
{
    if (isDeviceMove)
        device->askDeviceStopMove();
    isDeviceMove = false;
}

CecpProtocol Protocol{};