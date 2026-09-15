#include "exploration.h"
#include <limits>

namespace PokemonGame {
bool isValidExploration(const ExplorationSession& s) {
  if (s.status == ExplorationStatus::Idle)
    return s.regionId == 0 && s.startedAtEpoch == 0 && s.durationSeconds == 0;
  if (s.status != ExplorationStatus::Exploring && s.status != ExplorationStatus::Complete)
    return false;
  return s.regionId == TEST_REGION_ID && s.startedAtEpoch >= MIN_EXPLORATION_EPOCH &&
         s.durationSeconds > 0 &&
         s.startedAtEpoch <= std::numeric_limits<uint64_t>::max() - s.durationSeconds;
}

bool startExploration(ExplorationSession& session, uint16_t regionId,
                      uint64_t nowEpoch, uint32_t durationSeconds) {
  if (session.status != ExplorationStatus::Idle || !isValidExploration(session)) return false;
  ExplorationSession next{ExplorationStatus::Exploring, regionId, nowEpoch, durationSeconds};
  if (!isValidExploration(next)) return false;
  session = next;
  return true;
}

uint32_t remainingSeconds(const ExplorationSession& s, uint64_t nowEpoch) {
  if (!isValidExploration(s) || s.status != ExplorationStatus::Exploring) return 0;
  // A missing/backward clock must never underflow into an instant completion.
  if (nowEpoch < MIN_EXPLORATION_EPOCH || nowEpoch < s.startedAtEpoch) return s.durationSeconds;
  const uint64_t elapsed = nowEpoch - s.startedAtEpoch;
  return elapsed >= s.durationSeconds ? 0 : s.durationSeconds - static_cast<uint32_t>(elapsed);
}

bool updateExploration(ExplorationSession& session, uint64_t nowEpoch) {
  if (session.status != ExplorationStatus::Exploring || !isValidExploration(session) ||
      nowEpoch < MIN_EXPLORATION_EPOCH || nowEpoch < session.startedAtEpoch ||
      remainingSeconds(session, nowEpoch) != 0) return false;
  session.status = ExplorationStatus::Complete;
  return true;
}

bool acknowledgeExploration(ExplorationSession& session) {
  if (session.status != ExplorationStatus::Complete || !isValidExploration(session)) return false;
  session = ExplorationSession{};
  return true;
}
}
