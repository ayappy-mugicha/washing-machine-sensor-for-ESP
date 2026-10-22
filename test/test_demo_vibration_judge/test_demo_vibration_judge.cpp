#include <unity.h>
#include <DemoVibrationJudge.h>

static DemoVibrationState idleState() {
  DemoVibrationState state;
  state.phase = DEMO_IDLE;
  state.quietCount = 0;
  return state;
}

void test_idle_below_start_updates_offset() {
  const DemoVibrationConfig cfg = demoVibrationDefaultConfig();
  DemoVibrationResult result = DemoVibrationJudge::step(idleState(), 0.10f, true, cfg);
  TEST_ASSERT_EQUAL_UINT8(DEMO_IDLE, result.state.phase);
  TEST_ASSERT_EQUAL_UINT8(DEMO_ACTION_NONE, result.action);
  TEST_ASSERT_TRUE(result.updateOffset);
}

void test_one_window_starts() {
  const DemoVibrationConfig cfg = demoVibrationDefaultConfig();
  DemoVibrationResult result = DemoVibrationJudge::step(idleState(), 0.40f, true, cfg);
  TEST_ASSERT_EQUAL_UINT8(DEMO_VIBRATING, result.state.phase);
  TEST_ASSERT_EQUAL_UINT8(DEMO_ACTION_START, result.action);
  TEST_ASSERT_EQUAL_INT(0, result.state.quietCount);
  TEST_ASSERT_FALSE(result.updateOffset);
}

void test_one_quiet_window_does_not_stop() {
  const DemoVibrationConfig cfg = demoVibrationDefaultConfig();
  DemoVibrationState state = idleState();
  state.phase = DEMO_VIBRATING;

  DemoVibrationResult result = DemoVibrationJudge::step(state, 0.05f, true, cfg);
  TEST_ASSERT_EQUAL_UINT8(DEMO_VIBRATING, result.state.phase);
  TEST_ASSERT_EQUAL_INT(1, result.state.quietCount);
  TEST_ASSERT_EQUAL_UINT8(DEMO_ACTION_NONE, result.action);
}

void test_two_quiet_windows_stop() {
  const DemoVibrationConfig cfg = demoVibrationDefaultConfig();
  DemoVibrationState state = idleState();
  state.phase = DEMO_VIBRATING;

  DemoVibrationResult first = DemoVibrationJudge::step(state, 0.05f, true, cfg);
  DemoVibrationResult second = DemoVibrationJudge::step(first.state, 0.05f, true, cfg);
  TEST_ASSERT_EQUAL_UINT8(DEMO_IDLE, second.state.phase);
  TEST_ASSERT_EQUAL_UINT8(DEMO_ACTION_STOP, second.action);
  TEST_ASSERT_EQUAL_INT(0, second.state.quietCount);
}

void test_vibration_resets_quiet_count() {
  const DemoVibrationConfig cfg = demoVibrationDefaultConfig();
  DemoVibrationState state = idleState();
  state.phase = DEMO_VIBRATING;

  DemoVibrationResult quiet = DemoVibrationJudge::step(state, 0.05f, true, cfg);
  DemoVibrationResult shake = DemoVibrationJudge::step(quiet.state, 0.40f, true, cfg);
  TEST_ASSERT_EQUAL_UINT8(DEMO_VIBRATING, shake.state.phase);
  TEST_ASSERT_EQUAL_INT(0, shake.state.quietCount);
  TEST_ASSERT_EQUAL_UINT8(DEMO_ACTION_NONE, shake.action);
}

void test_can_restart_after_stop() {
  const DemoVibrationConfig cfg = demoVibrationDefaultConfig();
  DemoVibrationState state = idleState();
  state.phase = DEMO_VIBRATING;

  DemoVibrationResult first = DemoVibrationJudge::step(state, 0.05f, true, cfg);
  DemoVibrationResult stopped = DemoVibrationJudge::step(first.state, 0.05f, true, cfg);
  DemoVibrationResult again = DemoVibrationJudge::step(stopped.state, 0.40f, true, cfg);
  TEST_ASSERT_EQUAL_UINT8(DEMO_VIBRATING, again.state.phase);
  TEST_ASSERT_EQUAL_UINT8(DEMO_ACTION_START, again.action);
}

void test_failed_window_keeps_state() {
  const DemoVibrationConfig cfg = demoVibrationDefaultConfig();
  DemoVibrationState state = idleState();
  state.phase = DEMO_VIBRATING;
  state.quietCount = 1;

  DemoVibrationResult result = DemoVibrationJudge::step(state, 0.01f, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(DEMO_VIBRATING, result.state.phase);
  TEST_ASSERT_EQUAL_INT(1, result.state.quietCount);
  TEST_ASSERT_EQUAL_UINT8(DEMO_ACTION_NONE, result.action);
  TEST_ASSERT_FALSE(result.updateOffset);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_idle_below_start_updates_offset);
  RUN_TEST(test_one_window_starts);
  RUN_TEST(test_one_quiet_window_does_not_stop);
  RUN_TEST(test_two_quiet_windows_stop);
  RUN_TEST(test_vibration_resets_quiet_count);
  RUN_TEST(test_can_restart_after_stop);
  RUN_TEST(test_failed_window_keeps_state);
  return UNITY_END();
}
