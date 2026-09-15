#include "encounter.h"


namespace PokemonGame {

bool isValidEncounter(
  const EncounterState& encounter
) {

  if (
    encounter.status ==
    EncounterStatus::None
  ) {

    return
      encounter.speciesId == 0 &&
      encounter.formId == 0 &&
      encounter.level == 0 &&
      encounter.gender == Gender::Genderless &&
      encounter.shiny == false;
  }


  if (
    encounter.status !=
    EncounterStatus::Ready
  ) {
    return false;
  }


  if (
    encounter.speciesId == 0 ||
    encounter.speciesId > 1025
  ) {
    return false;
  }


  if (
    findSpecies(
      encounter.speciesId,
      encounter.formId
    ) == nullptr
  ) {
    return false;
  }


  if (
    encounter.level == 0 ||
    encounter.level > 100
  ) {
    return false;
  }


  if (
    static_cast<uint8_t>(
      encounter.gender
    ) >
    static_cast<uint8_t>(
      Gender::Genderless
    )
  ) {
    return false;
  }


  return true;
}


bool setEncounter(
  EncounterState& encounter,
  SpeciesId speciesId,
  FormId formId,
  uint8_t level,
  Gender gender,
  bool shiny
) {

  EncounterState next;

  next.status =
    EncounterStatus::Ready;

  next.speciesId =
    speciesId;

  next.formId =
    formId;

  next.level =
    level;

  next.gender =
    gender;

  next.shiny =
    shiny;


  if (
    !isValidEncounter(
      next
    )
  ) {
    return false;
  }


  encounter =
    next;

  return true;
}


void clearEncounter(
  EncounterState& encounter
) {

  encounter =
    EncounterState{};
}


bool resolveTestEncounter(
  EncounterState& encounter,
  uint32_t roll
) {

  // 이미 결과가 있으면 절대 다시 뽑지 않는다.
  if (
    encounter.status !=
    EncounterStatus::None
  ) {
    return false;
  }


  const uint32_t encounterRoll =
    roll % 100;


  SpeciesId speciesId = 0;
  uint8_t level = 1;


  // G3 Vertical Slice용 임시 fixture.
  //
  // 최종 지역/출현 콘텐츠가 아니다.
  if (encounterRoll < 50) {

    speciesId = 19;   // 꼬렛
    level = 2;

  } else if (encounterRoll < 85) {

    speciesId = 16;   // 구구
    level = 3;

  } else {

    speciesId = 25;   // 피카츄
    level = 4;
  }


  const Gender gender =
    ((roll >> 8) & 1u)
      ? Gender::Female
      : Gender::Male;


  return setEncounter(
    encounter,
    speciesId,
    0,
    level,
    gender,
    false
  );
}


uint32_t makeTestEncounterRoll(
  const ExplorationSession& session
) {

  // FNV-1a 기반의 간단한 deterministic hash.
  // 암호학적 난수 목적이 아니라
  // "같은 탐험 = 같은 결과"를 보장하기 위한 값이다.

  uint32_t hash =
    2166136261u;


  auto mix =
    [&hash](
      uint32_t value
    ) {

      for (
        int i = 0;
        i < 4;
        ++i
      ) {

        hash ^=
          static_cast<uint8_t>(
            value >>
            (i * 8)
          );

        hash *=
          16777619u;
      }
    };


  mix(
    static_cast<uint32_t>(
      session.regionId
    )
  );

  mix(
    static_cast<uint32_t>(
      session.startedAtEpoch
    )
  );

  mix(
    static_cast<uint32_t>(
      session.startedAtEpoch >> 32
    )
  );

  mix(
    session.durationSeconds
  );


  return hash;
}

}