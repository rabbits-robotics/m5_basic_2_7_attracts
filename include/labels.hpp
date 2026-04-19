#pragma once

#include <cstdint>

#include "state.hpp"

inline const char *mode_label(Mode m)
{
  switch (m) {
    case Mode::GAMEPAD:     return "GAMEPAD";
    case Mode::TRANSCEIVER: return "TRANSCEIVER";
    case Mode::ESTOP:       return "ESTOP";
  }
  return "?";
}

inline const char *load_label(uint8_t v)
{
  switch (v) {
    case 0: return "STOP";
    case 1: return "FWD";
    case 2: return "REV";
  }
  return "?";
}

inline const char *fire_label(uint8_t v)
{
  switch (v) {
    case 0: return "STOP";
    case 1: return "LOW";
    case 2: return "HIGH";
  }
  return "?";
}

inline const char *speed_label(uint8_t v)
{
  switch (v) {
    case 0: return "LOW";
    case 1: return "HIGH";
  }
  return "?";
}

inline const char *chassis_label(uint8_t v)
{
  switch (v) {
    case 0: return "NORM";
    case 1: return "INF";
  }
  return "?";
}

inline const char *type_label(uint8_t type, bool present)
{
  if (!present) return "---";
  switch (type) {
    case 0: return "TANK";
    case 1: return "ASSL";
    case 2: return "MARK";
  }
  return "?";
}

inline const char *team_label(uint8_t team, bool present)
{
  if (!present) return "-";
  switch (team) {
    case 0: return "A";
    case 1: return "B";
  }
  return "?";
}
