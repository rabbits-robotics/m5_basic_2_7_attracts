#pragma once

#include <cstdint>

enum class Mode : uint8_t { GAMEPAD, TRANSCEIVER, ESTOP };

struct Command
{
  float vx = 0, vy = 0, vz = 0;
  float yaw = 0, pitch = 0;
  uint8_t load = 0, fire = 0;
  uint8_t speed = 0, chassis = 0;
};

struct GamepadStatus
{
  bool connected = false;
  char name[24] = {};
  uint32_t update_ms = 0;
};

struct RobotStatus
{
  bool present = false;
  uint8_t type = 0;
  uint8_t team = 0;
  uint8_t projectile_speed_max = 0;
  uint16_t max_hp = 0, hp = 0;
  uint16_t max_heat = 0, heat = 0;
};

struct Comms
{
  uint32_t tx_count = 0;
  float tx_hz = 0;
  uint32_t rx1_bytes = 0;
  uint32_t rx1_frames = 0;
};

struct State
{
  Mode mode = Mode::GAMEPAD;
  Command cmd;
  GamepadStatus gp;
  RobotStatus robot;
  Comms comms;
};
