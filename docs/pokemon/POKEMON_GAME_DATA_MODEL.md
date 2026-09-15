# Pokémon Mini Game 데이터 모델 설계서

> 버전: v0.1  
> 상태: 구현 전 설계  
> 기준 문서: `POKEMON_MINIGAME_PUBLIC_DESIGN_v0.3.md`

---

## 1. 설계 목표

이 문서는 ESP32 버전의 첫 실제 게임 엔진을 만들기 전에 `PokemonSpecies`, `PokemonInstance`, `GameState`, Party, Box, Pokédex, Save 구조를 확정하기 위한 설계서다.

핵심 원칙:

- 원작 Pokémon 데이터와 플레이어의 개체 상태를 분리한다.
- 모든 1,025종의 정적 데이터를 RAM에 복사하지 않는다.
- 박스 전체를 RAM에 올리지 않는다.
- 세이브 데이터는 버전업과 향후 PC 포팅을 고려한다.
- C++ `struct`의 메모리 배치를 그대로 파일에 쓰지 않는다.
- ESP32 전용 API가 게임 코어 데이터 구조 안으로 들어오지 않게 한다.
- 배틀 중 임시 상태와 영구 저장 상태를 분리한다.

---

# 2. 데이터의 세 계층

게임 데이터는 세 종류로 분리한다.

## 2.1 정적 데이터 (Static Content)

게임 제작 시 정해지고 플레이 중에는 변하지 않는다.

예:

- 종 번호
- 이름
- 타입
- 기본 능력치 6종
- 포획 난이도
- 성장 그룹
- 성별 비율
- 기술 데이터
- 레벨별 기술 습득
- 진화 관계
- 폼/지방폼 데이터
- 종 전용 특수 메커니즘

대표 타입:

- `PokemonSpecies`
- `PokemonForm`
- `MoveData`
- `LearnsetEntry`
- `EvolutionRule`

이 데이터는 가능한 한 Flash/콘텐츠 저장소에서 직접 조회하고, 1,025종 전체를 RAM 구조체 배열로 복제하지 않는다.

---

## 2.2 영구 상태 (Persistent State)

전원을 껐다 켜도 유지되어야 한다.

예:

- 보유 Pokémon
- 파티
- 박스
- 지정 Desk Pet
- 경험치/레벨
- 친밀도
- 기술 구성
- 성별/폼/이로치
- 도감
- 지역 해금
- Ball 등급
- Master Ball 수
- 진화 핵심 아이템
- 게임 진행 플래그

대표 타입:

- `PokemonInstance`
- `GameProgress`
- `PokedexState`
- `GameSave`

---

## 2.3 런타임 상태 (Transient Runtime State)

배틀/탐험 중에만 필요하고 전원 재시작 후 그대로 복구할 필요가 없는 데이터다.

예:

- 현재 배틀의 HP
- 현재 PP
- 독/화상/마비 등 상태
- 능력치 랭크
- 현재 야생 Pokémon
- 탐험 경과 시간
- 현재 화면/메뉴 커서
- 전투 임시 효과

대표 타입:

- `BattleState`
- `BattlePokemonState`
- `ExplorationState`
- `EncounterState`

원칙:

> 탐험 시작 전 마지막 안전 상태를 저장하고, 탐험/배틀 도중 전원이 꺼지면 해당 탐험은 취소된 것으로 처리해도 된다.

따라서 복잡한 배틀 임시 상태를 세이브 파일에 넣지 않는다.

---

# 3. 공통 ID 타입

## 3.1 Species ID

```cpp
using SpeciesId = uint16_t;
```

- 전국도감 1~1025를 표현하기에 충분
- `0`은 invalid/none으로 예약 가능

## 3.2 Move ID

```cpp
using MoveId = uint16_t;
```

기술 수가 255개를 넘으므로 16bit 사용.

## 3.3 Instance ID

```cpp
using PokemonInstanceId = uint32_t;
```

