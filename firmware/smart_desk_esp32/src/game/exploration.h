#pragma once
#include <cstdint>

namespace PokemonGame {
constexpr uint64_t MIN_EXPLORATION_EPOCH = 1700000000ULL;
constexpr uint16_t TEST_REGION_ID = 1;
constexpr uint32_t TEST_EXPLORATION_SECONDS = 15;
enum class ExplorationStatus : uint8_t { Idle, Exploring, Complete };
struct ExplorationSession {
  ExplorationStatus status = ExplorationStatus::Idle;
  uint16_t regionId = 0;
  uint64_t startedAtEpoch = 0;
  uint32_t durationSeconds = 0;
};

bool isValidExploration(const ExplorationSession& session);
// Epoch 0 (or pre-sync clock values) means unavailable. No platform clock access.
bool startExploration(ExplorationSession& session, uint16_t regionId,
                      uint64_t nowEpoch, uint32_t durationSeconds);
// True only when Exploring changes to Complete.
bool updateExploration(ExplorationSession& session, uint64_t nowEpoch);
uint32_t remainingSeconds(const ExplorationSession& session, uint64_t nowEpoch);
bool acknowledgeExploration(ExplorationSession& session);
}
