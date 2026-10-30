#ifndef WASHER_JUDGE_H
#define WASHER_JUDGE_H

#include <math.h>
#include <stdint.h>

enum WasherPhase : uint8_t {
  WASHER_IDLE = 0,
  WASHER_WASHING = 1,
  WASHER_FINISH_CANDIDATE = 2,
  WASHER_SNOOZE_WAIT = 3
};

enum WasherAction : uint8_t {
  WASHER_ACTION_NONE = 0,
  WASHER_ACTION_REMIND_HANG = 1,
  WASHER_ACTION_THANK_HUNG = 2
};

struct WasherJudgeConfig {
  float startThreshold;
  float stopThreshold;
  int startConfirm;
  uint32_t minWashSec;
  uint32_t minQuietSec;
  uint32_t snoozeSec;
  uint32_t sleepIdleSec;
  uint32_t sleepWashingSec;
  uint32_t sleepAfterDoorSec;
  float openDoorDeg;
};

struct WasherJudgeState {
  uint8_t phase;
  int startCount;
  uint32_t washStartSec;
  uint32_t quietSinceSec;
  uint32_t finishTimeSec;
};

struct WindowStats {
  bool ok;
  float rms;
  float peak;
  float tiltDeg;
  int sampleCount;
  int openCount;
};

struct WasherJudgeResult {
  WasherJudgeState state;
  uint32_t sleepSec;
  bool updateOffset;
  uint8_t action;
};

inline float washerTiltDeg(float x, float y, float z) {
  const float horizontal = sqrtf((x * x) + (y * y));
  return atan2f(horizontal, z) * 57.2957795f;
}

inline bool washerDoorOpen(const WindowStats& window) {
  return window.ok && window.sampleCount > 0 &&
         (window.openCount * 2) > window.sampleCount;
}

inline WasherJudgeConfig washerJudgeDefaultConfig() {
  WasherJudgeConfig cfg;
  cfg.startThreshold = 0.90f;
  cfg.stopThreshold = 0.40f;
  cfg.startConfirm = 2;
  cfg.minWashSec = 25u * 60u;
  cfg.minQuietSec = 15u * 60u;
  cfg.snoozeSec = 1800u;
  cfg.sleepIdleSec = 300u;
  cfg.sleepWashingSec = 300u;
  cfg.sleepAfterDoorSec = 3600u;
  cfg.openDoorDeg = 30.0f;
  return cfg;
}

class WasherJudge {
public:
  static WasherJudgeResult step(const WasherJudgeState& state,
                                const WindowStats& window,
                                uint32_t monotonicSec,
                                const WasherJudgeConfig& cfg);
};

#endif
