#include <M5Stack.h>

#include "state.hpp"

static State g_state;

void setup()
{
  M5.begin();
  M5.Lcd.fillScreen(TFT_BLACK);
}

void loop()
{
  M5.update();
  (void)g_state;
  delay(10);
}