포획한 실제 개체마다 고유 ID를 부여한다.

사용처:

- Desk Pet 지정
- 파티/박스 이동 추적
- 향후 이벤트에서 특정 개체 참조

`nextInstanceId`를 세이브에 저장한다.

---

# 4. PokemonSpecies

`PokemonSpecies`는 특정 개체가 아니라 **종의 고정 데이터**다.

개념 구조:

```cpp
struct PokemonSpecies {
  SpeciesId speciesId;

  TypeId primaryType;
  TypeId secondaryType;

  uint8_t baseHp;
  uint8_t baseAttack;
  uint8_t baseDefense;
  uint8_t baseSpAttack;
  uint8_t baseSpDefense;
  uint8_t baseSpeed;

  uint8_t catchRate;
  GrowthRate growthRate;
  uint8_t genderRatio;

  uint16_t learnsetOffset;
  uint16_t learnsetCount;

  uint16_t evolutionOffset;
  uint8_t evolutionCount;

  uint16_t flags;
};
```

실제 구현에서는 메모리 정렬과 테이블 구조에 맞춰 필드를 재배치해도 된다.

### 저장하지 않는 것

`PokemonSpecies`는 세이브 파일에 저장하지 않는다.

개체는 `speciesId`만 가지고 정적 테이블에서 종 정보를 조회한다.

---

# 5. Form / Regional Form

전국도감 번호와 폼은 분리한다.

예:

```text
speciesId = 26      // Raichu
formId = 0          // 일반
formId = 1          // Alolan
```

개념 타입:

```cpp
using FormId = uint8_t;
```

폼 데이터는 기본 종 데이터에서 달라지는 부분만 override하는 방향을 우선한다.

예:

- 타입
- 기본 능력치
- 외형/asset key
- 진화 조건
- 종 전용 메커니즘

도감의 1,025종 등록과 폼 컬렉션은 별도 상태로 관리한다.

---

# 6. PokemonInstance

`PokemonInstance`는 실제로 포획해 보유하는 한 마리의 Pokémon이다.

권장 영구 데이터:

```cpp
struct PokemonInstance {
  PokemonInstanceId instanceId;
  SpeciesId speciesId;

  uint32_t exp;

  uint16_t currentHp;

  MoveId moves[4];

  uint8_t level;
  uint8_t friendship;
  FormId formId;
  uint8_t flags;
};
```

`flags`에는 bit 단위로 다음을 저장할 수 있다.

- gender
- shiny
- 필요한 소수의 개체 상태 플래그

### 저장하지 않는 것

- IV
- EV
- 성격 능력치 보정
- 최대 HP
- 공격/방어/특공/특방/스피드 최종값
- 현재 PP
- 배틀 상태이상
- 능력치 랭크

최대 HP와 최종 능력치는 `species + level + form`에서 계산한다.

PP는 탐험 종료 시 자동 회복하므로 런타임 배틀 상태에 둔다.

---

# 7. PokemonInstance 예상 크기

직렬화 레코드는 C++ `sizeof()`에 의존하지 않고 직접 필드별로 기록한다.

현재 권장 필드 기준 목표 크기:

```text
instanceId       4
speciesId        2
exp              4
currentHp        2
moves[4]         8
level            1
friendship       1
formId           1
flags            1
------------------
합계            24 bytes
```

따라서 이론상:

```text
256마리   ≈  6 KB
512마리   ≈ 12 KB
1024마리  ≈ 24 KB
2048마리  ≈ 48 KB
```

세이브 용량 자체는 큰 문제가 아니다.

실제 병목은 Pokémon 스프라이트/애니메이션 asset이 될 가능성이 훨씬 높다.

---

# 8. Party

파티 최대 3마리.

권장:

```cpp
struct PartyState {
  PokemonInstance members[3];
  uint8_t count;
};
```

파티 Pokémon은 자주 접근하므로 RAM에 유지한다.

전투 중 교체 가능.

