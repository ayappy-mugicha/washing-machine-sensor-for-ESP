#include <unity.h>
#include <WasherJudge.h>

static WasherJudgeState idleState() {
  WasherJudgeState state;
  state.phase = WASHER_IDLE;
  state.startCount = 0;
  state.washStartSec = 0;
  state.quietSinceSec = 0;
  state.finishTimeSec = 0;
  return state;
}

static WindowStats windowRms(float rms) {
  WindowStats window;
  window.ok = true;
  window.rms = rms;
  window.peak = rms;
  window.lastY = 0;
  return window;
}

void test_idle_single_spike_does_not_start() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeResult first = WasherJudge::step(idleState(), windowRms(1.5f), 100, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_IDLE, first.state.phase);
  TEST_ASSERT_EQUAL_INT(1, first.state.startCount);
  TEST_ASSERT_FALSE(first.updateOffset);

  WasherJudgeResult second = WasherJudge::step(first.state, windowRms(0.1f), 400, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_IDLE, second.state.phase);
  TEST_ASSERT_EQUAL_INT(0, second.state.startCount);
  TEST_ASSERT_TRUE(second.updateOffset);
}

void test_two_spikes_start_washing() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeResult first = WasherJudge::step(idleState(), windowRms(1.5f), 100, false, cfg);
  WasherJudgeResult second = WasherJudge::step(first.state, windowRms(1.2f), 400, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_WASHING, second.state.phase);
  TEST_ASSERT_EQUAL_UINT32(400, second.state.washStartSec);
  TEST_ASSERT_EQUAL_INT(0, second.state.startCount);
}

void test_quiet_before_min_wash_does_not_finish() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_WASHING;
  state.washStartSec = 0;

  WasherJudgeResult result = WasherJudge::step(state, windowRms(0.05f), 5 * 60, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_WASHING, result.state.phase);
  TEST_ASSERT_EQUAL_UINT32(0, result.state.quietSinceSec);
  TEST_ASSERT_NOT_EQUAL(WASHER_ACTION_REMIND_HANG, result.action);
}

void test_spin_dry_alternating_rms_does_not_confirm_finish() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_WASHING;
  state.washStartSec = 0;
  uint32_t now = cfg.minWashSec;

  WasherJudgeResult result = WasherJudge::step(state, windowRms(0.05f), now, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_FINISH_CANDIDATE, result.state.phase);

  now += 300;
  result = WasherJudge::step(result.state, windowRms(1.1f), now, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_WASHING, result.state.phase);
  TEST_ASSERT_EQUAL_UINT32(0, result.state.quietSinceSec);

  now += 300;
  result = WasherJudge::step(result.state, windowRms(0.05f), now, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_FINISH_CANDIDATE, result.state.phase);
  TEST_ASSERT_EQUAL_UINT32(now, result.state.quietSinceSec);

  now += 300;
  result = WasherJudge::step(result.state, windowRms(0.05f), now, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_FINISH_CANDIDATE, result.state.phase);
  TEST_ASSERT_NOT_EQUAL(WASHER_SNOOZE_WAIT, result.state.phase);
}

void test_long_quiet_after_min_wash_enters_snooze() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_WASHING;
  state.washStartSec = 0;

  uint32_t now = cfg.minWashSec;
  WasherJudgeResult result = WasherJudge::step(state, windowRms(0.05f), now, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_FINISH_CANDIDATE, result.state.phase);

  now += cfg.minQuietSec;
  result = WasherJudge::step(result.state, windowRms(0.05f), now, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_SNOOZE_WAIT, result.state.phase);
  TEST_ASSERT_EQUAL_UINT32(now, result.state.finishTimeSec);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_NONE, result.action);
}

void test_snooze_does_not_fire_when_finish_time_is_in_the_future() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_SNOOZE_WAIT;
  state.finishTimeSec = 50000;

  WasherJudgeResult result = WasherJudge::step(state, windowRms(0.05f), 8000, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_SNOOZE_WAIT, result.state.phase);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_NONE, result.action);
  TEST_ASSERT_EQUAL_UINT32(50000, result.state.finishTimeSec);
}

void test_snooze_reminds_after_elapsed_seconds() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_SNOOZE_WAIT;
  state.finishTimeSec = 100;

  WasherJudgeResult result = WasherJudge::step(state, windowRms(0.05f), 100 + cfg.snoozeSec, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_REMIND_HANG, result.action);
  TEST_ASSERT_EQUAL_UINT32(100 + cfg.snoozeSec, result.state.finishTimeSec);
}

void test_door_open_clears_snooze() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_SNOOZE_WAIT;
  state.finishTimeSec = 100;

  WindowStats closed = windowRms(0.05f);
  closed.lastY = 8.0f;
  WasherJudgeResult result = WasherJudge::step(state, closed, 400, true, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_IDLE, result.state.phase);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_THANK_HUNG, result.action);
  TEST_ASSERT_EQUAL_UINT32(cfg.sleepAfterDoorSec, result.sleepSec);
}

void test_failed_window_keeps_state() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_WASHING;
  state.washStartSec = 0;
  WindowStats bad;
  bad.ok = false;
  bad.rms = 99.0f;
  bad.peak = 99.0f;
  bad.lastY = 0;

  WasherJudgeResult result = WasherJudge::step(state, bad, 2000, false, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_WASHING, result.state.phase);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_NONE, result.action);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_idle_single_spike_does_not_start);
  RUN_TEST(test_two_spikes_start_washing);
  RUN_TEST(test_quiet_before_min_wash_does_not_finish);
  RUN_TEST(test_spin_dry_alternating_rms_does_not_confirm_finish);
  RUN_TEST(test_long_quiet_after_min_wash_enters_snooze);
  RUN_TEST(test_snooze_does_not_fire_when_finish_time_is_in_the_future);
  RUN_TEST(test_snooze_reminds_after_elapsed_seconds);
  RUN_TEST(test_door_open_clears_snooze);
  RUN_TEST(test_failed_window_keeps_state);
  return UNITY_END();
}
