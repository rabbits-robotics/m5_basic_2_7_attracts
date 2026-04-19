#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_sanity_check(void)
{
  TEST_ASSERT_EQUAL(1, 1);
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_sanity_check);
  return UNITY_END();
}