파티에서 박스로 이동하면 해당 개체 레코드를 Box 저장소로 이동한다.

---

# 9. Box

박스 전체를 RAM에 올리지 않는 방향을 권장한다.

## 9.1 Box 파일 구조

고정 크기 24 byte 슬롯을 사용하는 방식을 우선 고려한다.

```text
BoxStorage
├─ occupancy bitmap
├─ slot 0: PokemonInstance record
├─ slot 1: PokemonInstance record
├─ slot 2: empty
├─ ...
└─ slot N
```

장점:

- 특정 슬롯만 읽고 쓰기 쉬움
- 놓아주기 시 전체 파일 재작성 불필요
- 수백~수천 개체를 저장해도 RAM 부담이 작음
- PC 버전에서도 동일한 논리 구조 사용 가능

## 9.2 박스 최대치

지금 숫자를 고정하지 않는다.

다만 24 byte record 기준으로는 1,024~2,048개체도 저장 용량 자체는 매우 작다.

최종 최대치는 다음을 확인한 뒤 확정한다.

- 실제 ESP32 파일시스템 파티션 크기
- 파일시스템 overhead
- 세이브 A/B 백업
- 이벤트 데이터
- 기타 persistent data

권장 시작 후보:

- 1,024 slots
- 또는 여유가 충분하면 2,048 slots

박스 UI에서는 전체를 한 번에 로드하지 않고 현재 페이지의 일부 슬롯만 읽는다.

---

# 10. Desk Pet 지정

Desk Pet은 `PokemonInstanceId`로 지정한다.

```cpp
PokemonInstanceId deskPetId;
```

이렇게 하면 해당 Pokémon이 파티와 박스 사이를 이동해도 같은 개체를 계속 추적할 수 있다.

부팅 시 해당 개체를 찾아 Desk Pet cache에 로드한다.

---

# 11. Pokédex

전국도감 상태는 bitset을 사용한다.

1,025 bits:

```text
ceil(1025 / 8) = 129 bytes
```

권장:

```cpp
struct PokedexState {
  uint8_t seen[129];
  uint8_t caught[129];
  uint8_t shinyCaught[129];
};
```

약 387 bytes.

### 폼 컬렉션

폼은 전국도감 species bitset과 분리한다.

빌드 시 전체 collectible form에 연속적인 `formCollectionIndex`를 부여하고 별도 bitset으로 저장하는 방식이 적합하다.

---

# 12. MoveData

정적 데이터.

개념 필드:

```cpp
struct MoveData {
  MoveId moveId;
  TypeId type;
  MoveCategory category; // Physical / Special / Status

  uint8_t power;
  uint8_t accuracy;
  uint8_t maxPp;
  uint16_t effectId;
};
```

기술 이름/설명은 별도 문자열 테이블을 사용할 수 있다.

복잡한 모든 원작 기술 효과를 첫 버전부터 구현하지 않는다.

공통 효과를 먼저 구현하고 지원하지 않는 특수 효과는 단계적으로 확장한다.

---

# 13. Learnset

종마다 큰 고정 배열을 가지게 하지 않고 공용 테이블 + offset/count 방식 사용을 권장한다.

예:

```cpp
struct LearnsetEntry {
  uint8_t level;
  MoveId moveId;
};
```

`PokemonSpecies`는:

```text
learnsetOffset
learnsetCount
```

만 가진다.

---

# 14. EvolutionRule

진화는 데이터 기반 공통 규칙 + 소수의 custom rule로 나눈다.

개념:

```cpp
enum class EvolutionTrigger : uint8_t {
  Level,
  Item,
  Friendship,
  TimeOfDay,
  Gender,
  KnownMove,
  Region,
  Custom
};
```

각 Rule은:

- target species/form
- trigger
- parameter
- 추가 flags

를 가진다.

특이한 원작 진화는 `Custom` ID를 통해 별도 handler로 처리한다.

---

# 15. 종 전용 특수 메커니즘

