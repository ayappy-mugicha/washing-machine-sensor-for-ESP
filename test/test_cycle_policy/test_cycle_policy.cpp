#include <unity.h>
#include <CyclePolicy.h>

static const int kStartHour = 8;
static const int kEndHour = 18;

static WasherJudgeState stateWith(uint8_t phase, uint32_t finishTimeSec) {
  WasherJudgeState state;
  state.phase = phase;
  state.startCount = 0;
  state.washStartSec = 0;
  state.quietSinceSec = 0;
  state.finishTimeSec = finishTimeSec;
  return state;
}

void test_hour_before_start_is_night() {
  TEST_ASSERT_TRUE(shouldNightSleep(WASHER_IDLE, 7, kStartHour, kEndHour));
}

void test_start_hour_is_day() {
  TEST_ASSERT_FALSE(shouldNightSleep(WASHER_IDLE, 8, kStartHour, kEndHour));
}

void test_hour_after_start_is_day() {
  TEST_ASSERT_FALSE(shouldNightSleep(WASHER_IDLE, 9, kStartHour, kEndHour));
}

void test_hour_before_end_is_day() {
  TEST_ASSERT_FALSE(shouldNightSleep(WASHER_IDLE, 17, kStartHour, kEndHour));
}

void test_end_hour_is_night() {
  TEST_ASSERT_TRUE(shouldNightSleep(WASHER_IDLE, 18, kStartHour, kEndHour));
}

void test_hour_after_end_is_night() {
  TEST_ASSERT_TRUE(shouldNightSleep(WASHER_IDLE, 19, kStartHour, kEndHour));
}

void test_midnight_and_late_evening_are_night() {
  TEST_ASSERT_TRUE(shouldNightSleep(WASHER_IDLE, 0, kStartHour, kEndHour));
  TEST_ASSERT_TRUE(shouldNightSleep(WASHER_IDLE, 23, kStartHour, kEndHour));
}

void test_active_phases_do_not_night_sleep_at_end_hour() {
  TEST_ASSERT_FALSE(shouldNightSleep(WASHER_WASHING, 18, kStartHour, kEndHour));
  TEST_ASSERT_FALSE(shouldNightSleep(WASHER_FINISH_CANDIDATE, 18, kStartHour, kEndHour));
  TEST_ASSERT_FALSE(shouldNightSleep(WASHER_SNOOZE_WAIT, 18, kStartHour, kEndHour));
}

void test_remind_sent_keeps_committed_state() {
  const WasherJudgeState previous = stateWith(WASHER_SNOOZE_WAIT, 100);
  WasherJudgeState committed = previous;
  committed.finishTimeSec = 1900;

  const NotifyCommit out = commitNotify(
      committed, 300, previous, WASHER_ACTION_REMIND_HANG, true, 300);
  TEST_ASSERT_EQUAL_UINT8(WASHER_SNOOZE_WAIT, out.state.phase);
  TEST_ASSERT_EQUAL_UINT32(1900, out.state.finishTimeSec);
  TEST_ASSERT_EQUAL_UINT32(300, out.sleepSec);
}

void test_remind_failed_restores_previous_and_retries_soon() {
  const WasherJudgeState previous = stateWith(WASHER_SNOOZE_WAIT, 100);
  WasherJudgeState committed = previous;
  committed.finishTimeSec = 1900;

  const NotifyCommit out = commitNotify(
      committed, 300, previous, WASHER_ACTION_REMIND_HANG, false, 300);
  TEST_ASSERT_EQUAL_UINT8(WASHER_SNOOZE_WAIT, out.state.phase);
  TEST_ASSERT_EQUAL_UINT32(100, out.state.finishTimeSec);
  TEST_ASSERT_EQUAL_UINT32(300, out.sleepSec);
}

void test_thank_sent_keeps_idle_and_long_sleep() {
  const WasherJudgeState previous = stateWith(WASHER_SNOOZE_WAIT, 100);
  const WasherJudgeState committed = stateWith(WASHER_IDLE, 0);

  const NotifyCommit out = commitNotify(
      committed, 3600, previous, WASHER_ACTION_THANK_HUNG, true, 300);
  TEST_ASSERT_EQUAL_UINT8(WASHER_IDLE, out.state.phase);
  TEST_ASSERT_EQUAL_UINT32(3600, out.sleepSec);
}

void test_thank_failed_restores_snooze() {
  const WasherJudgeState previous = stateWith(WASHER_SNOOZE_WAIT, 100);
  const WasherJudgeState committed = stateWith(WASHER_IDLE, 0);

  const NotifyCommit out = commitNotify(
      committed, 3600, previous, WASHER_ACTION_THANK_HUNG, false, 300);
  TEST_ASSERT_EQUAL_UINT8(WASHER_SNOOZE_WAIT, out.state.phase);
  TEST_ASSERT_EQUAL_UINT32(100, out.state.finishTimeSec);
  TEST_ASSERT_EQUAL_UINT32(300, out.sleepSec);
}

void test_no_action_keeps_committed_state_when_send_flag_is_false() {
  const WasherJudgeState previous = stateWith(WASHER_WASHING, 0);
  const WasherJudgeState committed = stateWith(WASHER_FINISH_CANDIDATE, 0);

  const NotifyCommit out = commitNotify(
      committed, 300, previous, WASHER_ACTION_NONE, false, 300);
  TEST_ASSERT_EQUAL_UINT8(WASHER_FINISH_CANDIDATE, out.state.phase);
  TEST_ASSERT_EQUAL_UINT32(300, out.sleepSec);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_hour_before_start_is_night);
  RUN_TEST(test_start_hour_is_day);
  RUN_TEST(test_hour_after_start_is_day);
  RUN_TEST(test_hour_before_end_is_day);
  RUN_TEST(test_end_hour_is_night);
  RUN_TEST(test_hour_after_end_is_night);
  RUN_TEST(test_midnight_and_late_evening_are_night);
  RUN_TEST(test_active_phases_do_not_night_sleep_at_end_hour);
  RUN_TEST(test_remind_sent_keeps_committed_state);
  RUN_TEST(test_remind_failed_restores_previous_and_retries_soon);
  RUN_TEST(test_thank_sent_keeps_idle_and_long_sleep);
  RUN_TEST(test_thank_failed_restores_snooze);
  RUN_TEST(test_no_action_keeps_committed_state_when_send_flag_is_false);
  return UNITY_END();
}
