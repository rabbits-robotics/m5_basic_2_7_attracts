#include <M5Stack.h>

#include "lcd_view.hpp"
#include "state.hpp"

static State g_state;

void setup()
{
  M5.begin();
  lcd_init();
  lcd_draw(g_state);
}

void loop()
{
  M5.update();
  lcd_draw(g_state);
  delay(100);
}