전체 Ability 시스템은 만들지 않는다.

대신 다음 구조를 사용한다.

```text
SpeciesMechanicId
```

예:

- 캐스퐁 날씨 폼 변화
- 체리꼬 폼 변화
- 껍질몬 특수 HP/방어 규칙
- 기타 특성이 없으면 정체성이 사라지는 Pokémon

종 데이터의 flag/mechanic ID를 통해 필요한 경우에만 전용 handler를 호출한다.

---

# 16. GameProgress

영구적인 게임 진행 상태.

예:

```cpp
struct GameProgress {
  uint32_t nextInstanceId;
  PokemonInstanceId deskPetId;

  uint8_t ballTier;
  uint8_t masterBallCount;

  uint32_t playTimeSeconds;

  // unlocked region bitset
  // story/progression flag bitset
  // key item bitset
};
```

지역/이벤트 수가 확정되기 전에는 bitset 크기를 콘텐츠 생성 단계에서 정한다.

---

# 17. GameSave 논리 구조

```text
GameSave
├─ SaveHeader
├─ GameProgress
├─ PartyState
├─ PokedexState
├─ FormCollection
├─ KeyItems / ProgressFlags
└─ BoxStorage (별도 파일 가능)
```

Box는 크기가 크고 일부만 자주 읽으므로 메인 세이브와 분리하는 것을 권장한다.

---

# 18. 저장 포맷

C++ 구조체를 `reinterpret_cast`하여 그대로 Flash에 쓰지 않는다.

이유:

- compiler padding
- ESP32/PC 구조 차이
- endian/정렬 차이
- 필드 추가 시 이전 세이브 호환 어려움

명시적인 serializer/deserializer를 사용한다.

## SaveHeader 권장

```text
magic
saveVersion
sequence
payloadLength
crc32
```

예:

```text
magic = "PKDG"
```

### A/B 세이브

가능하면:

```text
save_a.bin
save_b.bin
```

을 번갈아 기록한다.

부팅 시:

1. 두 파일 CRC 검사
2. 정상 파일 중 sequence가 높은 것 선택
3. 하나가 깨져도 다른 하나로 복구

이 방식은 전원 차단 중 save corruption 위험을 낮춘다.

---

# 19. 자동 저장 시점

모든 프레임마다 저장하지 않는다.

저장 후보:

- Pokémon 포획 성공
- 레벨업/진화 완료
- 파티/박스 변경
- Pokémon 놓아주기
- Desk Pet 변경
- 지역 해금
- Ball 등급 상승
- 핵심 이벤트 완료
- GAME MODE 종료 전 안전 시점

탐험 시작 직전에도 안전 저장을 둘 수 있다.

---

# 20. BattlePokemonState

배틀 중 임시 상태는 별도 구조를 사용한다.

예:

```cpp
struct BattlePokemonState {
  PokemonInstanceId instanceId;

  uint16_t currentHp;
  uint8_t pp[4];
  StatusCondition status;

  int8_t attackStage;
  int8_t defenseStage;
  int8_t spAttackStage;
  int8_t spDefenseStage;
  int8_t speedStage;
};
```

배틀이 끝나면:

- EXP 적용
- friendship 적용
- 포획 결과 적용
- 필요한 영구 변화만 `PokemonInstance`에 반영
- 탐험 종료 시 HP/PP 자동 회복

---

# 21. 스타팅 질문과 초기 세이브

첫 세이브가 없으면:

```text
NEW GAME
→ 스타팅 성향 질문
→ starter candidate 결정
→ PokemonInstance 생성
→ Party slot 0 배치
→ Desk Pet으로 지정
→ Pokédex caught 등록
→ 첫 지역 해금
→ 최초 save
```

질문별 점수와 Pokémon 매핑은 콘텐츠 데이터로 분리해 사용자가 정답을 미리 알지 않게 할 수 있다.

---

# 22. Asset 저장 문제

