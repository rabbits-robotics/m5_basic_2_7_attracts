#include <unity.h>

#include "labels.hpp"

void setUp(void) {}
void tearDown(void) {}

void test_mode_label(void)
{
  TEST_ASSERT_EQUAL_STRING("GAMEPAD", mode_label(Mode::GAMEPAD));
  TEST_ASSERT_EQUAL_STRING("TRANSCEIVER", mode_label(Mode::TRANSCEIVER));
  TEST_ASSERT_EQUAL_STRING("ESTOP", mode_label(Mode::ESTOP));
}

void test_load_label(void)
{
  TEST_ASSERT_EQUAL_STRING("STOP", load_label(0));
  TEST_ASSERT_EQUAL_STRING("FWD", load_label(1));
  TEST_ASSERT_EQUAL_STRING("REV", load_label(2));
  TEST_ASSERT_EQUAL_STRING("?", load_label(99));
}

void test_fire_label(void)
{
  TEST_ASSERT_EQUAL_STRING("STOP", fire_label(0));
  TEST_ASSERT_EQUAL_STRING("LOW", fire_label(1));
  TEST_ASSERT_EQUAL_STRING("HIGH", fire_label(2));
  TEST_ASSERT_EQUAL_STRING("?", fire_label(99));
}

void test_speed_label(void)
{
  TEST_ASSERT_EQUAL_STRING("LOW", speed_label(0));
  TEST_ASSERT_EQUAL_STRING("HIGH", speed_label(1));
  TEST_ASSERT_EQUAL_STRING("?", speed_label(99));
}

void test_chassis_label(void)
{
  TEST_ASSERT_EQUAL_STRING("NORM", chassis_label(0));
  TEST_ASSERT_EQUAL_STRING("INF", chassis_label(1));
  TEST_ASSERT_EQUAL_STRING("?", chassis_label(99));
}

void test_type_label(void)
{
  TEST_ASSERT_EQUAL_STRING("---", type_label(0, false));
  TEST_ASSERT_EQUAL_STRING("TANK", type_label(0, true));
  TEST_ASSERT_EQUAL_STRING("ASSL", type_label(1, true));
  TEST_ASSERT_EQUAL_STRING("MARK", type_label(2, true));
  TEST_ASSERT_EQUAL_STRING("?", type_label(99, true));
}

void test_team_label(void)
{
  TEST_ASSERT_EQUAL_STRING("-", team_label(0, false));
  TEST_ASSERT_EQUAL_STRING("A", team_label(0, true));
  TEST_ASSERT_EQUAL_STRING("B", team_label(1, true));
  TEST_ASSERT_EQUAL_STRING("?", team_label(99, true));
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_mode_label);
  RUN_TEST(test_load_label);
  RUN_TEST(test_fire_label);
  RUN_TEST(test_speed_label);
  RUN_TEST(test_chassis_label);
  RUN_TEST(test_type_label);
  RUN_TEST(test_team_label);
  return UNITY_END();
}
