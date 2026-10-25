#ifndef CYCLE_POLICY_H
#define CYCLE_POLICY_H

#include <WasherJudge.h>
#include <stdint.h>

inline bool isNightHour(int hour, int startHour, int endHour) {
  return hour >= endHour || hour < startHour;
}

inline bool shouldNightSleep(uint8_t phase, int hour, int startHour, int endHour) {
  return phase == WASHER_IDLE && isNightHour(hour, startHour, endHour);
}

struct NotifyCommit {
  WasherJudgeState state;
  uint32_t sleepSec;
};

inline bool notifyNeedsDelivery(uint8_t action) {
  return action == WASHER_ACTION_REMIND_HANG || action == WASHER_ACTION_THANK_HUNG;
}

inline NotifyCommit commitNotify(const WasherJudgeState& committed,
                                 uint32_t sleepSec,
                                 const WasherJudgeState& previous,
                                 uint8_t action,
                                 bool sent,
                                 uint32_t retrySleepSec) {
  NotifyCommit out;
  out.state = committed;
  out.sleepSec = sleepSec;
  if (notifyNeedsDelivery(action) && !sent) {
    out.state = previous;
    out.sleepSec = retrySleepSec;
  }
  return out;
}

#endif
