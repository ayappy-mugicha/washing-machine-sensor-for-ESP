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
  window.tiltDeg = 0;
  window.sampleCount = 10;
  window.openCount = 0;
  return window;
}

static WindowStats windowOpen(float rms) {
  WindowStats window = windowRms(rms);
  window.tiltDeg = 40.0f;
  window.sampleCount = 10;
  window.openCount = 8;
  return window;
}

void test_idle_single_spike_does_not_start() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeResult first = WasherJudge::step(idleState(), windowRms(1.5f), 100, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_IDLE, first.state.phase);
  TEST_ASSERT_EQUAL_INT(1, first.state.startCount);
  TEST_ASSERT_FALSE(first.updateOffset);

  WasherJudgeResult second = WasherJudge::step(first.state, windowRms(0.1f), 400, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_IDLE, second.state.phase);
  TEST_ASSERT_EQUAL_INT(0, second.state.startCount);
  TEST_ASSERT_TRUE(second.updateOffset);
}

void test_two_spikes_start_washing() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeResult first = WasherJudge::step(idleState(), windowRms(1.5f), 100, cfg);
  WasherJudgeResult second = WasherJudge::step(first.state, windowRms(1.2f), 400, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_WASHING, second.state.phase);
  TEST_ASSERT_EQUAL_UINT32(400, second.state.washStartSec);
  TEST_ASSERT_EQUAL_INT(0, second.state.startCount);
}

void test_quiet_before_min_wash_does_not_finish() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_WASHING;
  state.washStartSec = 0;

  WasherJudgeResult result = WasherJudge::step(state, windowRms(0.05f), 5 * 60, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_WASHING, result.state.phase);
  TEST_ASSERT_EQUAL_UINT32(0, result.state.quietSinceSec);
  TEST_ASSERT_NOT_EQUAL(WASHER_ACTION_REMIND_HANG, result.action);
}

void test_door_open_during_wash_does_not_finish() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_WASHING;
  state.washStartSec = 0;

  WasherJudgeResult result = WasherJudge::step(state, windowOpen(1.2f), 10 * 60, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_WASHING, result.state.phase);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_NONE, result.action);
  TEST_ASSERT_FALSE(result.updateOffset);
}

void test_spin_dry_alternating_rms_does_not_confirm_finish() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_WASHING;
  state.washStartSec = 0;
  uint32_t now = cfg.minWashSec;

  WasherJudgeResult result = WasherJudge::step(state, windowRms(0.05f), now, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_FINISH_CANDIDATE, result.state.phase);

  now += 300;
  result = WasherJudge::step(result.state, windowRms(1.1f), now, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_WASHING, result.state.phase);
  TEST_ASSERT_EQUAL_UINT32(0, result.state.quietSinceSec);

  now += 300;
  result = WasherJudge::step(result.state, windowRms(0.05f), now, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_FINISH_CANDIDATE, result.state.phase);
  TEST_ASSERT_EQUAL_UINT32(now, result.state.quietSinceSec);

  now += 300;
  result = WasherJudge::step(result.state, windowRms(0.05f), now, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_FINISH_CANDIDATE, result.state.phase);
  TEST_ASSERT_NOT_EQUAL(WASHER_SNOOZE_WAIT, result.state.phase);
}

void test_long_quiet_after_min_wash_enters_snooze() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_WASHING;
  state.washStartSec = 0;

  uint32_t now = cfg.minWashSec;
  WasherJudgeResult result = WasherJudge::step(state, windowRms(0.05f), now, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_FINISH_CANDIDATE, result.state.phase);

  now += cfg.minQuietSec;
  result = WasherJudge::step(result.state, windowRms(0.05f), now, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_SNOOZE_WAIT, result.state.phase);
  TEST_ASSERT_EQUAL_UINT32(now, result.state.finishTimeSec);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_NONE, result.action);
}

void test_finish_confirm_with_door_open_goes_idle() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_WASHING;
  state.washStartSec = 0;

  uint32_t now = cfg.minWashSec;
  WasherJudgeResult result = WasherJudge::step(state, windowRms(0.05f), now, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_FINISH_CANDIDATE, result.state.phase);

  now += cfg.minQuietSec;
  result = WasherJudge::step(result.state, windowOpen(0.05f), now, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_IDLE, result.state.phase);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_THANK_HUNG, result.action);
  TEST_ASSERT_FALSE(result.updateOffset);
  TEST_ASSERT_EQUAL_UINT32(cfg.sleepAfterDoorSec, result.sleepSec);
}

void test_snooze_does_not_fire_when_finish_time_is_in_the_future() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_SNOOZE_WAIT;
  state.finishTimeSec = 50000;

  WasherJudgeResult result = WasherJudge::step(state, windowRms(0.05f), 8000, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_SNOOZE_WAIT, result.state.phase);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_NONE, result.action);
  TEST_ASSERT_EQUAL_UINT32(50000, result.state.finishTimeSec);
}

