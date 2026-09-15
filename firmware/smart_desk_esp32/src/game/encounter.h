#pragma once

#include "pokemon.h"

#include "exploration.h"

#include <cstdint>


namespace PokemonGame {

enum class EncounterStatus : uint8_t {
  None,
  Ready
};


// 탐험이 끝난 뒤 확정된 야생 포켓몬.
//
// 아직 BattleState는 아니다.
// 조우 결과가 재부팅으로 다시 뽑히지 않도록
// 저장할 최소 정보만 가진다.
struct EncounterState {
  EncounterStatus status =
    EncounterStatus::None;

  SpeciesId speciesId =
    0;

  FormId formId =
    0;

  uint8_t level =
    0;

  Gender gender =
    Gender::Genderless;

  bool shiny =
    false;
};


// 현재 조우 상태가 유효한지 검사한다.
bool isValidEncounter(
  const EncounterState& encounter
);


// 이미 확정된 야생 포켓몬을 조우 상태에 넣는다.
//
// 이번 단계에서는 랜덤 조우를 만들지 않는다.
// 이후 Encounter Engine이 결정한 결과를
// 이 함수에 전달하게 된다.
bool setEncounter(
  EncounterState& encounter,
  SpeciesId speciesId,
  FormId formId,
  uint8_t level,
  Gender gender,
  bool shiny
);


// 조우를 종료하고 빈 상태로 되돌린다.
void clearEncounter(
  EncounterState& encounter
);


// G3 Vertical Slice용 공개 테스트 조우.
//
// roll은 플랫폼 쪽에서 전달한다.
// 이 함수 자체는 Arduino random(), millis() 등에 의존하지 않는다.
//
// 이미 Ready 상태라면 다시 뽑지 않는다.
bool resolveTestEncounter(
  EncounterState& encounter,
  uint32_t roll
);


// 동일한 탐험에서는 항상 동일한 조우 roll을 만든다.
//
// ESP32 random(), millis()에 의존하지 않는다.
// 탐험 시작 시각 / 지역 / 탐험 시간을 기반으로 한다.
uint32_t makeTestEncounterRoll(
  const ExplorationSession& session
);

}