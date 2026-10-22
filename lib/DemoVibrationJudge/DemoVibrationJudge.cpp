#include "DemoVibrationJudge.h"

DemoVibrationResult DemoVibrationJudge::step(const DemoVibrationState& state,
                                             float rms,
                                             bool windowOk,
                                             const DemoVibrationConfig& cfg) {
  DemoVibrationResult out;
  out.state = state;
  out.updateOffset = false;
  out.action = DEMO_ACTION_NONE;

  if (!windowOk) {
    return out;
  }

  if (state.phase == DEMO_IDLE) {
    if (rms > cfg.startThreshold) {
      out.state.phase = DEMO_VIBRATING;
      out.state.quietCount = 0;
      out.action = DEMO_ACTION_START;
    } else {
      out.updateOffset = true;
    }
    return out;
  }

  if (rms >= cfg.stopThreshold) {
    out.state.quietCount = 0;
    return out;
  }

  out.state.quietCount = state.quietCount + 1;
  if (out.state.quietCount >= cfg.stopConfirm) {
    out.state.phase = DEMO_IDLE;
    out.state.quietCount = 0;
    out.action = DEMO_ACTION_STOP;
  }
  return out;
}
