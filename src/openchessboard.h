#pragma once

extern void initHW(void);
extern String getMoveInput(void);
extern String getFen(void);
extern bool areFensSame(const String& peripheralFen, const String& centralFen);
extern void clearDisplay(void);
extern void displayMove(String mv);
extern void displayWaitForGame(void);
extern void displayNewGame(void);
extern void hw_test(void);
