#pragma once
#include "save_data.h"

// ESP32 capture coordinator; core battle code never knows about files/NVS.
namespace CaptureStorage {
enum class Result { Committed, NotCommitted, Indeterminate, StorageError,
                    Unavailable, BoxFull, IdExhausted, GenerationExhausted };
// Serialized use, after load/initialize. Only Committed changes live/report.
// A failed capture can still be Committed (counterattack/HP/RNG persisted).
Result attempt(PokemonGame::GameSave& live, PokemonGame::CaptureBall ball,
               PokemonGame::BattleCaptureReport& report);
}