세이브 데이터는 매우 작다.

반면 전체 1,025종의 여러 애니메이션은 내장 Flash에서 가장 큰 병목이 될 수 있다.

예를 들어 40×56 1-bit 프레임 하나는 압축 전 약 280 bytes다.

6-frame 애니메이션 하나만 해도 약 1.7 KB이며, 이를 1,025종에 적용하면 한 종류의 애니메이션만으로도 약 1.7 MB가 된다.

Idle / Walk / Attack / Sleep 등을 모두 넣으면 내장 Flash만으로는 장기적으로 부족할 가능성이 높다.

따라서 게임 코어에서 asset 로딩을 직접 Flash 주소에 고정하지 말고 다음과 같은 추상 경계를 둔다.

```text
AssetProvider
→ getSprite(species, form, animation, frame)
```

Vertical Slice:
- 현재처럼 compile-time local bitmap 사용 가능

전체 콘텐츠 단계:
- 압축 asset pack
- 외부 저장장치(예: microSD 모듈)
- 더 큰 Flash를 가진 보드

중 하나를 선택할 수 있게 만든다.

중요:

> 지금 당장 추가 하드웨어를 구매할 필요는 없다. Vertical Slice는 현재 장비만으로 충분하다. 다만 1,025종 전체 asset을 넣기 전에 저장 전략을 반드시 확정한다.

---

# 23. PC 포팅을 위한 경계

게임 코어 타입은 다음을 직접 include하지 않는다.

- `Arduino.h`
- `Adafruit_SSD1306.h`
- `Preferences.h`
- `Wire.h`

가능하면 표준 타입만 사용:

```cpp
#include <cstdint>
#include <array>
```

플랫폼 전용 역할:

```text
ESP32
- 화면
- 버튼
- 파일/Flash
- 시간
- RNG

PC
- Window/UI
- Keyboard
- File system
- Desktop clock
- RNG
```

배틀/탐험/포획/진화 규칙은 동일한 코어를 사용한다.

---

# 24. 첫 구현 범위

다음 구현에서는 전체 Pokémon 데이터를 넣지 않는다.

### Phase G1 — Game State Foundation

구현:

- `PokemonSpecies` 최소 테스트 데이터
- `PokemonInstance`
- `PartyState`
- `PokedexState`
- `GameProgress`
- `GameSave`
- serializer/deserializer
- 최초 세이브 생성
- save/load
- GAME HOME에 실제 파트너 정보 표시
- STATUS 화면

테스트 species:

- Pikachu 하나만으로 시작해도 됨

아직 구현하지 않음:

- 실제 탐험
- 배틀
- 포획
- 전체 박스 UI
- 1,025종 데이터
- 스타팅 질문
- 대규모 asset

---

# 25. Phase G1 완료 조건

1. 새 게임 데이터가 없으면 테스트 파트너 생성
2. 파트너가 GAME HOME에 실제 데이터로 표시
3. STATUS에 species / Lv / HP / EXP / friendship 표시
4. 게임 상태 변경 후 재부팅해도 유지
5. 세이브 버전 필드 존재
6. corrupted save를 감지할 기반 존재
7. Smart Desk 기능에는 변화 없음
8. DESK/GAME 전환 유지
9. Pokémon 로컬 asset이 없어도 fallback 빌드 성공
10. ESP32 Huge APP 빌드 성공

---

## 최종 설계 요약

```text
Static Content
  PokemonSpecies / Move / Learnset / Evolution
          │
          ↓
Persistent State
  PokemonInstance / Party / Box / Pokedex / Progress
          │
          ↓
Runtime State
  Exploration / Encounter / Battle
```

```text
Pokemon species data ≠ Pokémon owned instance ≠ battle temporary state
```

이 경계를 유지하면:

- ESP32 메모리를 절약하고
- 세이브 호환성을 관리하며
- 1,025종으로 확장하고
- 향후 PC 버전으로 포팅하기 쉬워진다.
