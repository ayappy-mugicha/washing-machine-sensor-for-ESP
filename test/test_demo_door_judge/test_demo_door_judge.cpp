#include <unity.h>
#include <DemoDoorJudge.h>
#include <DemoVibrationJudge.h>

// ---- demoDoorStep 単体の検証 ----------------------------------------------

static DemoDoorState closedState() {
  DemoDoorState state;
  state.confirmedOpen = false;
  state.candidateOpen = false;
  return state;
}

void test_closed_window_keeps_closed_and_judge_runs() {
  DemoDoorResult r = demoDoorStep(closedState(), false);
  TEST_ASSERT_FALSE(r.state.confirmedOpen);
  TEST_ASSERT_FALSE(r.changed);
  TEST_ASSERT_FALSE(r.suppressJudge);
}

void test_first_open_window_is_candidate_only_but_suppresses_judge() {
  DemoDoorResult r = demoDoorStep(closedState(), true);
  TEST_ASSERT_FALSE(r.state.confirmedOpen);  // まだ確定しない
  TEST_ASSERT_FALSE(r.changed);
  TEST_ASSERT_TRUE(r.state.candidateOpen);
  TEST_ASSERT_TRUE(r.suppressJudge);  // ここで START を抑止するのが要
}

void test_second_open_window_confirms() {
  DemoDoorResult first = demoDoorStep(closedState(), true);
  DemoDoorResult second = demoDoorStep(first.state, true);
  TEST_ASSERT_TRUE(second.state.confirmedOpen);
  TEST_ASSERT_TRUE(second.changed);
  TEST_ASSERT_TRUE(second.suppressJudge);
}

void test_open_then_closed_resets_candidate() {
  DemoDoorResult first = demoDoorStep(closedState(), true);
  DemoDoorResult second = demoDoorStep(first.state, false);
  TEST_ASSERT_FALSE(second.state.confirmedOpen);
  TEST_ASSERT_FALSE(second.changed);
  TEST_ASSERT_FALSE(second.state.candidateOpen);
  TEST_ASSERT_FALSE(second.suppressJudge);
}

void test_close_needs_two_windows_too() {
  DemoDoorState open;
  open.confirmedOpen = true;
  open.candidateOpen = true;

  DemoDoorResult first = demoDoorStep(open, false);
  TEST_ASSERT_TRUE(first.state.confirmedOpen);  // 1窓では閉に確定しない
  TEST_ASSERT_FALSE(first.changed);

  DemoDoorResult second = demoDoorStep(first.state, false);
  TEST_ASSERT_FALSE(second.state.confirmedOpen);
  TEST_ASSERT_TRUE(second.changed);
}

// ---- demo.cpp の loop と同じ順序で組んだシナリオ検証 -----------------------
// loop の順序: (1) IDLE の時だけ蓋判定 (2) 確定開 or 生判定が開なら
// 振動判定をしない (3) それ以外は DemoVibrationJudge::step。
// washFinished のリセット（開→閉で idle へ）も demo.cpp と同じに扱う。

struct LoopSim {
  DemoVibrationState judge;
  DemoDoorState door;
  bool washFinished;
  int startCount;
  int stopCount;

  LoopSim() : washFinished(false), startCount(0), stopCount(0) {
    judge.phase = DEMO_IDLE;
    judge.quietCount = 0;
    door.confirmedOpen = false;
    door.candidateOpen = false;
  }

  // rawOpen: その窓の生判定（openCount 過半数か）, rms: その窓の rms
  void window(bool windowOk, bool rawOpen, float rms) {
    const DemoVibrationConfig cfg = demoVibrationDefaultConfig();
    bool suppress = false;
    if (windowOk && judge.phase == DEMO_IDLE) {
      DemoDoorResult d = demoDoorStep(door, rawOpen);
      door = d.state;
      suppress = d.suppressJudge;
      if (d.changed && !door.confirmedOpen && washFinished) {
        washFinished = false;  // 開→閉 = 取り出し完了でリセット
      }
    }
    if (door.confirmedOpen || suppress) {
      return;  // 振動判定はしない
    }
    DemoVibrationResult r = DemoVibrationJudge::step(judge, rms, windowOk, cfg);
    judge = r.state;
    if (r.action == DEMO_ACTION_START) {
      ++startCount;
    } else if (r.action == DEMO_ACTION_STOP) {
      ++stopCount;
      washFinished = true;
    }
  }
};

