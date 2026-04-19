#include <M5Stack.h>

void setup()
{
  M5.begin();
  M5.Lcd.fillScreen(TFT_BLACK);
}

void loop()
{
  M5.update();
  delay(10);
}
