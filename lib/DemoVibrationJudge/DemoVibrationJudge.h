#ifndef DEMO_VIBRATION_JUDGE_H
#define DEMO_VIBRATION_JUDGE_H

#include <stdint.h>

enum DemoPhase : uint8_t {
  DEMO_IDLE = 0,
  DEMO_VIBRATING = 1
};

enum DemoAction : uint8_t {
  DEMO_ACTION_NONE = 0,
  DEMO_ACTION_START = 1,
  DEMO_ACTION_STOP = 2
};

struct DemoVibrationConfig {
  float startThreshold;
  float stopThreshold;
  int stopConfirm;
};

struct DemoVibrationState {
  uint8_t phase;
  int quietCount;
};

struct DemoVibrationResult {
  DemoVibrationState state;
  bool updateOffset;
  uint8_t action;
};

inline DemoVibrationConfig demoVibrationDefaultConfig() {
  DemoVibrationConfig cfg;
  cfg.startThreshold = 0.25f;
  cfg.stopThreshold = 0.15f;
  cfg.stopConfirm = 2;
  return cfg;
}

class DemoVibrationJudge {
public:
  static DemoVibrationResult step(const DemoVibrationState& state,
                                  float rms,
                                  bool windowOk,
                                  const DemoVibrationConfig& cfg);
};

#endif
