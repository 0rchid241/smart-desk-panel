#include "box_browser.h"
#include <algorithm>

namespace BoxBrowser {
using namespace PokemonGame;
void Index::reset() { size_ = count_ = 0; }
bool Index::add(Entry entry) {
  if (size_ >= BOX_CAPACITY || entry.slot >= BOX_CAPACITY || !entry.instanceId ||
      !entry.speciesId || entry.speciesId > POKEDEX_SPECIES_COUNT) return false;
  entries_[size_++] = entry;
  return true;
}
bool Index::generationRange(uint16_t generation, uint16_t& first, uint16_t& last) {
  constexpr uint16_t ends[] = {0,151,251,386,493,649,721,809,905,1025};
  if (generation < 1 || generation > 9) return false;
  first = ends[generation - 1] + 1; last = ends[generation];
  return true;
}
bool Index::select(View view, uint16_t argument) {
  uint16_t first = 1, last = POKEDEX_SPECIES_COUNT;
  switch (view) {
    case View::Species:
      if (!argument || argument > POKEDEX_SPECIES_COUNT) return false;
      first = last = argument; break;
    case View::Generation:
      if (!generationRange(argument, first, last)) return false;
      break;
    case View::Recent: case View::Dex: case View::All: case View::Shiny: break;
    default: return false;
  }
  count_ = 0;
  for (uint16_t i = 0; i < size_; ++i) {
    const auto& e = entries_[i];
    if (e.speciesId >= first && e.speciesId <= last && (view != View::Shiny || e.shiny))
      order_[count_++] = i;
  }
  std::sort(order_, order_ + count_, [this, view](uint16_t a, uint16_t b) {
    const auto& left = entries_[a]; const auto& right = entries_[b];
    if (view == View::All) return left.slot < right.slot;
    if (view == View::Dex || view == View::Generation) {
      if (left.speciesId != right.speciesId) return left.speciesId < right.speciesId;
      return left.instanceId < right.instanceId;
    }
    return left.instanceId > right.instanceId;
  });
  return true;
}
const Entry* Index::at(uint16_t row) const {
  return row < count_ ? &entries_[order_[row]] : nullptr;
}
void Browser::close() { snapshot_.close(); index_.reset(); open_ = false; }
bool Browser::open(const BoxRoot& root) {
  close();
  if (!isValidBoxRoot(root) || BoxStorage::mount() != BoxStorage::Result::Ok ||
      snapshot_.open({root.storeId, root.generation}) != BoxStorage::Result::Ok) return false;
  const auto& m = snapshot_.metadata();
  if (m.key.storeId != root.storeId || m.key.generation != root.generation ||
      m.occupiedCount != root.occupiedCount || m.crc32 != root.snapshotCrc32) {
    close(); return false;
  }
  for (uint32_t slot = 0; slot < BOX_CAPACITY; ++slot) {
    bool occupied = false;
    if (snapshot_.occupied(slot, occupied) != BoxStorage::Result::Ok) { close(); return false; }
    if (!occupied) continue;
    PokemonInstance pokemon;
    if (snapshot_.readSlot(slot, pokemon) != BoxStorage::Result::Ok ||
        !index_.add({pokemon.instanceId, static_cast<uint16_t>(slot), pokemon.speciesId, pokemon.shiny})) {
      close(); return false;
    }
  }
  open_ = index_.select(View::Recent);
  return open_;
}
bool Browser::select(View view, uint16_t argument) { return open_ && index_.select(view, argument); }
bool Browser::read(uint16_t row, PokemonInstance& output) {
  const Entry* entry = index_.at(row);
  if (!open_ || !entry) return false;
  PokemonInstance pokemon;
  if (snapshot_.readSlot(entry->slot, pokemon) != BoxStorage::Result::Ok ||
      pokemon.instanceId != entry->instanceId || pokemon.speciesId != entry->speciesId ||
      pokemon.shiny != entry->shiny) { close(); return false; }
  output = pokemon;
  return true;
}
}