// 実機報告の再現: 洗濯終了後に本体を傾ける（生判定=開、rms は高止まり）。
// 旧ロジックでは1窓目で START して door が永遠に確定しなかった。
void test_tilt_after_finish_confirms_door_without_start() {
  LoopSim sim;
  // 洗濯中→終了（STOP で washFinished が立つ）
  sim.window(true, false, 3.0f);   // START
  sim.window(true, false, 0.5f);
  sim.window(true, false, 0.5f);   // STOP
  TEST_ASSERT_TRUE(sim.washFinished);
  TEST_ASSERT_EQUAL_UINT8(DEMO_IDLE, sim.judge.phase);
  const int startsBeforeTilt = sim.startCount;

  // 傾ける: 生判定=開が続き、rms は 9.8 で高止まり
  sim.window(true, true, 9.8f);
  TEST_ASSERT_FALSE(sim.door.confirmedOpen);  // 1窓目は候補まで
  sim.window(true, true, 9.8f);
  TEST_ASSERT_TRUE(sim.door.confirmedOpen);   // 2窓目で確定する
  sim.window(true, true, 9.8f);
  sim.window(true, true, 9.8f);
  TEST_ASSERT_TRUE(sim.door.confirmedOpen);
  TEST_ASSERT_EQUAL_INT(startsBeforeTilt, sim.startCount);  // 再 START しない
  TEST_ASSERT_EQUAL_UINT8(DEMO_IDLE, sim.judge.phase);
  TEST_ASSERT_TRUE(sim.washFinished);  // finished 表示のまま
}

// 終了→蓋開→蓋閉でリセットされる既存の流れ。
void test_open_close_after_finish_resets_cycle() {
  LoopSim sim;
  sim.window(true, false, 3.0f);
  sim.window(true, false, 0.5f);
  sim.window(true, false, 0.5f);
  TEST_ASSERT_TRUE(sim.washFinished);

  sim.window(true, true, 9.8f);
  sim.window(true, true, 9.8f);
  TEST_ASSERT_TRUE(sim.door.confirmedOpen);

  sim.window(true, false, 0.2f);   // 閉じ始め（1窓目）
  TEST_ASSERT_TRUE(sim.door.confirmedOpen);
  sim.window(true, false, 0.2f);   // 閉が確定
  TEST_ASSERT_FALSE(sim.door.confirmedOpen);
  TEST_ASSERT_FALSE(sim.washFinished);  // idle へリセット
}

// 正常系: 蓋を閉めた通常振動では従来どおり START する。
void test_normal_vibration_still_starts() {
  LoopSim sim;
  sim.window(true, false, 3.0f);
  TEST_ASSERT_EQUAL_INT(1, sim.startCount);
  TEST_ASSERT_EQUAL_UINT8(DEMO_VIBRATING, sim.judge.phase);
  TEST_ASSERT_FALSE(sim.door.confirmedOpen);
}

// 生判定が開の窓が1回だけ混ざっても確定せず、後の通常開始も妨げない。
void test_single_open_window_does_not_block_later_start() {
  LoopSim sim;
  sim.window(true, true, 9.8f);    // ノイズ的に開が1窓
  TEST_ASSERT_FALSE(sim.door.confirmedOpen);
  TEST_ASSERT_EQUAL_INT(0, sim.startCount);  // その窓は抑止される
  sim.window(true, false, 0.2f);   // 候補リセット
  sim.window(true, false, 3.0f);   // 通常の振動開始
  TEST_ASSERT_EQUAL_INT(1, sim.startCount);
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_closed_window_keeps_closed_and_judge_runs);
  RUN_TEST(test_first_open_window_is_candidate_only_but_suppresses_judge);
  RUN_TEST(test_second_open_window_confirms);
  RUN_TEST(test_open_then_closed_resets_candidate);
  RUN_TEST(test_close_needs_two_windows_too);
  RUN_TEST(test_tilt_after_finish_confirms_door_without_start);
  RUN_TEST(test_open_close_after_finish_resets_cycle);
  RUN_TEST(test_normal_vibration_still_starts);
  RUN_TEST(test_single_open_window_does_not_block_later_start);
  return UNITY_END();
}
