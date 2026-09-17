#pragma once
#include "save_data.h"
#include "box_storage.h"

namespace BoxBrowser {
enum class View { Recent, Dex, All, Species, Generation, Shiny };
struct Entry {
  uint32_t instanceId;
  uint16_t slot;
  uint16_t speciesId;
  bool shiny;
};
// Metadata only. Sorting/filtering never reads the filesystem or species table.
class Index {
public:
  void reset();
  bool add(Entry entry);
  bool select(View view, uint16_t argument = 0);
  uint16_t count() const { return count_; }
  const Entry* at(uint16_t row) const;
  static bool generationRange(uint16_t generation, uint16_t& first, uint16_t& last);
private:
  Entry entries_[PokemonGame::BOX_CAPACITY] = {};
  uint16_t order_[PokemonGame::BOX_CAPACITY] = {};
  uint16_t size_ = 0, count_ = 0;
};
static_assert(sizeof(Entry) == 12, "Browser entry RAM budget");
static_assert(sizeof(Index) <= 32 * 1024, "Browser index RAM budget");

// Lifetime is one browser visit. Exact NVS root only; never loads/adopts other files.
class Browser {
public:
  bool open(const PokemonGame::BoxRoot& root);
  void close();
  bool isOpen() const { return open_; }
  bool select(View view, uint16_t argument = 0);
  const Index& index() const { return index_; }
  bool read(uint16_t row, PokemonGame::PokemonInstance& output);
private:
  Index index_;
  BoxStorage::Snapshot snapshot_;
  bool open_ = false;
};
}
