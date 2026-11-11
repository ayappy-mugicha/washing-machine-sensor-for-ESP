#ifndef DEMO_DOOR_JUDGE_H
#define DEMO_DOOR_JUDGE_H

// 文化祭デモ用、蓋の開閉判定（2窓連続で一致したら確定）。
//
// 生判定（rawDoorOpen: openCount が成功数の過半数）は demo.cpp 側で
// VibrationWindow から作り、この関数には bool で渡す。
// 評価は振動していない時（DEMO_IDLE）だけ、という前提は呼び出し側が守る。
//
// suppressJudge が重要な理由:
//   蓋を開けると rms が開始しきい値（1.7）を大きく超える。確定前の
//   1窓目で振動判定を走らせると START して phase が DEMO_VIBRATING に
//   移り、蓋判定の対象（IDLE）から外れて、2窓目の確定が永遠に来ない。
//   そのため、生判定が「開」の窓では未確定でも振動判定を抑止する。

struct DemoDoorState {
  bool confirmedOpen;  // 確定した蓋状態
  bool candidateOpen;  // 候補（前窓の生判定）
};

struct DemoDoorResult {
  DemoDoorState state;
  bool changed;        // この窓で確定状態が変わった
  bool suppressJudge;  // 生判定が開 → この窓は振動判定をしない
};

inline DemoDoorResult demoDoorStep(const DemoDoorState& state, bool rawOpen) {
  DemoDoorResult out;
  out.state = state;
  out.changed = false;
  out.suppressJudge = rawOpen;
  if (rawOpen == state.candidateOpen && rawOpen != state.confirmedOpen) {
    out.state.confirmedOpen = rawOpen;
    out.changed = true;
  }
  out.state.candidateOpen = rawOpen;
  return out;
}

#endif
