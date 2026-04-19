#pragma once

#include <M5Stack.h>

#include <cstdio>
#include <cstring>

#include "labels.hpp"
#include "state.hpp"

namespace lcd_view_detail
{

inline uint16_t mode_color(Mode m)
{
  switch (m) {
    case Mode::GAMEPAD:     return TFT_DARKGREEN;
    case Mode::TRANSCEIVER: return TFT_NAVY;
    case Mode::ESTOP:       return TFT_RED;
  }
  return TFT_BLACK;
}

struct Prev
{
  bool initialized = false;
  Mode mode = Mode::ESTOP;
  bool gp_connected = false;
  char gp_name[24] = {};
  float vx = 0, vy = 0, vz = 0, yaw = 0, pitch = 0;
  uint8_t load = 0, fire = 0, speed = 0, chassis = 0;
  bool robot_present = false;
  uint8_t robot_type = 0, robot_team = 0;
  uint16_t hp = 0, max_hp = 0, heat = 0, max_heat = 0;
  float tx_hz = 0;
  uint32_t tx_count = 0, rx1_bytes = 0, rx1_frames = 0;
};

inline Prev &prev()
{
  static Prev p;
  return p;
}

}  // namespace lcd_view_detail

inline void lcd_init()
{
  M5.Lcd.fillScreen(TFT_BLACK);

  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Lcd.setCursor(4, 34);
  M5.Lcd.print("Cmd");
  M5.Lcd.setCursor(4, 132);
  M5.Lcd.print("Robot");

  M5.Lcd.drawFastHLine(0, 30, 320, TFT_DARKGREY);
  M5.Lcd.drawFastHLine(0, 128, 320, TFT_DARKGREY);
  M5.Lcd.drawFastHLine(0, 200, 320, TFT_DARKGREY);

  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Lcd.setCursor(25, 222);
  M5.Lcd.print("[A]GP");
  M5.Lcd.setCursor(118, 222);
  M5.Lcd.print("[B]STOP");
  M5.Lcd.setCursor(237, 222);
  M5.Lcd.print("[C]TR");

  lcd_view_detail::prev().initialized = false;
}

inline void lcd_draw(const State &s)
{
  using namespace lcd_view_detail;
  auto &p = prev();
  const bool force = !p.initialized;

  if (force || p.mode != s.mode) {
    const uint16_t bg = mode_color(s.mode);
    M5.Lcd.fillRect(0, 0, 320, 30, bg);
    M5.Lcd.setTextColor(TFT_WHITE, bg);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(4, 8);
    M5.Lcd.printf("MODE: %s", mode_label(s.mode));
    p.mode = s.mode;
    p.gp_name[0] = '\xff';
  }

  if (force || p.gp_connected != s.gp.connected ||
      strncmp(p.gp_name, s.gp.name, sizeof(p.gp_name)) != 0)
  {
    const uint16_t bg = mode_color(s.mode);
    M5.Lcd.fillRect(180, 4, 140, 24, bg);
    M5.Lcd.setTextColor(TFT_WHITE, bg);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(184, 12);
    M5.Lcd.printf("GP: %s", s.gp.connected ? s.gp.name : "---");
    p.gp_connected = s.gp.connected;
    std::strncpy(p.gp_name, s.gp.name, sizeof(p.gp_name) - 1);
    p.gp_name[sizeof(p.gp_name) - 1] = '\0';
  }

  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

  if (force || p.vx != s.cmd.vx || p.vy != s.cmd.vy || p.vz != s.cmd.vz) {
    M5.Lcd.fillRect(4, 54, 316, 16, TFT_BLACK);
    M5.Lcd.setCursor(4, 54);
    M5.Lcd.printf("vx%+5.2f vy%+5.2f vz%+5.2f", s.cmd.vx, s.cmd.vy, s.cmd.vz);
    p.vx = s.cmd.vx;
    p.vy = s.cmd.vy;
    p.vz = s.cmd.vz;
  }

  if (force || p.yaw != s.cmd.yaw || p.pitch != s.cmd.pitch) {
    M5.Lcd.fillRect(4, 74, 316, 16, TFT_BLACK);
    M5.Lcd.setCursor(4, 74);
    M5.Lcd.printf("yaw%+6.3f  pitch%+6.3f", s.cmd.yaw, s.cmd.pitch);
    p.yaw = s.cmd.yaw;
    p.pitch = s.cmd.pitch;
  }

  if (force || p.load != s.cmd.load || p.fire != s.cmd.fire ||
      p.speed != s.cmd.speed || p.chassis != s.cmd.chassis)
  {
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.fillRect(4, 94, 316, 16, TFT_BLACK);
    M5.Lcd.setCursor(4, 94);
    M5.Lcd.printf("ld %s fr %s sp %c ch %c",
                  load_label(s.cmd.load), fire_label(s.cmd.fire),
                  speed_label(s.cmd.speed)[0], chassis_label(s.cmd.chassis)[0]);
    p.load = s.cmd.load;
    p.fire = s.cmd.fire;
    p.speed = s.cmd.speed;
    p.chassis = s.cmd.chassis;
  }

  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);

  if (force || p.robot_present != s.robot.present ||
      p.robot_type != s.robot.type || p.robot_team != s.robot.team)
  {
    M5.Lcd.fillRect(120, 134, 200, 10, TFT_BLACK);
    M5.Lcd.setCursor(120, 134);
    M5.Lcd.printf("type %s  team %s",
                  type_label(s.robot.type, s.robot.present),
                  team_label(s.robot.team, s.robot.present));
    p.robot_present = s.robot.present;
    p.robot_type = s.robot.type;
    p.robot_team = s.robot.team;
  }

  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

  if (force || p.hp != s.robot.hp || p.max_hp != s.robot.max_hp) {
    M5.Lcd.fillRect(4, 154, 316, 16, TFT_BLACK);
    M5.Lcd.setCursor(4, 154);
    if (s.robot.present) {
      M5.Lcd.printf("HP   %4u/%4u", s.robot.hp, s.robot.max_hp);
    } else {
      M5.Lcd.printf("HP   ----/----");
    }
    p.hp = s.robot.hp;
    p.max_hp = s.robot.max_hp;
  }

  if (force || p.heat != s.robot.heat || p.max_heat != s.robot.max_heat) {
    M5.Lcd.fillRect(4, 176, 316, 16, TFT_BLACK);
    M5.Lcd.setCursor(4, 176);
    if (s.robot.present) {
      M5.Lcd.printf("Heat %4u/%4u", s.robot.heat, s.robot.max_heat);
    } else {
      M5.Lcd.printf("Heat ----/----");
    }
    p.heat = s.robot.heat;
    p.max_heat = s.robot.max_heat;
  }

  if (force || p.tx_hz != s.comms.tx_hz || p.tx_count != s.comms.tx_count ||
      p.rx1_bytes != s.comms.rx1_bytes)
  {
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    M5.Lcd.fillRect(4, 204, 316, 16, TFT_BLACK);
    M5.Lcd.setCursor(4, 204);
    M5.Lcd.printf("TX%5.1fHz #%-5lu RX%luB",
                  s.comms.tx_hz,
                  static_cast<unsigned long>(s.comms.tx_count),
                  static_cast<unsigned long>(s.comms.rx1_bytes));
    p.tx_hz = s.comms.tx_hz;
    p.tx_count = s.comms.tx_count;
    p.rx1_bytes = s.comms.rx1_bytes;
    p.rx1_frames = s.comms.rx1_frames;
  }

  p.initialized = true;
}
