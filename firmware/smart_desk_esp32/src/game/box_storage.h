#pragma once
#include "box_data.h"
#include <LittleFS.h>

// ESP32 boundary. No calls from GameApp in B1; callers supply every snapshot key.
// Serialized use only: no concurrent create/delete/unmount or external file edits.
namespace BoxStorage {
using PokemonGame::BoxKey;
using PokemonGame::BoxMetadata;
using PokemonGame::BoxMutation;
using PokemonGame::PokemonInstance;
enum class Result { Ok, NotMounted, Missing, Collision, Invalid, IoError,
                    InvalidArgument, EmptySlot, SlotOccupied, NotFound, DuplicateId };
constexpr size_t PATH_SIZE = 52;
bool snapshotPath(BoxKey key, char* output, size_t capacity);
Result mount(); // Never formats on failure.
void unmount(); // Close all Snapshot readers first.
Result exists(BoxKey key, bool& output);
Result removeSnapshot(BoxKey key); // Explicit only. Caller must protect referenced roots.

class Snapshot {
public:
  Snapshot() = default;
  Snapshot(const Snapshot&) = delete;
  Snapshot& operator=(const Snapshot&) = delete;
  ~Snapshot() { close(); }
  Result open(BoxKey key); // Full validation before exposing metadata/records.
  void close();
  const BoxMetadata& metadata() const { return metadata_; } // Valid after successful open.
  Result occupied(uint32_t slot, bool& output) const;
  Result readSlot(uint32_t slot, PokemonInstance& output);
  Result findEmpty(uint32_t& slot) const;
  Result findInstance(uint32_t id, uint32_t& slot);
  // Repeated calls with startSlot=previous+1 enumerate duplicates of a species.
  Result findSpecies(uint16_t species, uint32_t startSlot, uint32_t& slot);
private:
  Result validateFile(BoxKey key);
  Result validateUniqueIds();
  fs::File file_;
  BoxMetadata metadata_;
  uint8_t bitmap_[PokemonGame::BOX_BITMAP_SIZE] = {};
  bool ready_ = false;
};
Result validate(BoxKey key, BoxMetadata& output);
Result createEmpty(BoxKey key);
Result mutate(BoxKey source, uint64_t newGeneration, const BoxMutation& mutation);
// Failed writes may leave an unreferenced destination file. Never overwrite it;
// caller can inspect/delete it explicitly. Source is never opened for writing.
}
