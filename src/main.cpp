#include <M5Stack.h>

#include <cmath>

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

  if (M5.BtnA.wasPressed()) g_state.mode = Mode::GAMEPAD;
  if (M5.BtnB.wasPressed()) g_state.mode = Mode::ESTOP;
  if (M5.BtnC.wasPressed()) g_state.mode = Mode::TRANSCEIVER;

  const float t = millis() / 1000.0f;

  if (g_state.mode == Mode::GAMEPAD) {
    g_state.cmd.vx = std::sin(t);
    g_state.cmd.vy = std::sin(t * 0.7f + 1.0f);
    g_state.cmd.vz = 0.5f * std::sin(t * 1.3f);
    g_state.cmd.yaw = std::fmod(t * 0.5f, 2.0f * 3.14159265f);
    g_state.cmd.pitch = 0.3f * std::sin(t * 0.8f);

    const uint32_t sec = static_cast<uint32_t>(t);
    g_state.cmd.load = static_cast<uint8_t>(sec % 3);
    g_state.cmd.fire = static_cast<uint8_t>((sec / 2) % 3);
    g_state.cmd.speed = static_cast<uint8_t>((sec / 3) % 2);
    g_state.cmd.chassis = static_cast<uint8_t>((sec / 4) % 2);
  } else {
    g_state.cmd = Command{};
  }

  g_state.comms.tx_count++;
  g_state.comms.tx_hz = 10.0f;

  lcd_draw(g_state);
  delay(100);
}