void test_snooze_reminds_after_elapsed_seconds() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_SNOOZE_WAIT;
  state.finishTimeSec = 100;

  WasherJudgeResult result =
      WasherJudge::step(state, windowRms(0.05f), 100 + cfg.snoozeSec, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_REMIND_HANG, result.action);
  TEST_ASSERT_EQUAL_UINT32(100 + cfg.snoozeSec, result.state.finishTimeSec);
}

void test_snooze_elapsed_and_door_open_thanks_without_remind() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_SNOOZE_WAIT;
  state.finishTimeSec = 100;

  WasherJudgeResult result =
      WasherJudge::step(state, windowOpen(0.05f), 100 + cfg.snoozeSec, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_IDLE, result.state.phase);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_THANK_HUNG, result.action);
  TEST_ASSERT_NOT_EQUAL(WASHER_ACTION_REMIND_HANG, result.action);
  TEST_ASSERT_FALSE(result.updateOffset);
  TEST_ASSERT_EQUAL_UINT32(cfg.sleepAfterDoorSec, result.sleepSec);
}

void test_single_open_sample_is_not_a_majority() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_SNOOZE_WAIT;
  state.finishTimeSec = 100;

  WindowStats window = windowRms(0.05f);
  window.sampleCount = 10;
  window.openCount = 1;
  window.tiltDeg = 80.0f;

  WasherJudgeResult result =
      WasherJudge::step(state, window, 100 + cfg.snoozeSec, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_SNOOZE_WAIT, result.state.phase);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_REMIND_HANG, result.action);
  TEST_ASSERT_FALSE(washerDoorOpen(window));
}

void test_door_open_clears_snooze() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_SNOOZE_WAIT;
  state.finishTimeSec = 100;

  WasherJudgeResult result = WasherJudge::step(state, windowOpen(0.05f), 400, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_IDLE, result.state.phase);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_THANK_HUNG, result.action);
  TEST_ASSERT_FALSE(result.updateOffset);
  TEST_ASSERT_EQUAL_UINT32(cfg.sleepAfterDoorSec, result.sleepSec);
}

void test_snooze_failed_window_does_not_remind() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  WasherJudgeState state = idleState();
  state.phase = WASHER_SNOOZE_WAIT;
  state.finishTimeSec = 100;

  WindowStats bad;
  bad.ok = false;
  bad.rms = 99.0f;
  bad.peak = 99.0f;
  bad.tiltDeg = 0;
  bad.sampleCount = 0;
  bad.openCount = 0;

  WasherJudgeResult result = WasherJudge::step(state, bad, 100 + cfg.snoozeSec, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_SNOOZE_WAIT, result.state.phase);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_NONE, result.action);
  TEST_ASSERT_EQUAL_UINT32(100, result.state.finishTimeSec);
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
  bad.tiltDeg = 0;
  bad.sampleCount = 0;
  bad.openCount = 0;

  WasherJudgeResult result = WasherJudge::step(state, bad, 2000, cfg);
  TEST_ASSERT_EQUAL_UINT8(WASHER_WASHING, result.state.phase);
  TEST_ASSERT_EQUAL_UINT8(WASHER_ACTION_NONE, result.action);
}

void test_closed_lid_tilt_is_near_zero() {
  const float tilt = washerTiltDeg(0.0f, 0.0f, 9.8f);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, tilt);
}

void test_tilt_on_x_or_y_exceeds_open_threshold() {
  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  TEST_ASSERT_TRUE(washerTiltDeg(9.8f, 0.0f, 0.0f) > cfg.openDoorDeg);
  TEST_ASSERT_TRUE(washerTiltDeg(0.0f, 9.8f, 0.0f) > cfg.openDoorDeg);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_idle_single_spike_does_not_start);
  RUN_TEST(test_two_spikes_start_washing);
  RUN_TEST(test_quiet_before_min_wash_does_not_finish);
  RUN_TEST(test_door_open_during_wash_does_not_finish);
  RUN_TEST(test_spin_dry_alternating_rms_does_not_confirm_finish);
  RUN_TEST(test_long_quiet_after_min_wash_enters_snooze);
  RUN_TEST(test_finish_confirm_with_door_open_goes_idle);
  RUN_TEST(test_snooze_does_not_fire_when_finish_time_is_in_the_future);
  RUN_TEST(test_snooze_reminds_after_elapsed_seconds);
  RUN_TEST(test_snooze_elapsed_and_door_open_thanks_without_remind);
  RUN_TEST(test_single_open_sample_is_not_a_majority);
  RUN_TEST(test_door_open_clears_snooze);
  RUN_TEST(test_snooze_failed_window_does_not_remind);
  RUN_TEST(test_failed_window_keeps_state);
  RUN_TEST(test_closed_lid_tilt_is_near_zero);
  RUN_TEST(test_tilt_on_x_or_y_exceeds_open_threshold);
  return UNITY_END();
}
