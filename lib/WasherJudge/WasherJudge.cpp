#include "WasherJudge.h"

static bool elapsedAtLeast(uint32_t now, uint32_t then, uint32_t need) {
  return now >= then && (now - then) >= need;
}

static void markHung(WasherJudgeResult& out, const WasherJudgeConfig& cfg) {
  out.state.phase = WASHER_IDLE;
  out.state.startCount = 0;
  out.state.quietSinceSec = 0;
  out.action = WASHER_ACTION_THANK_HUNG;
  out.sleepSec = cfg.sleepAfterDoorSec;
  out.updateOffset = false;
}

WasherJudgeResult WasherJudge::step(const WasherJudgeState& state,
                                    const WindowStats& window,
                                    uint32_t monotonicSec,
                                    const WasherJudgeConfig& cfg) {
  WasherJudgeResult out;
  out.state = state;
  out.sleepSec = cfg.sleepIdleSec;
  out.updateOffset = false;
  out.action = WASHER_ACTION_NONE;

  if (state.phase == WASHER_SNOOZE_WAIT) {
    if (!window.ok) {
      return out;
    }
    if (washerDoorOpen(window)) {
      markHung(out, cfg);
      return out;
    }
    if (elapsedAtLeast(monotonicSec, state.finishTimeSec, cfg.snoozeSec)) {
      out.state.finishTimeSec = monotonicSec;
      out.action = WASHER_ACTION_REMIND_HANG;
    }
    return out;
  }

  if (!window.ok) {
    return out;
  }

  if (state.phase == WASHER_IDLE) {
    if (window.rms > cfg.startThreshold) {
      out.state.startCount = state.startCount + 1;
      if (out.state.startCount >= cfg.startConfirm) {
        out.state.phase = WASHER_WASHING;
        out.state.startCount = 0;
        out.state.washStartSec = monotonicSec;
        out.state.quietSinceSec = 0;
        out.sleepSec = cfg.sleepWashingSec;
      }
    } else {
      out.state.startCount = 0;
      out.updateOffset = true;
    }
    return out;
  }

  if (window.rms >= cfg.stopThreshold) {
    out.state.phase = WASHER_WASHING;
    out.state.quietSinceSec = 0;
    out.sleepSec = cfg.sleepWashingSec;
    return out;
  }

  if (!elapsedAtLeast(monotonicSec, state.washStartSec, cfg.minWashSec)) {
    out.state.phase = WASHER_WASHING;
    out.state.quietSinceSec = 0;
    out.sleepSec = cfg.sleepWashingSec;
    return out;
  }

  const uint32_t quietSince =
      (state.phase == WASHER_FINISH_CANDIDATE && state.quietSinceSec != 0)
          ? state.quietSinceSec
          : monotonicSec;
  out.state.quietSinceSec = quietSince;
  out.state.phase = WASHER_FINISH_CANDIDATE;
  out.sleepSec = cfg.sleepWashingSec;

  if (elapsedAtLeast(monotonicSec, quietSince, cfg.minQuietSec)) {
    if (washerDoorOpen(window)) {
      markHung(out, cfg);
      return out;
    }
    out.state.phase = WASHER_SNOOZE_WAIT;
    out.state.finishTimeSec = monotonicSec;
    out.state.quietSinceSec = 0;
    out.state.startCount = 0;
    out.sleepSec = cfg.sleepIdleSec;
    out.action = WASHER_ACTION_REMIND_HANG;
  }
  return out;
}
