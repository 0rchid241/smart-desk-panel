# Firmware modules

Arduino IDE에서 상위 `smart_desk_esp32.ino`를 열어 빌드합니다.
보드는 **ESP32 Dev Module**, Partition Scheme은 **Huge APP**을 사용합니다.
기존 라이브러리를 그대로 사용하며 Arduino가 `src/` 아래 C++ 파일을 컴파일합니다.

## 상태와 책임

| 모듈 | 소유 상태 / 역할 |
| --- | --- |
| `core/app_config.h` | 핀, OLED 크기, debounce/hold/동기화 간격, 일정 제한 |
| `core/app_types.h` | 화면·장치·버튼 타입, ScheduleEvent, 콜백 타입 |
| `hardware/displays` | OLED 객체 두 개, 두 번째 TwoWire 버스, 초기화와 참조 접근 |
| `hardware/buttons` | debounce와 LEFT+RIGHT chord 상태, 버튼 이벤트 생성 |
| `services/network_time` | Wi-Fi 연결·재연결 상태, NTP, 현재 시간 조회 |
| `services/calendar_service` | 일정 배열, 날짜/정렬 조회, HTTPS/JSON, NVS/캐시, 알림 판단 |
| `desk/desk_app` | HOME/CALENDAR/TIMER 화면, 페이지, 타이머, 공통 알림 상태·화면 |
| `game/game_app` | Pikachu 프레임, Desk Pet, 실제 GameState 기반 HOME/STATUS, 메뉴 입력 |
| `game/pokemon`, `game/game_state` | 플랫폼 독립적인 종/개체/파티/도감/진행 상태, 초기 데이터, 검증 |
| `game/save_data` | 플랫폼 독립적인 GameSave와 버전/CRC를 포함한 필드별 직렬화 |
| `game/save_storage` | ESP32 Preferences 어댑터, 게임 전용 A/B 저장 및 복구 |
| `game/exploration` | epoch 인자를 받는 탐험 세션 검증·시작·완료·남은 시간 계산 |
| `game/encounter` | 공개 테스트 조우 확정, 검증, 탐험 정보 기반 결정적 seed |
| `game/type`, `game/move`, `game/battle` | 플랫폼 독립 타입 상성, 기술 fixture, 영속 1:1 전투와 턴 계산 |

가변 상태는 각 `.cpp`의 익명 namespace 안에 둡니다. 일정 조회는 복사 대신
`const ScheduleEvent&`를 반환하며, 조회 인덱스와 참조는 동기화 이후 다시 얻습니다.
디스플레이 참조는 애플리케이션 수명 동안 유효합니다.
게임 모듈은 Desk나 Calendar를 참조하지 않습니다.

## 실행 순서

- 부팅: 버튼 → OLED → 부팅 메시지/Desk Pet → NVS와 캐시 → Wi-Fi/NTP
  → 연결 성공 시 Calendar 동기화 → 주기 타임스탬프 → HOME/DESK.
- 루프: 버튼 → 재연결/NTP → 복구 시 Calendar 동기화 → 주기 동기화
  → 일정 알림 → 타이머 → 화면 → 애니메이션 → 기존 20ms 지연.
- 모드 전환을 먼저 처리한 후, 알림이 있으면 OK로 닫습니다. 그 외 입력만
  현재 모드의 앱으로 보냅니다. LEFT/RIGHT는 뗄 때, OK는 누를 때 처리합니다.
- GAME 모드의 Desk 알림은 진입점이 OLED 2를 Desk 모듈에 빌려 표시합니다.
  타이머와 일정 알림은 두 모드에서 모두 계속 동작합니다.
- NetworkTime은 부팅 메시지 콜백을, CalendarService는 알림 콜백을 받습니다.
  콜백은 동기적으로 호출되며 서비스가 화면 모듈을 직접 참조하지 않습니다.

## 유지한 저장/애셋 계약

- Reminder: NVS `schedule`, 기존 FNV-1a identity 및 `r%08lX` 키, UChar 플래그.
- Calendar cache: NVS `calcache`, `json` 키, 마지막 성공 응답 문자열.
- 15분 Calendar 동기화, 30초 Wi-Fi 재시도, 12초 연결 timeout,
  30ms debounce, 1초 chord, 10초 테스트 타이머를 유지합니다.
- `wifi_secrets.h`는 기존 위치/내용 그대로 사용합니다.
- 게임 bitmap은 `game_app.cpp` 한 곳에서만 `__has_include`로 포함합니다.
  `local_game_assets/`가 없으면 로컬 그림 누락 안내 화면을 사용합니다.
- 한글 렌더러와 PROGMEM 폰트는 원본 파일을 그대로 사용합니다.

## 실제 장치 회귀 확인

2026-09-15 검증: 설치된 Arduino CLI / ESP32 core 3.3.11에서
`esp32:esp32:esp32:PartitionScheme=huge_app` 전체 컴파일·링크가 성공했습니다.

| 빌드 | Flash | 전역 RAM |
| --- | ---: | ---: |
| 로컬 Pikachu 애셋 포함 | 1,448,500 bytes | 51,512 bytes |
| 애셋 없는 별도 스케치 복사본 | 1,446,756 bytes | 51,504 bytes |

원본 44개 함수의 이동을 대조했습니다. 29개는 공백·주석을 제외하고 동일하고,
11개는 API/참조/인자 변경만 있으며, 초기화·루프·버튼 라우팅·모드 전환 4개는
분리된 실행 순서를 검토했습니다. ELF 심볼에서 폰트/bitmap과 OLED/버스 객체의
중복 정의가 없음을 확인했습니다. 한글 렌더러·폰트·Git 제외 규칙은 변경하지 않았습니다.
장치 업로드와 실기 동작 확인은 수행하지 않았습니다.

1. 온라인/오프라인 부팅, Wi-Fi 복구, Calendar 수동 데이터 변경 후 자동 동기화.
2. 캐시 일정 표시, 같은 일정의 재부팅 후 알림 중복 방지, D-3/D-1/D-DAY.
3. HOME/CALENDAR/TIMER 이동, Calendar 페이지와 긴 한글 제목 스크롤.
4. 10초 타이머 종료와 OK 알림 닫기(두 모드 모두).
5. LEFT/RIGHT 단독 입력, 짧은 동시 입력, 1초 이상 동시 입력,
   길게 누른 채 유지 및 한쪽씩 놓을 때 불필요한 화면 이동이 없는지 확인.
6. DESK의 OLED 1/2 역할, GAME의 그래픽/메뉴, 메뉴 순환과 OK 시 Serial 로그,
   알림 중 모드 전환과 알림을 닫은 뒤 화면 복귀.

## 기존 코드에서 확인했으나 수정하지 않은 사항

- HTTPS 요청은 `setInsecure()`로 인증서 검증을 생략합니다.
- 날짜 파서는 일자를 1~31로만 제한하여 월별 말일을 검증하지 않습니다.
- `ok=true`인 응답의 `events`가 배열인지 별도로 검증하지 않아,
  해당 필드가 없거나 잘못된 타입이면 현재 목록이 비워질 수 있습니다.

이번 변경에서는 기존 동작 보존을 위해 위 로직도 그대로 옮겼습니다.

## Phase G1 — Game State Foundation

게임 관련 파일만 확장합니다. 메인 스케치, Desk, 버튼/chord, 디스플레이 핀,
네트워크/Calendar, 한글 렌더러 및 애셋 파일은 변경하지 않습니다.

### 데이터와 초기화

- `PokemonSpecies`: Pikachu 한 종의 이름/기본 능력치만 갖는 정적 테스트 데이터.
- `PokemonInstance`: 32-bit instanceId, speciesId, level, EXP, currentHp,
  gender, formId, shiny, friendship, 최대 4개 moveId.
- `GameState`: 최대 3개체의 PartyState, GameProgress, PokedexState.
  지정 파트너는 `progress.deskPetId`로 파티에서 조회합니다. Box는 아직 없습니다.
- PokedexState는 seen/caught/shinyCaught 각각 129바이트입니다.
  speciesId 1은 bit 0, 1025는 마지막 바이트 bit 0에 대응합니다.
  도감 저장 용량만 준비하며 1,025종 콘텐츠나 도감 UI는 추가하지 않습니다.
- GameProgress에는 nextInstanceId, deskPetId, ballTier, masterBallCount,
  playTimeSeconds를 둡니다. 시간 누적/등급 해금 시스템은 아직 구현하지 않습니다.
- 기존 `GameApp::init()`에서 Desk Pet을 그린 뒤 세이브를 읽습니다.
  없거나 두 슬롯 모두 손상되었으면 테스트 파트너를 생성하고 즉시 최초 저장합니다.
- 초기값: instanceId 1 / nextInstanceId 2, speciesId 25, Lv.5, EXP 0,
  HP 18/18, Friendship 70, 기본 폼, 수컷, 일반 색상, moveId 84/45/0/0.
  파티 첫 슬롯/Desk Pet으로 지정하고 도감 seen/caught를 등록합니다.
  이는 G1 fixture이며 스타팅 질문·첫 지역 해금은 포함하지 않습니다.
- HP = floor(2 × baseHp × level / 100) + level + 10;
  나머지 능력치 = floor(2 × baseStat × level / 100) + 5.
  IV/EV/성격 보정과 최종 능력치 저장은 없습니다. EXP는 현재 레벨 내 진행값으로
  두며 실제 EXP 지급/레벨업 규칙은 후속 단계에서 구현합니다.

### 저장 계약

- 게임 전용 NVS namespace `pokemon_g1`, 키 `save_a`/`save_b`만 사용합니다.
  Calendar의 `schedule`/`calcache`를 읽거나 수정하지 않습니다.
- `GameSave` = saveVersion + sequence + GameState. 버전 1 레코드는
  작은 정수를 명시적인 little-endian 순서로 기록합니다. struct raw write는 없습니다.
- 18바이트 헤더: `PKDG`(4), saveVersion(2), sequence(4), payloadLength(4), CRC32(4).
  CRC32/IEEE는 CRC 필드 자체를 제외한 헤더와 payload를 보호합니다.
- payload: GameProgress(14), party count(1), 사용 중인 파티 개체(count × 24),
  도감 bitset(387). 최초 1마리 세이브는 **444바이트**, 3마리는 **492바이트**입니다.
- 개체 레코드 24바이트: instanceId(4), speciesId(2), exp(4), currentHp(2),
  moves(8), level(1), friendship(1), formId(1), flags(1).
  flags의 bit 0~1은 성별(0/1/2), bit 2는 shiny이고 나머지는 예약입니다.
- 저장 시 비활성 슬롯에 sequence+1로 쓰고 바이트 수와 readback을 확인합니다.
  성공할 때만 활성 슬롯/sequence를 변경합니다. 부팅 시 CRC와 상태 검증을 통과한
  슬롯 중 높은 sequence를 선택합니다. 최신 슬롯이 손상되면 이전 슬롯을 사용합니다.
  sequence가 uint32 최대치이면 wrap 대신 저장 실패를 반환합니다.
- 현재 버전과 다른 데이터 또는 지원 크기를 초과하는 레코드는 보존합니다.
  저장소 열기/읽기 오류도 기존 데이터를 덮어쓰지 않고 RAM 테스트 파트너로 동작합니다.
  이러한 경우 HOME/STATUS에 `SAVE ERROR`, Serial에 원인을 표시합니다.
- 최초 데이터 생성 이외에 G1 UI는 영구 상태를 변경하지 않습니다.
  후속 기능은 `GameApp::state()`를 복사하여 수정한 다음 `GameApp::saveState(next)`를
  호출합니다. 검증·저장 성공 시에만 실행 중 상태를 교체하며 실패하면 기존 상태를 유지합니다.
  화면 그리기/STATUS 조회/모드 전환마다 저장하지 않습니다.

### 화면

OLED 1/Pikachu Idle 및 DESK Pet은 기존 구현 그대로입니다.
GAME HOME은 실제 파트너의 이름, Lv, 현재/최대 HP와 기존 단일 선택 메뉴
`STATUS / EXPLORE / POKEDEX`를 표시합니다.
STATUS에서 이름/Lv/HP/EXP/Friendship을 한 화면에 표시하고 OK로 HOME에 돌아갑니다.
STATUS의 LEFT/RIGHT는 동작하지 않습니다. EXPLORE/POKEDEX는 기존 Serial 선택 로그만
남깁니다. Desk 알림 중 게임 화면이 가려졌다가 알림을 닫으면 현재 게임 화면으로 돌아옵니다.

### G1 검증과 실기 체크

2026-09-16: 설치된 Arduino CLI / ESP32 core 3.3.11,
`esp32:esp32:esp32:PartitionScheme=huge_app` 전체 컴파일·링크 성공.

| G1 빌드 | Flash | 전역 RAM |
| --- | ---: | ---: |
| 로컬 Pikachu 애셋 포함 | 1,454,204 bytes (46%) | 52,040 bytes (15%) |
| 로컬 애셋 없는 복사본 | 1,452,472 bytes (46%) | 52,032 bytes (15%) |

두 빌드의 게임 소스가 최종 소스와 동일함을 확인했고, fallback 의존 파일에는
Pokémon bitmap 헤더가 포함되지 않았습니다. 장치 업로드는 수행하지 않았습니다.

PC 테스트: 저장소 루트에서 `./firmware/tests/game_state/run.ps1` 실행(MSVC 필요).
실제 게임 코어/codec/저장 어댑터를 빌드하며 Preferences만 메모리 fake로 대체합니다.
초기값, ID/도감/HP 검증, 전체 필드 왕복, 단일 비트 손상/절단/잘못된 버전,
변경 데이터 재로드, A/B 복구, 부분 쓰기/readback 실패, NVS 오류와 namespace 격리를 검사합니다.
MSVC C++17 `/W4 /WX` 빌드와 모든 테스트가 통과했습니다.
이 테스트는 실제 플래시의 전원 차단 검증을 대신하지 않습니다.

실기 확인:

1. 첫 부팅 Serial의 `Game: first save`/`Game save created`, GAME HOME의 Pikachu/Lv.5/18/18.
2. 재부팅 후 `Game save loaded`; 같은 파트너와 STATUS의 EXP 0/Friendship 70.
3. STATUS 진입/OK 복귀, 메뉴 순환, STATUS 중 DESK/GAME 전환과 알림 표시/닫기.
4. 기존 Desk Pet 애니메이션, 타이머, 한글 일정, 캐시/재연결 및 Calendar 알림 중복 방지.
5. 상태 변경 보존은 후속 기능 또는 별도 테스트 코드에서 state 복사본의 EXP/HP/friendship을
   수정한 뒤 `saveState()` 성공을 확인하고 재부팅하여 점검합니다. G1에는 테스트용 능력치 변경 UI를 넣지 않았습니다.
6. NVS 손상/전원 차단 회귀는 별도 테스트 장치에서 게임 namespace에 한정해 수행합니다.
   단일 슬롯 손상 시 다른 슬롯 로드, 양쪽 손상 시 초기 파트너 재생성,
   저장 불가 시 SAVE ERROR와 기존 Calendar 데이터 보존을 확인합니다.

다음 단계에서는 짧은 탐험의 런타임 상태와 안전한 저장 시점을 정의합니다.
배틀/조우/포획/전체 도감/박스/진화/지역 콘텐츠는 G1에 포함하지 않습니다.

## Phase G2 — Exploration Foundation

### 탐험과 시간

- `GameState::exploration`은 status(Idle/Exploring/Complete), regionId(uint16),
  startedAtEpoch(uint64), durationSeconds(uint32)를 보존합니다.
  공개 fixture는 ID 1의 **테스트 초원**, 15초 탐험 하나뿐입니다.
- `exploration.*`는 플랫폼 API를 호출하지 않고 전달받은 UTC epoch로만 계산합니다.
  0 또는 1700000000 미만은 시간 미확보입니다. 유효하지 않은 시작, 0초 기간,
  종료 시각 overflow를 거부합니다. 시계가 시작 시각보다 뒤로 갔으면 완료하지 않고 기다립니다.
- `NetworkTime::getCurrentEpoch()`는 기존 시간 유효성 기준으로 `time()`만 읽습니다.
  Wi-Fi 연결/재시도/NTP 설정 로직은 바꾸지 않습니다. 인터넷 연결 자체가 아니라
  유효한 시계가 기준이므로, NTP 동기화 이후 Wi-Fi가 끊겨도 탐험은 계속됩니다.
- 메인 루프는 두 모드 모두 `GameApp::update()`를 호출합니다. 이 함수는 세션과
  저장만 갱신하며 OLED를 그리거나 Smart Desk 알림을 생성하지 않습니다.
- 전원 차단 후 Exploring을 복원하고, 시간이 없으면 시작 시각/기간을 그대로 유지합니다.
  NTP 복구 후 종료 시각이 지났다면 Complete로 저장합니다. 이미 Complete인 세션은
  인터넷 없이도 결과를 확인할 수 있습니다. 탐험에는 `millis()`를 사용하지 않습니다.
  이는 G1 설계서의 '탐험 취소 가능' 선택지보다 우선하는 G2의 영속 세션 요구사항입니다.

### 화면과 저장 시점

- `GameScreen`: Home, Status, RegionSelect, Exploring, ExplorationComplete.
- HOME의 탐험 → 지역 선택 → OK 시작 → 진행/남은 시간 → 완료 → OK 확인 → HOME.
  지역 선택에서 L/R로 테스트 초원과 돌아가기를 선택합니다. 돌아가기는 지역이 아닌
  복귀 항목입니다. 실제 지역 테이블이나 해금 조건은 없습니다.
- 한글 이름/메뉴/설명은 기존 renderer를 사용합니다. 16px 한글을 탐험 화면의
  y=0/24/48에 표시합니다. 시간 미확보 시 '시간 동기화 중...' 또는 '시간 확인 중...'를
  표시합니다. 60초 이상 표시는 분:초 형식도 지원합니다.
- OLED 1과 DESK Pet은 기존 Pikachu Idle을 재사용합니다. 신규 그래픽은 없습니다.
  애셋 없는 fallback 안내도 기존 한글 렌더러로 '포켓몬 / 로컬 그림 / 없음'을 표시합니다.
  STATUS, 버튼 debounce/chord, 알림 우선순위는 유지합니다.
- 영속 전이는 시작(Idle→Exploring), 최초 완료(Exploring→Complete),
  결과 확인(Complete→Idle)에서만 저장합니다. 초기 생성/버전 이전 저장은 별도입니다.
- 상태 복사본을 저장한 후 성공 시에만 적용합니다. 시작/확인 저장 실패 시 현재 화면과
  세션을 유지하며 OK로 재시도합니다. 자동 완료 저장 실패 시에는 시도 latch를 세워
  이후 루프의 자동 저장을 막고 '저장 오류 / OK 재시도'를 표시합니다.
  재부팅은 새 복구 시도로 취급됩니다. 정상 Complete/Idle 반복 업데이트는 저장하지 않습니다.

### G1 → G2 세이브 이전

- namespace는 **`pokemon_g1` 그대로**, 키도 `save_a`/`save_b` 그대로 유지합니다.
- version 2는 G1 payload 뒤에 탐험 15바이트를 추가합니다:
  status(1), regionId(2), startedAtEpoch(8), durationSeconds(4), 모두 little-endian.
  최초 1마리 기준 459바이트, 최대 3마리 507바이트입니다. CRC 범위/헤더는 동일합니다.
- decoder는 v1/v2를 모두 읽습니다. v1은 기존 파티/진행/도감을 보존하고 탐험만 Idle로
  채웁니다. 부팅 시 가장 높은 정상 sequence를 선택한 뒤 비활성 슬롯에 v2로 씁니다.
  이전 성공 시에만 버전/sequence를 전진합니다. 실패하면 읽은 G1 데이터와 원래 슬롯을
  유지하며, 다음 정상 게임 저장 또는 재부팅 시 이전할 수 있습니다.
- A/B 검증, CRC32, readback, 미지원 미래 버전 보호, Calendar namespace 격리는 유지합니다.
  G2 저장 후 G1 펌웨어로의 다운그레이드는 지원하지 않습니다.

### 검증 / 실기 확인 / G3 연결점

- 기존 `firmware/tests/game_state/run.ps1`은 코어/NVS 회귀와 탐험/마이그레이션 테스트,
  실제 game_app을 host display/time/NVS fake에 연결한 테스트를 실행합니다.
  정상 전이, 남은 시간, 64-bit epoch, offline/rollback, 중복 완료 방지, 손상/부분 저장,
  기존 3마리 G1 레코드 보존, 혼합 버전 A/B 복구를 검사합니다.
- 앱 테스트는 시간 없이 시작 금지, 돌아가기, 재부팅 후 탐험 복원, background update의
  OLED 비간섭, 자동 저장 실패 후 반복 쓰기 방지, OK 재시도, 결과 확인 후 Idle 저장을 검사합니다.
  새 탐험 화면의 한글 행 높이와 폭도 확인합니다. 실제 장치의 전원 차단 검증은 별도입니다.
- 실기: G1 데이터 유지/이전 로그 → 15초 탐험 → 완료/확인 → 재부팅 Idle 확인;
  탐험 중 DESK 사용 후 GAME 복귀; 탐험 중 전원 OFF 뒤 오프라인 부팅/시간 대기/NTP 복구;
  완료 상태 재부팅; STATUS와 Calendar/타이머 알림 중 게임 화면 복귀를 확인합니다.
  저장 실패/전원 차단 테스트는 게임 namespace만 사용하는 별도 테스트 장치에서 진행합니다.
- 완료 화면은 '탐험을 마쳤다.'까지만 표시합니다. G3 연결점은 저장된
  `ExplorationStatus::Complete`입니다. 조우/종 선택/보상/EXP/배틀/포획은 구현하지 않았습니다.
- 기존 시간 API는 실제 NTP 성공 이력 대신 epoch 하한으로 유효성을 판단하고,
  기존 NTP/Calendar 동기화는 동기 호출입니다. 해당 동작은 이번 단계에서 변경하지 않았습니다.

## Phase G4 — Battle Vertical Slice

G3의 탐험/야생 조우에 실제 1:1 전투를 연결합니다. G4 요구사항에 따라 기존 데이터
모델 문서의 선택적 전투 취소 정책보다 **전투 재부팅 복원**을 우선합니다.

### 코어와 상태 전이

- `type.*`: None + 18타입. 상성은 4=1배의 정수(0/1/2/4/8/16)이며,
  잘못된 타입/중복 방어 타입은 0을 반환합니다. 두 번째 None은 단일 타입입니다.
- `move.*`: 84 전기쇼크(특수 40/100/30), 45 울음소리(변화 0/100/40),
  33 몸통박치기(물리 40/100/35), 21 힘껏치기(물리 80/75/20) fixture.
  숫자는 위력/명중률/PP입니다. 21은 명중 테스트에도 사용하며 초기 파트너의 84/45는 유지합니다.
  울음소리는 PP와 턴만 소비하고 능력치 랭크/상태이상은 적용하지 않습니다.
- `BattleState`: None/Active/Won/Lost, playerId, 야생 종/폼/레벨/성별/shiny,
  야생 기술 4개, 양쪽 HP/PP, turn, rngState. 영구 파티 개체와 전투 HP/PP는 분리합니다.
  플레이어 종/레벨/기술은 playerId가 가리키는 partner에서 읽습니다.
- 야생 기술은 종별 fixture로 확정합니다. 야생 개체에는 영구 instanceId를 배정하지 않습니다.
- 조우 OK는 candidate에서 Battle 생성 + Exploration Idle + Encounter None을 함께 저장합니다.
  실패하면 기존 조우가 유지됩니다. HP가 이미 0인 구세이브 파트너는 즉시 Lost로 들어가
  결과 확인으로 정상 회복할 수 있습니다.
- 턴: 사용 가능한 기술 선택 → 야생 공격 기술 우선 선택 → Speed 순서(동률 RNG) →
  선공 → 기절 검사 → 살아 있으면 후공 → turn 증가 → 한 번 저장.
  HP/PP/RNG/turn은 성공 시에만 RAM에 반영합니다. update/모드 전환/그리기는 턴을 진행하지 않습니다.
- xorshift32 상태를 저장합니다. 초기 seed는 탐험 hash, partner ID, 야생 정보 조합이며
  0은 고정 비영 seed로 대체합니다. 명중, 피해량 85~100%, 동률 순서에 같은 RNG를 사용합니다.
- 데미지는 `((2*Lv/5+2)*Power*Attack/Defense/50+2)`를 기반으로 STAB 1.5배,
  타입 상성, 난수 배율을 적용합니다. 물리/특수 능력치를 구분하고 uint64 중간값을 사용합니다.
  무효 상성은 0, 그 외 최소 1, 최종 uint16 범위를 초과하면 포화합니다.
- PP 0 기술은 건너뛰며 모두 소진하면 무속성 발버둥(위력 50/명중 100, 반동 없음)을 사용합니다.
  턴 카운터는 uint32 최대치에서 포화하여 overflow나 진행 불능을 피합니다.
- Won은 야생 HP=0/플레이어 생존/turn>0, Lost는 플레이어 HP=0/야생 생존입니다.
  결과 OK는 partner를 최대 HP로 회복하고 Battle None을 한 번 저장합니다. 실패 시 결과 유지.
  영구 개체의 EXP/레벨/친밀도/도감에 전투 보상을 적용하지 않습니다.
- None 전투는 모든 필드가 canonical default입니다. 전투가 있으면 탐험 Idle/조우 None만
  허용하며, player ID, 종/폼/레벨/성별, HP 범위, move/PP, RNG 비영 값을 검증합니다.
  구버전 비전투 상태 조합은 이전 규칙을 유지하여 마이그레이션 중 데이터를 잃지 않습니다.

### v4 wire format / 이전

namespace `pokemon_g1`, `save_a`/`save_b`, 18바이트 헤더, CRC32, A/B readback은 유지합니다.
G3 payload 뒤에 **40바이트 Battle record**를 추가합니다. 전부 명시적 little-endian입니다.

| Battle offset | 필드 | bytes |
| --- | --- | ---: |
| 0 | BattleStatus | 1 |
| 1 | playerId | 4 |
| 5 | wild status/speciesId/formId/level/gender/shiny | 7 |
| 12 | wildMoves[4] | 8 |
| 20 | player currentHp + pp[4] | 6 |
| 26 | opponent currentHp + pp[4] | 6 |
| 32 | turn | 4 |
| 36 | rngState | 4 |

총 레코드 크기는 1마리 **506바이트**, 3마리 **554바이트**입니다.
shiny bool은 0/1 외 값도 CRC가 맞더라도 거부합니다.

- v1: 파티/진행/도감 유지, 탐험 Idle/조우 None/전투 None 추가.
- v2: 기존 탐험 유지, 조우 None/전투 None 추가.
- v3: 기존 탐험/조우 유지, 전투 None 추가.
- v4: 전투 HP/PP/turn/RNG/야생 정보를 그대로 복원.
- 이전 버전은 로드 후 비활성 슬롯에 v4를 저장하며 실패해도 원본과 RAM 상태를 유지합니다.
  지원하지 않는 미래 버전/읽기 실패 시 기존 데이터를 덮어쓰지 않는 정책도 동일합니다.

### OLED / 조작

복원 우선순위: Battle → Encounter Ready → 탐험 진행/완료 → Home.
전투 OLED 1은 상대를 오른쪽 위, partner를 왼쪽 아래에 표시합니다.
기존 48x48 야생 sprite가 있으면 24x24로 화면에서만 축소하며 원본은 변경하지 않습니다.
없으면 이름/YOU/WILD/위치 표시로 양 진영을 구분합니다.
OLED 2는 상대 이름/Lv, 상대 HP, YOU HP, 선택 기술, PP를 표시합니다.
한글 y=0/34의 16px 행과 ASCII y=16/24/56의 8px 행으로 구성합니다.
LEFT/RIGHT는 사용 가능한 기술 순환, OK는 턴 실행입니다. 별도 메시지 대기 없이
HP/PP 갱신 후 선택으로 복귀하고 기절 시 `전투 승리!`/`쓰러졌다...`를 표시합니다.
저장 실패는 오류/OK 재시도로 알리며 결과 화면에서 OK를 누르면 Home으로 돌아갑니다.
알림 우선순위/버튼 chord/핀은 진입점의 기존 라우팅을 사용합니다.
전투/조우 정적 그래픽 제한은 GAME에서만 적용하여 DESK Pet 애니메이션은 계속됩니다.

### 자동 검증과 실기 확인

`powershell -ExecutionPolicy Bypass -File .\firmware\tests\game_state\run.ps1`

MSVC C++17 `/W4 /WX`로 기존 코어/storage/탐험/조우 회귀, 독립 v1/v2/v3 레코드 이전,
상성/물리/특수/STAB/명중과 빗나감/PP/선공 기절/동률/Won/Lost/전투 왕복을 검사합니다.
실제 GameApp에서는 조우 시작/턴/결과 확인의 저장 실패, 재부팅, 버튼과 화면 복구를 검사합니다.
애셋 없는 소스와 저작물 없는 합성 도형 fixture 소스를 별도로 빌드하여
ASCII/한글/pixel 경계 및 조우/전투 중 DESK 애니메이션을 검증합니다.

실기 순서:

1. ESP32 Dev Module / Huge APP으로 컴파일 후 사용자가 업로드합니다(NVS 지우기 금지).
2. 기존 파트너/STATUS가 유지되고 이전 저장이 v4로 로드되는지 확인합니다.
3. GAME → 탐험 → 테스트 초원 → 15초 → 조우 → OK로 전투에 진입합니다.
4. L/R로 전기쇼크/울음소리를 선택하고 OK로 HP/PP가 갱신되는지 확인합니다.
5. Active 중 재부팅하여 GAME 복귀 후 상대/HP/PP가 같은지 확인합니다.
6. 전투 중 DESK 전환, 펫/타이머 사용, 알림 닫기, GAME 복귀를 확인합니다.
7. 전기쇼크로 승리하거나 울음소리를 반복해 패배한 뒤 결과 화면에서 재부팅합니다.
8. 동일 결과 복원 → OK → Home/HP 완전 회복 → 재탐험 시 PP 초기화를 확인합니다.

G5 이후 범위: 포획, EXP/레벨업, 진화, 전체 콘텐츠, 복잡한 상태이상/기술 효과.

### G4 실행 결과 (2026-09-16)

- 위 PowerShell 회귀 명령: 코어, 저장, 탐험, 조우, v1/v2/v3 이전,
  전투, fallback GameApp, 합성 애셋 GameApp 모두 PASS. MSVC `/W4 /WX` 통과.
- 설치된 Arduino CLI: `C:/Users/first/AppData/Local/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe`.
- 실제 전체 컴파일/링크 명령(저장소 루트, PowerShell):

```powershell
& 'C:/Users/first/AppData/Local/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe' compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app --build-path C:/smart-desk-panel/build/firmware-g4 firmware/smart_desk_esp32
& 'C:/Users/first/AppData/Local/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe' compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app --build-path C:/smart-desk-panel/build/firmware-g4-fallback build/g4-fallback-source/smart_desk_esp32
```

| 최종 빌드 | Flash | 전역 RAM | 결과 |
| --- | ---: | ---: | --- |
| 로컬 애셋 포함 | 1,465,596 bytes / 3,145,728 (46%) | 52,152 bytes (15%) | PASS |
| 애셋 제외 복사본 | 1,462,688 bytes / 3,145,728 (46%) | 52,144 bytes (15%) | PASS |

fallback은 ignored `build/` 아래 별도 소스 복사본에서 `local_game_assets`를 제외했습니다.
두 컴파일 소스 snapshot을 최종 게임 소스와 대조했고, fallback의 컴파일 의존 파일에도
로컬 애셋 헤더가 없음을 확인했습니다. `git diff --check` 통과. 업로드/실기 검증은 미실행.

## Phase G4.1 — 전투 UI / 피드백

SAVE_VERSION=4와 BattleState/codec/storage는 그대로 유지합니다.

- 그래픽 OLED: 상대는 오른쪽 위, 파트너는 왼쪽 아래. YOU/WILD/VS 대신 각각
  이름(16px), Lv(ASCII), 현재/최대 HP(ASCII)를 표시합니다. Lv.100과 양쪽 3자리 HP를 검증했습니다.
- 텍스트 OLED: 제목 y=0, 선택 기술 y=18, 다음 사용 가능 기술 y=36, PP/조작 y=56.
  이름/Lv/HP 반복은 제거했습니다. 4기술 순환, PP 0 건너뛰기, 발버둥은 동일합니다.
- `BattleTurnReport`는 실행 순서대로 최대 2개의 `BattleActionReport`를 제공합니다.
  actor, MoveId(0=발버둥), hit, 실제 HP 감소량, 상성(4=1배), faint를 담습니다.
  선공 기절 시 1개만 기록합니다. optional output이므로 기존 호출은 그대로 사용할 수 있습니다.
- 코어 결과를 candidate에서 계산하고 저장에 성공한 뒤에만 UI report를 적용합니다.
  저장 실패 시 상태/메시지/피격 효과는 적용하지 않습니다. 보고서 읽기는 난수를 소비하지 않습니다.
- 행동당 이름/기술/피해량 또는 빗나감/변화 기술을 한 화면에 표시합니다.
  OK로 다음 행동, 마지막 OK로 기술 선택 또는 승패 화면에 돌아갑니다(턴당 최대 2번).
  메시지 중 LEFT/RIGHT는 무시하며 메시지 확인은 저장/다음 턴 실행을 유발하지 않습니다.
- 실제 피해가 있는 행동만 피격 대상 sprite/fallback을 300ms 동안 75ms 간격으로 깜박입니다.
  millis/graphicsDirty 처리는 GameApp에만 있고, MODE_GAME에서만 효과를 갱신합니다.
  DESK Pet, 알림 라우팅, 버튼 chord는 유지합니다.
- 48x48→24x24 축소는 2x2 중 흰 픽셀 3개 이상일 때 켜는 과반수 방식입니다.
  OR 방식의 고립 픽셀 확장을 억제합니다. 실제 로컬 그림으로 비교했으며 원본 애셋과
  애셋 pipeline은 변경하지 않았습니다. 추가 framebuffer도 없습니다.
- report/메시지/효과는 저장하지 않습니다. 메시지 중 재부팅은 이미 저장된 Active의
  기술 선택 또는 Won/Lost 결과 화면으로 복귀합니다. 결과 확인/회복/저장 실패 재시도는 동일합니다.

검증(2026-09-16):

- `powershell -ExecutionPolicy Bypass -File .\firmware\tests\game_state\run.ps1`: 전체 PASS.
- 기존 회귀 + report의 행동 순서/명중/피해량/기절/변화 기술, 4기술/PP 0,
  메시지/저장 실패/재부팅, 양 진영 피격/종료/모드 격리, 좌표 경계를 검증했습니다.
- 애셋 경로는 저작물이 아닌 합성 2x2 패턴(흰 픽셀 1/2/3/4개)을 사용하여
  OR로 회귀하면 실패하는 정확한 출력 픽셀 수/위치를 검사합니다. fallback도 별도 실행합니다.
- 별도 일회성 비교: 커밋 f281693의 원래 턴 함수와 2,880개 조건을 비교하여
  전체 v4 직렬화 결과가 일치했습니다(종 3 × 레벨 3 × seed 64 × 기술/발버둥 5).
- G4와 동일한 Arduino CLI / Huge APP 명령 및 빌드 경로를 재사용했습니다.

| G4.1 최종 빌드 | Flash | 전역 RAM | 결과 |
| --- | ---: | ---: | --- |
| 로컬 애셋 포함 | 1,466,528 bytes (46%) | 52,176 bytes (15%) | PASS |
| 애셋 없는 fallback | 1,463,620 bytes (46%) | 52,168 bytes (15%) | PASS |

실기: 양쪽 이름/Lv/HP 배치 → 기술 두 줄과 PP → OK 후 행동/피격/피해량 →
OK로 상대 행동 → 선택 복귀를 확인합니다. 메시지 중 DESK/GAME 전환 및 재부팅,
승패 결과 재부팅/확인/HP 회복도 확인합니다. 업로드/commit/push는 수행하지 않았습니다.

## Phase G5-C1 — 포획 개체의 파티 소유 반영

기준 main: `0a1e54ccddeb9d4aba7d316048e2bee95f3d0589`.
Box 없이 빈 파티 슬롯이 있을 때만 포획을 허용합니다.

- `attemptBattleCapture()`의 성공 후보 안에서 nextInstanceId를 새 ID로 발급하고 1 증가,
  파티 마지막 슬롯에 개체 추가, 도감 등록, 마스터볼 소비, Battle None 처리를 함께 수행합니다.
- 새 개체는 야생 species/form/level/gender/shiny, 실제 wildMoves 4개와 포획 순간 HP를
  복사합니다. EXP/친밀도는 0입니다. 계산용 wildPokemon의 임시 ID는 사용하지 않습니다.
- 기존 파티 순서와 deskPetId는 유지합니다. 중복 종 포획을 허용하고 각 ID는 고유합니다.
  일반 포획은 shinyCaught를 켜지 않으며 기존 shiny 등록도 지우지 않습니다.
- `canUseCaptureBall()`에서 파티 3마리 또는 nextInstanceId=UINT32_MAX를 차단합니다.
  나머지 비정상 ID/상태는 기존 isValidState 검증이 거부합니다.
  마지막 발급 가능한 ID는 UINT32_MAX-1이며 wrap하지 않습니다.
- UI의 차단 안내는 transient입니다. `파티가 가득 찼다 / 박스 준비 중` 또는
  `포획 불가 / 개체 ID 부족`을 표시하고 OK로 볼 선택에 돌아갑니다.
  이 경로에서는 RNG/볼/반격/Battle/NVS를 변경하지 않습니다.
- GameApp은 기존 candidate → core → saveState → live 적용 순서를 유지합니다.
  저장 실패 시 소유/ID/도감/마스터볼/전투/RNG가 모두 유지되며 성공 연출도 시작하지 않습니다.
- 성공 저장 후 기존 3회 흔들림(350ms 도입 + 3×420ms + 250ms 정지)을 그대로 재생합니다.
  성공 화면은 종 이름 / 포획 성공! / 파티에 합류! / OK를 표시합니다.
  OK는 추가 저장 없이 Home으로 돌아갑니다. 연출 중 재부팅해도 새 소유는 유지되며 Home으로 복원합니다.
- SAVE_VERSION=4, POKEMON_RECORD_SIZE=24, BATTLE_RECORD_SIZE=40, PARTY_CAPACITY=3,
  namespace `pokemon_g1`과 v1/v2/v3 이전 규칙은 변경하지 않습니다.
  Box/파티 UI/파트너 변경/성장/포획 확률 및 흔들림 그래픽은 이번 범위가 아닙니다.

검증:

`powershell -ExecutionPolicy Bypass -File .\firmware\tests\game_state\run.ps1`

MSVC `/W4 /WX` 전체 PASS. 신규 소유 필드/HP/moves/ID, 중복 종과 shiny 도감,
가득 찬 파티의 모든 볼 거부, ID 한계, v4 왕복, 저장 실패/정확히 한 번 재시도,
성공 연출 중 재부팅, 기존 3회 흔들림 시간/좌우 위치를 검사합니다.
G1~G4, 도망, 포획 확률/실패 반격, v1~v3 이전, fallback/합성 애셋 앱 회귀도 유지합니다.

실기: 파티에 빈자리가 있는 상태에서 포획 → 3회 흔들림/파티 합류 → OK → 기존 파트너 Home;
성공 연출 중 재부팅 후 Home; 두 마리 추가 포획으로 파티 3마리 → 다음 전투에서
일반/마스터볼 시도 시 파티 가득 참 안내 → OK로 볼 선택 복귀를 확인합니다.
마스터볼이 있는 세이브에서는 성공 때만 1개 줄고 차단 시에는 유지되는지도 확인합니다.
파티/개체 상세 UI는 C1에 추가하지 않으므로 개별 필드 보존은 자동 테스트로 검증합니다.

ESP32 core 3.3.11 / ESP32 Dev Module / Huge APP 최종 전체 컴파일·링크 PASS:

| G5-C1 빌드 | Flash | 전역 RAM |
| --- | ---: | ---: |
| 로컬 애셋 포함 | 1,471,516 bytes (46%) | 52,208 bytes (15%) |
| 애셋 없는 fallback | 1,468,600 bytes (46%) | 52,200 bytes (15%) |

앞서 기록한 Arduino CLI 명령과 `build/firmware-g4`, `build/firmware-g4-fallback` 경로를 재사용했습니다.
양쪽 컴파일 snapshot과 최종 게임 소스의 일치 및 fallback 애셋 의존성 부재를 확인했습니다.
`git diff --check` PASS. 업로드/commit/push는 수행하지 않았습니다.

## G5-C2-B1 — 독립 Box snapshot 저장 계층

B1은 UI/포획/GameState와 연결하지 않습니다. SAVE_VERSION=4, SAVE_MAX_SIZE=554,
`pokemon_g1/save_a/save_b`, 기존 migration과 NVS load/save는 그대로입니다.
GameApp은 mount/create를 호출하지 않으며 업로드 시 자동 포맷하지 않습니다.

### 파일 계약

- `box_data.*`: 플랫폼 독립 header/bitmap/mutation 타입.
- `box_storage.*`: 번들 Arduino LittleFS adapter. mount는 `begin(false)`.
- `pokemon_record_codec.*`: GameSave와 Box가 공유하는 기존 24-byte 표현.
- `crc32.h`: IEEE CRC32 incremental helper. GameSave CRC 범위/결과 유지.
- Box capacity=2048, formatVersion=1. Party 3슬롯과 별도.
- 경로 `/pokemon/box_<16 hex storeId>_<16 hex generation>.bin` (50문자).
  storeId/generation은 0이 아닌 uint64. 새로운 generation은 source보다 커야 합니다.
- 고정 길이 **49,472 bytes (48.3125 KiB)** = header 64 + bitmap 256 + records 49,152.
- Header는 explicit little-endian이며 struct memory를 쓰지 않습니다.

| offset | bytes | field |
| --- | ---: | --- |
| 0 | 4 | PKBX |
| 4 | 2 | formatVersion=1 |
| 6 | 2 | headerSize=64 |
| 8 | 2 | recordSize=24 |
| 10 | 2 | flags=0 |
| 12 | 4 | capacity=2048 |
| 16 | 4 | occupiedCount |
| 20 | 4 | bitmapBytes=256 |
| 24 | 8 | storeId |
| 32 | 8 | generation |
| 40 | 4 | payloadBytes=49408 |
| 44 | 4 | CRC32 |
| 48 | 16 | reserved=0 |

CRC는 bytes 44..47만 제외한 전체 파일을 보호합니다. slot offset은
`320 + slot * 24`. 빈 레코드는 모두 0이며 bitmap bit는 slot%8에 대응합니다.
`Snapshot::open`은 전체 길이/header/key/bitmap/count/CRC/개체 의미/중복 ID를
검증한 뒤에만 metadata와 레코드를 노출합니다. 현재 isValidPokemon의 fixture 종/폼/
기술 제한도 그대로 적용됩니다. Party 중복 및 도감 정합성은 아직 검사하지 않습니다.

### API와 수명

`mount/unmount`, `exists`, `createEmpty`, `validate`, `mutate`, `removeSnapshot`과
`Snapshot::open/close/metadata/occupied/readSlot/findEmpty/findInstance/findSpecies`를 제공합니다.
`findSpecies(species, startSlot, outputSlot)`을 startSlot=이전 결과+1로 반복하면 중복 종을
순차 조회할 수 있습니다. metadata는 성공한 open 이후에만 사용합니다.
조회 실패 시 출력 개체/metadata는 부분 적용하지 않습니다.

모든 호출은 직렬로 사용합니다. reader가 열린 동안 해당 파일 삭제/외부 수정이나 unmount를
하지 않습니다. 삭제 API는 명시적 요청만 수행하며 root 보호는 향후 상위 coordinator 책임입니다.
최신 generation 자동 선택, 자동 cleanup, 자동 format은 없습니다.

mutation은 Insert/Remove/Replace 한 슬롯만 바꿔 새 파일로 씁니다. source 전체 검증 후
쓰기 → flush/close → reopen → 전체 검증 및 예상 CRC/count 비교를 수행합니다.
기존 파일명 충돌은 거부하며 source는 읽기 전용입니다. 실패한 destination은 남을 수 있고,
재시도도 collision으로 거부합니다. 호출자가 validate/removeSnapshot으로 명시적으로 처리합니다.
Arduino flush/close는 오류 반환값이 없으므로 B1은 재오픈 검증까지만 보장합니다.
전원 차단 transaction/불확정 commit 해결은 향후 v5 root 연결 단계의 책임입니다.

### RAM과 검증

상주 Box 배열이나 검색 index는 없습니다. 열린 Snapshot마다 bitmap 256 bytes + metadata +
File handle을 가집니다. 작성 중에는 bitmap 256/header 64/record 24 bytes와 개체 한 개를 사용합니다.
중복 ID 검사는 임시 ID 64개(256 bytes)를 이용해 최대 32그룹의 순차 비교를 수행합니다.
고정 scratch만 사용하지만 채워진 Box 검증에는 추가 I/O가 필요합니다. 실기 최악 지연은 후속 측정 대상입니다.
LittleFS 내부 cache/heap과 함수 stack overhead는 별도입니다.

`firmware/tests/game_state/run.ps1`은 실제 box_storage.cpp를 메모리 LittleFS fake로 빌드하며
MSVC /W4 /WX를 유지합니다. empty/full 2048슬롯, insert/remove/replace, 원본 보존,
검색/경계/64-bit key, 중복 ID(그룹 내부/경계), header/bitmap/count/CRC/개체 손상,
truncation/trailing bytes, 부분 쓰기/flush 손상/close 절단/reopen 실패를 검증합니다.
`save_v4_golden.h`는 기준 2d699e2의 수정 전 save_data.cpp로 생성한 554-byte fixture이며,
공통 codec 적용 후 header/payload/CRC 전체가 동일한지 매번 검사합니다.

다음 단계는 v5 BoxRoot, 기존 저장의 migration/초기 filesystem 정책, main+Box 복구/commit
coordinator입니다. B1의 key/metadata/mutation API를 연결하되 NVS A/B 양쪽 root 보호와
Party+Box ID 정합성을 그 단계에서 추가해야 합니다.

## G5-C2-B2 — v5 main + BoxRoot 저장 쌍

B1 Box format/capacity/24-byte codec은 유지합니다. Box UI, 포획→Box, Party full 제한,
전투 계산은 변경하지 않습니다. SAVE_VERSION=5이고 GameState 바깥 GameSave에 BoxRoot를 둡니다.

### v5 wire

기존 v4 payload(Battle까지) 뒤에 32 bytes를 append합니다.
`SAVE_V4_MAX_SIZE=554`, `BOX_ROOT_RECORD_SIZE=32`, `SAVE_MAX_SIZE=586`.
기존 18-byte PKDG header/CRC 규칙은 유지하며 새 root도 CRC에 포함합니다.
v1/v2/v3/v4/v5는 명시적으로 decode하고 legacy의 root는 모두 0으로 남깁니다.

| root offset | bytes | field |
| --- | ---: | --- |
| 0 | 8 | storeId (nonzero) |
| 8 | 8 | generation (nonzero) |
| 16 | 4 | capacity=2048 |
| 20 | 4 | occupiedCount<=2048 |
| 24 | 4 | snapshotCrc32 (0 포함 전체 uint32 허용) |
| 28 | 2 | boxFormatVersion=1 |
| 30 | 2 | flags=0 |

### load / recovery / global validation

`GameSaveStorage::load`는 NVS pokemon_g1의 save_a/save_b를 독립적으로 읽습니다.
키 존재/내용 판독은 NVS의 nvs_get_blob 오류 코드를 직접 확인합니다. Preferences의 false/0 반환만으로
I/O 오류를 Missing으로 오인하지 않기 위한 읽기 경계 보완이며, 쓰기는 기존 Preferences를 사용합니다.
v5는 main CRC/의미 검증 후 B1 Snapshot::open으로 root의 파일을 전체 검증하고
key/count/CRC를 대조합니다. bitmap을 순회해 Party/Box ID 중복, nextInstanceId 상한,
seen/caught, shiny 개체의 shinyCaught도 검사합니다. 일반 개체의 과거 shinyCaught는 허용합니다.
전체 Box 개체 배열은 만들지 않습니다.

완전한 후보 중 높은 main sequence를 선택합니다. 최신 파일이 missing/corrupt이면 이전 쌍으로
복구합니다. NVS/파일 I/O 오류 또는 future version은 쓰기 불가로 처리합니다.
v5 흔적이 있으나 완전한 쌍이 없으면 RecoveryRequired이며, 식별 불가 손상도 Invalid로 보존합니다.
GameApp은 Missing만 새 게임으로 초기화합니다. 나머지 오류는 RAM fallback과 기존 저장 오류 표시를
사용하며 새 save로 덮어쓰지 않습니다.

### 명시적인 initialize / migration

GameApp 초기 로드에서 legacy이면 `initialize`, Missing이면 새 GameState로 `initialize`합니다.
이 함수만 빈 Box 생성 및 최초 filesystem 초기화를 수행합니다. 일반 save는 초기화하지 않습니다.

1. generic BoxStorage::mount는 여전히 LittleFS.begin(false).
2. mount가 실패했을 때만 별도 initializeFilesystem(format→mount)을 검토합니다.
3. load에서 진짜 Missing 또는 legacy가 선택되었고 양쪽 슬롯 어디에도 modern/future/식별 불가
   데이터가 없다고 확인한 경우만 format을 허용합니다. v5 쓰기를 한 번이라도 시도하면 이 실행에서는
   다시 format하지 않습니다.
4. esp_random 두 번으로 nonzero uint64 storeId 생성. zero/파일 충돌은 최대 8회 재시도합니다.
5. generation=1 empty snapshot 작성·재오픈 검증 후 metadata에서 BoxRoot를 구성합니다.
6. 모든 기존 GameState 필드는 그대로 복사하고 비활성 NVS 슬롯에 v5/sequence+1을 commit합니다.
7. 확인된 성공만 호출자의 GameSave를 승격합니다. 실패/불확정 orphan은 삭제하지 않습니다.

### commit 결과와 API

- `validatePair(save)`: main+Box/global 검증, storage 변경 없음 (필요 시 비파괴 mount).
- `initialize(save)`: 명시적인 최초 설치/legacy migration. 이미 commit된 동일 v5는 추가 쓰기 없음.
- `saveDetailed(save)`: Committed / NotCommitted / Indeterminate.
- `save(save)`: 기존 bool wrapper. Committed만 true.

일반 v5 save는 이미 존재하는 Box를 검증한 뒤 NVS만 씁니다. snapshot 생성/복사/삭제가 없고
Box generation은 유지됩니다. 향후 준비된 새 root를 candidate에 넣는 연결점도 같은 saveDetailed입니다.

write 반환값만으로 성공/실패를 단정하지 않습니다. 대상 NVS를 재판독하고 candidate와 바이트 비교 및
pair 검증 후 Committed를 반환합니다. 대상이 commit되지 않았고 기존 active 쌍이 유지됨을 확인하면
NotCommitted입니다. readback I/O 오류 또는 기존 상태 보존을 확인할 수 없으면 Indeterminate입니다.
Indeterminate는 live/activeSlot/sequence를 진행시키지 않고 이후 모든 쓰기를 잠급니다.
재부팅 또는 load가 실제 A/B를 다시 판정할 때만 해제됩니다. 자동 cleanup은 없습니다.

### host 검증

동일 run.ps1에서 기존 코어/전투/C1 포획/파티 viewer/애셋/fallback 앱/B1 Box 회귀를 유지합니다.
v4 golden 554 bytes는 수정하지 않았으며, 정상 decode와 모든 게임 필드 보존을 검사합니다.
v5에서 root를 제외하고 legacy header/CRC를 복원한 결과도 golden과 전부 일치합니다.
추가 save_pair_test는 v4 golden/배틀 HP·PP·turn·RNG/탐험·조우 이전, 최초 포맷 제한,
key 충돌, 최신 쌍 손상 rollback, 양쪽 불완전, global ID/도감, 부분 write와 write 후 read 오류,
불확정 재시도 차단/재부팅 복원, 일반 save의 Box 불변을 검사합니다.
GameApp 테스트도 불완전 쌍이 NVS를 덮어쓰지 않는지와 불확정 시 live 상태 보존을 검증합니다.

실기 업로드/포맷은 이번 작업에서 실행하지 않습니다. 이후 v4→v5 전환 후 상태 보존, 재부팅,
전원 차단 복구, mount 실패 시 비파괴 동작, Wi-Fi 사용 중 heap/저장 지연을 확인해야 합니다.
다음 G5-C2-C는 B1 mutate로 준비한 snapshot의 metadata→BoxRoot를 포획 candidate와 묶어
saveDetailed에 전달하는 연결입니다. 포획 성공 시 Box 사용과 UI 안내는 아직 구현하지 않았습니다.

B2 최종 검증: 위 run.ps1 전체 PASS (MSVC /W4 /WX), git diff --check PASS.
ESP32 core 3.3.11 / ESP32 Dev Module / PartitionScheme=huge_app 전체 compile/link PASS.

| B2 최종 빌드 | Flash | 전역 RAM |
| --- | ---: | ---: |
| 로컬 애셋 포함 | 1,516,976 bytes (48%) | 52,408 bytes (15%) |
| 애셋 없는 fallback | 1,514,092 bytes (48%) | 52,400 bytes (15%) |

Arduino CLI는 기존 B1 build cache 경로(build/firmware-g5-c2-b1 및
build/firmware-g5-c2-b1-fallback)를 재사용했으며, 최종 산출물은 B2 소스입니다.
fallback은 build/b1-fallback-source/smart_desk_esp32에 local_game_assets를 제외해 복사했고,
게임 .h/.cpp 파일의 hash 일치를 확인했습니다. 이 RAM 수치는 전역 변수만이며 실행 중 heap/stack
최대 사용량은 실기 측정 대상입니다. 실제 업로드/format/commit/push는 수행하지 않았습니다.

## Huge APP LittleFS subtype 수정 (ESP32 core 3.3.11)

스케치 루트 `firmware/smart_desk_esp32/partitions.csv`를 프로젝트 파티션 테이블로 사용합니다.
Arduino IDE에서는 ESP32 Dev Module + Huge APP을 그대로 선택합니다. core 3.3.11 platform.txt의
prebuild hook은 보드 기본 테이블 → variant 테이블 → sketch-local partitions.csv 순으로 복사하므로
이 프로젝트의 CSV가 최종 build/partitions.csv를 덮어씁니다. 별도 IDE 설정이나 설치 core 수정은 없습니다.

설치 core의 huge_app.csv와 기존 빌드 산출물은 filesystem subtype이 spiffs(0x82)였습니다.
하지만 해당 core의 CONFIG_LITTLEFS_SPIFFS_COMPAT는 꺼져 있고, 번들 LittleFS는 littlefs(0x83)를
찾습니다. 실제 설치된 libjoltwallet__littlefs.a에도 실기 오류와 같은
`No data partition with subtype "littlefs" found` 메시지가 있습니다.
라이브러리 컴파일/링크 성공은 실기 partition 검색이나 mount 성공을 검증하지 않기 때문에,
이전 전체 빌드가 성공해도 실기 mount는 실패할 수 있었습니다.

| name | type/subtype | offset | size |
| --- | --- | --- | --- |
| nvs | data/nvs | 0x9000 | 0x5000 |
| otadata | data/ota | 0xE000 | 0x2000 |
| app0 | app/ota_0 | 0x10000 | 0x300000 |
| spiffs | data/littlefs | 0x310000 | 0xE0000 |
| coredump | data/coredump | 0x3F0000 | 0x10000 |

기존 Huge APP와 비교해 filesystem subtype 하나만 변경합니다. label `spiffs`는
LittleFS.begin(false)의 기본 partitionLabel과 맞추기 위해 유지합니다. label은 이름이며
SPIFFS를 사용하는 뜻이 아닙니다. BoxStorage는 계속 LittleFS입니다.
NVS를 포함한 모든 offset/size/flags는 유지하며 B1 포맷, SAVE_VERSION=5, BoxRoot 및 transaction은
변경하지 않습니다. 새 CSV 추가 자체는 보드 데이터를 쓰거나 filesystem을 포맷하지 않습니다.

재현: 기존 Arduino CLI 명령의 FQBN `esp32:esp32:esp32:PartitionScheme=huge_app`으로 빌드합니다.
fallback 소스를 복사할 때도 이 partitions.csv를 함께 복사해야 합니다. 생성된 partitions.csv와
smart_desk_esp32.ino.partitions.bin의 filesystem subtype이 0x83인지 확인합니다.
실기 후속 확인 시에는 새 partition table을 포함해 업로드하고 Erase All Flash는 Disabled로 유지해야
기존 NVS를 보존할 수 있습니다. 이번 작업에서는 보드 upload/format을 실행하지 않았습니다.

검증 결과: host run.ps1 전체 PASS (/W4 /WX), asset/fallback Huge APP 전체 compile/link PASS,
git diff --check PASS. 두 build/partitions.csv는 sketch-local 파일과 hash가 일치하고,
두 partitions.bin을 32-byte entry 단위로 판독해 filesystem subtype=0x83,
나머지 모든 type/subtype/offset/size/flags 보존과 partition MD5를 확인했습니다.
컴파일러 Flash/RAM은 asset 1,516,976/52,408 bytes, fallback 1,514,092/52,400 bytes입니다.
partition generator의 name 'spiffs'와 subtype 0x83 불일치 경고는 default label을 유지하기 위한
의도된 이름/subtype 조합이며 오류가 아닙니다. 저장 코드 수정 및 보드 upload/format은 없습니다.

## G5-C2-C — Party full 포획 → Box

`battle.*`는 `CaptureDestination::Party/Box`를 구분합니다. `canUseCaptureBall`은
볼/배틀/ID 공통 조건만 검사하고, Party destination은 별도로 빈칸을 요구합니다.
성공 report의 `caught`는 C1과 같은 필드(야생 HP/기술/폼/성별/shiny, EXP/친밀도 0)의
영구 개체입니다. 실패 report의 caught.instanceId는 0이며 BattleState/wire에는 포함하지 않습니다.
전투 core에는 LittleFS/NVS 의존성이 없습니다.

`capture_storage.*`가 GameApp의 포획 저장을 조정합니다.

- Party < 3: 기존 Party append → main v5 save. Box snapshot/root/count는 변경하지 않습니다.
- Party == 3: B2 write gate → 현재 NVS BoxRoot의 pair/snapshot 검증 → 빈 slot 및 새 generation
  사전 검사 → core candidate/report → 성공일 때만 B1 Insert snapshot → 전체 validate →
  metadata에서 candidate BoxRoot 생성 → B2 saveDetailed → Committed일 때만 live/report 반영.
- 일반 볼 포획 실패는 반격/HP/turn/RNG만 일반 v5 save로 저장하며 snapshot을 만들지 않습니다.
- Box 2048칸 만재/ID UINT32_MAX/새 generation 불가 시 RNG·소비·상태·파일·NVS 변경 없이 안내합니다.
  현재 generation이 UINT64_MAX이면 증가하지 않습니다. 현재 root+1부터 최대 8개 경로를 검사하고,
  충돌하면 다음 세대로 건너뜁니다. orphan을 채택하거나 기존 파일을 덮어쓰지 않습니다.
- snapshot write/flush/reopen/validate 실패는 main commit 전 중단합니다.
  NotCommitted는 live를 유지하고 SAVE ERROR를 표시하며 재시도를 허용합니다.
  Indeterminate는 성공 연출 없이 live를 유지하고, B2 canWrite gate로 추가 snapshot/main write를
  막습니다. read 오류가 사라져도 load 전에는 재시도하지 않습니다. 자동 삭제/format은 없습니다.
- Committed 뒤에만 기존 성공 3회/실패 1~2회 흔들림을 시작합니다. 성공 위치에 따라
  "파티에 합류!" 또는 "박스로 전송!"을 표시합니다. 만재는 "박스가 가득 찼다 / OK 확인"입니다.
  재부팅은 B2 A/B pair validation으로 복구하며 성공 메시지는 복원하지 않습니다.

Box 성공마다 49,472 bytes의 immutable snapshot 1개가 추가되고 source도 남습니다.
실패한 destination도 orphan으로 보존하므로 재시도 시 파일이 더 남을 수 있습니다.
896 KiB 영역의 단순 나눗셈은 최대 18개 파일이지만 LittleFS metadata/여유 블록 때문에 실제 수는
더 적습니다. Box의 논리 용량 2048과 GC 없이 가능한 반복 mutation 횟수는 별개입니다.
공간 부족은 저장 오류로 처리하며 현재 pair를 유지합니다. 8개 후보 경로가 모두 존재하면 검사도
중단하므로 반복 실패 후에는 정리가 필요할 수 있습니다. 이번 단계에는 global GC가 없습니다.

후속 GC는 load 또는 확정 commit 후 저장 coordinator에 연결해야 합니다. save_a와 save_b가
각각 참조하는 두 BoxRoot(같을 수도 있음)를 모두 검증·보호한 뒤, 참조되지 않는 파일만 삭제해야
합니다. Indeterminate/읽기 오류/미지원 버전처럼 참조를 확정할 수 없으면 정리하지 않습니다.
RAM live root 하나만 기준으로 지우면 A/B rollback을 잃습니다.

G5-C2-D Box viewer는 load로 확정된 root와 B1 Snapshot의 occupied/readSlot/findInstance/
findSpecies를 이용해 필요한 슬롯만 읽으면 됩니다. GameApp의 root는 현재 private이므로 다음
단계에서 작은 읽기 전용 접근 경계를 추가해야 합니다. 이번에는 Box UI/교체/방생/GC가 없습니다.
SAVE_VERSION=5, BoxRoot/wire, BOX_CAPACITY=2048, B1 포맷과 B2 transaction은 유지합니다.

검증: 기존 run.ps1에 capture_storage_test와 full-Box/UI fixture를 연결했습니다. Party 1/2 capture,
Box 성공 필드/중복/재부팅/이전 pair rollback, 일반 볼 실패, 만재/ID/세대 경계, 손상 root,
8회 collision 한도, write/flush/reopen/validate 실패, NotCommitted retry, Indeterminate의
commit/미commit 재부팅 결과, 실제 GameApp 성공 문구/연출/저장 오류를 검사합니다.

C 최종 검증: run.ps1 전체 PASS (MSVC /W4 /WX, core/app/asset app/B1 Box),
git diff --check PASS. ESP32 core 3.3.11 / ESP32 Dev Module / Huge APP 전체 compile/link PASS:

| G5-C2-C 빌드 | Flash | 전역 RAM |
| --- | ---: | ---: |
| 로컬 애셋 포함 | 1,518,844 bytes (48%) | 52,456 bytes (16%) |
| 애셋 없는 fallback | 1,515,900 bytes (48%) | 52,448 bytes (16%) |

기존 build/firmware-g5-c2-b1 및 build/firmware-g5-c2-b1-fallback 경로를 재사용한 C 산출물입니다.
fallback 소스의 .h/.cpp/.ino 및 partitions.csv는 원본과 동일하고 local_game_assets는 없습니다.
두 최종 partitions.bin의 MD5와 LittleFS subtype 0x83, NVS 0x9000/0x5000 및 나머지 배치를
확인했습니다. 기존 label 'spiffs'/subtype 0x83 경고는 위 B2.1 설명과 같습니다.
실기 upload/format/commit/push는 실행하지 않았습니다. 실행 중 heap/stack 및 Box가 커졌을 때의
전체 snapshot 검증 지연은 실기에서 별도 확인해야 합니다.

## G5-C2-D — Scalable Box Browser (읽기 전용)

Home은 상태 → 파티 → 박스 → 탐험 → 도감 순서입니다. 물리 저장은 기존 flat 2048 slots이며,
Box 1/2 같은 구획이나 persistent index를 추가하지 않습니다. 최근/도감/전체/검색은 동일한
NVS BoxRoot snapshot 위에 만든 가상 View입니다. Box 메뉴는 최근 포획, 도감순 보기, 전체 보기,
찾기, 돌아가기이며 메뉴는 커서가 움직이는 2행 viewport입니다.

`box_browser.h/.cpp`의 Browser는 정확한 root로 mount(false)/open한 뒤 B1 전체 검증과
key/count/CRC/root 형식 일치를 확인합니다. occupied slot을 한 번 순서대로 읽어 Index를 만들고,
이후 View 변경은 RAM에서만 필터/정렬합니다. Index는 종 테이블에 의존하지 않는 1~1025 범위를
지원합니다. 현재 B1의 실제 개체 검증은 기존 3종 fixture 범위 그대로이며, D 때문에 알 수 없는
종을 저장 데이터로 허용하거나 새 종 콘텐츠를 추가하지 않습니다. 모든 세대 경계는 독립적인
숫자 메타데이터 인덱스 테스트로 검증합니다.

| View | 필터 | 순서 |
| --- | --- | --- |
| 최근 포획 | 전체 | instanceId 내림차순 |
| 도감순 보기 | 전체 | speciesId 오름차순, 같은 종 instanceId 오름차순 |
| 전체 보기 | 전체 | occupied slot 오름차순 |
| 도감번호 | speciesId 정확히 일치, 중복 모두 포함 | instanceId 내림차순 |
| 세대 | 1~151 / 152~251 / 252~386 / 387~493 / 494~649 / 650~721 / 722~809 / 810~905 / 906~1025 | 도감순 |
| 색이 다른 | shiny=true | instanceId 내림차순 |

도감번호는 4자리이며 LEFT/RIGHT로 현재 자리를 0~9 wrap, OK로 다음 자리로 이동합니다.
마지막 자리 OK로 검색합니다. 0000/1026 이상은 범위 안내 후 OK로 입력에 돌아갑니다.
LEFT 길게는 입력 취소입니다. 결과가 없으면 안내 후 OK로 상위 검색 메뉴로 돌아갑니다.

OLED2 목록은 header(선택 번호/결과 수) + 최대 3행입니다. 커서가 viewport를 벗어날 때만
스크롤하며 선택 행만 기존 UTF-8 scrolling helper를 사용합니다. 다른 행은 영역 내 clipping을
사용합니다. 마지막에는 별도 돌아가기 행이 있고 결과 수에는 포함하지 않습니다. 짧은 L/R은
이 행을 포함해 wrap합니다. 긴 L/R은 이 행을 건너뛰고 실제 개체 수 기준 ±10 wrap합니다.
D.1부터 목록 OK는 개체 메뉴(능력치 / 기술 / 돌아가기)입니다. 돌아가기 또는 BACK으로
목록 선택 위치를 유지합니다. ±1/±10 개체 이동은 목록에서만 합니다. instanceId는 내부
식별자로 저장·정렬·검증에 유지하며 플레이어 UI에는 표시하지 않습니다.
OLED1은 선택 개체의 기존 48x48 local wild sprite와 #번호/Lv/성별/SHINY/폼을 표시합니다.
애셋 또는 해당 sprite가 없으면 '?' fallback이며 새 asset은 없습니다.

버튼은 Box에서만 `readButtonEvent(true)`로 단독 750ms LONG을 활성화합니다. 다른 화면은
기존 OK press / L/R release 동작을 유지합니다. LONG 뒤 release는 소비하고, 기존 1000ms
LEFT+RIGHT chord가 우선합니다. 반대 버튼의 raw LOW도 확인해 debounce 중 LONG 오발을 막습니다.
알림 중에는 LONG을 활성화하지 않습니다. 핀과 chord 시간은 바꾸지 않습니다.

RAM: Entry는 instanceId 4 + slot 2 + speciesId 2 + shiny 1 + padding 3 = 12 bytes입니다.
2048 metadata 24,576 + uint16_t 정렬 인덱스 4,096 + count 필드 4 = Index 28,676 bytes입니다.
이는 재사용하는 전역 버퍼이며 내용을 파일/NVS에 저장하지 않습니다. 전체 PokemonInstance
배열을 복사하지 않고 선택 개체 1개만 캐시합니다. std::sort는 O(n log n), 필터는 O(n),
비교 함수는 RAM만 읽습니다. 이동은 선택 레코드 1회 read이고 redraw에는 filesystem read가 없습니다.
진입 시 기존 B1 CRC/개체/중복 ID 검증은 그대로 수행합니다. B1의 64-ID scratch 중복 검증은
최대 32 passes이므로 전체 진입 비용이 새 인덱스의 선형 스캔/정렬 비용보다 클 수 있습니다.
실제 2048개 Box에서의 flash read 지연은 실기 측정 대상입니다.

GameApp은 Box 화면에서 update/saveState를 통한 저장을 막고 Browser에는 쓰기 API가 없습니다.
종료/오류/init 및 DESK 전환에서 close/reset하며 GAME 복귀는 Home입니다. 따라서 다음 진입은
현재 root를 다시 검증합니다. 빈 Box도 handle을 즉시 닫습니다. 목록·정렬·검색·상세는
GameSave/BoxRoot/sequence/Party/ID/도감/탐험/배틀/NVS/파일 바이트를 변경하지 않습니다.

후속 E의 이동/방생은 이 read-only 모듈을 저장 경로로 사용하지 않고 기존 transaction coordinator에
연결해야 합니다. mutation/GC 전 Browser를 닫고, 확정 root로 다시 열어 transient index를
재구축해야 합니다. GC에서는 여전히 A/B 양쪽 root를 보호해야 합니다. D에는 이동/방생/교체/GC가 없습니다.

호스트 검증은 기존 run.ps1에 포함합니다: 단일/중복/sparse/2048 슬롯, 모든 세대 경계,
정렬/번호 범위/shiny/무결과, missing/corrupt/root mismatch/I/O, 100회 반복의 파일/NVS 불변,
인덱스 filter/sort와 redraw의 read 횟수, 이동당 1 record read, 버튼 short/long/chord/debounce/
시간 wrap, 애셋 및 fallback 실제 UI와 모든 기존 회귀를 검사합니다.

D 최종 검증: run.ps1 전체 PASS (MSVC /W4 /WX), git diff --check PASS.
초기화/load 없이 실제 UI를 100회 다시 열고 닫은 뒤에도 RAM GameState와 NVS/파일 바이트가
동일합니다. 호스트 memory-backed LittleFS의 full Box open+4 Views는 최종 실행에서 9ms였으며,
이는 ESP32 flash 성능 수치가 아닙니다. 테스트 로그: build/game-state-tests/g5-c2-d-results.txt.

ESP32 core 3.3.11 / ESP32 Dev Module / Huge APP 최종 compile/link 결과:

| G5-C2-D 빌드 | Flash | 전역 RAM | C 대비 전역 RAM 증가 |
| --- | ---: | ---: | ---: |
| 로컬 애셋 포함 | 1,524,992 bytes (48%) | 81,560 bytes (24%) | 29,104 bytes |
| 애셋 없는 fallback | 1,522,012 bytes (48%) | 81,552 bytes (24%) | 29,104 bytes |

기존 B1 이름의 build 경로를 재사용했으며 산출물은 최종 D 소스와 대조했습니다. fallback에는
local_game_assets가 없고 .h/.cpp/.ino/partitions.csv가 원본과 같습니다. 두 최종 partition binary의
LittleFS subtype 0x83, NVS offset/size와 MD5를 확인했습니다. 기존 'spiffs' label 경고는 B2.1의
의도된 설정입니다. 보드 upload/실제 format/commit/push는 실행하지 않았습니다.

실기에서는 빈 Box, 각 View와 중복 종, 0025/0000/1026 입력 및 취소, 세대/shiny 무결과,
짧게 ±1/길게 ±10(빠른 이동은 돌아가기 행 제외), 상세 복귀 위치, 750ms 단독 long과 1초 chord,
Box 중 DESK 전환→GAME Home 복귀→재진입, 재부팅 후 동일 개체, 애셋/fallback 배치를 확인합니다.
특히 full Box 진입 지연과 Wi-Fi/알림 사용 중 heap/stack 여유는 실제 보드에서 측정해야 합니다.

## G5-C2-D.1: Box 조회와 공통 BACK

- OLED1: 선택 개체 이름, #전국도감 번호, Lv/성별, SHINY/폼, 기존 sprite 또는 `?` fallback.
- 개체 메뉴: **능력치 / 기술 / 돌아가기**. 파티 이동·방생은 후속 E이며 가짜 메뉴는 없습니다.
- 능력치: 기존 `calculateStats()` 결과를 2페이지로 표시합니다. 1페이지 HP/공격/방어,
  2페이지 특공/특방/스피드. LEFT/RIGHT 페이지 전환, OK/BACK 개체 메뉴 복귀입니다.
- 기술: `moves[4]`의 0 슬롯을 제외하고 기존 lookup 이름을 2개씩 표시합니다. 0개는
  `기술 없음`, 미등록 ID는 `?`입니다. 개체에 없는 PP는 표시하지 않습니다.
  LEFT/RIGHT 페이지 전환, OK/BACK 개체 메뉴 복귀입니다.
- 짧은 LEFT+RIGHT는 debounce 후 두 버튼 모두 release일 때 BACK 1회입니다.
  1000ms 이상 chord는 기존 MODE SWITCH 1회이며 release BACK은 없습니다.
  chord의 단독 short/long 입력은 억제합니다. 단독 Box 750ms long ±10과 OK press는 유지합니다.
- BACK: 능력치/기술 → 개체 메뉴 → 목록 → Box 메인 → Home. 찾기 하위 메뉴는 찾기로,
  찾기는 Box 메인으로 돌아갑니다. 무결과는 해당 검색 부모로 돌아갑니다.
  기존 화면상 돌아가기 항목도 유지합니다(검색 목록의 돌아가기는 검색 부모로 복귀).
- Status/Party/RegionSelect BACK은 Home입니다. Battle Move/Ball 선택 BACK은 Command로만
  복귀하며 BattleState/HP/PP/턴/RNG/저장에 영향이 없습니다. Command BACK은 무시합니다.
  공격 결과, 포획 연출/성공, 도주 성공, 탐험 진행, 저장 오류·복구 상태는 BACK으로 취소하지 않습니다.
- DESK는 새 BACK을 무시합니다. 알림 dismiss/Timer/화면 이동/모드 전환의 기존 의미를 유지합니다.
- 저장 버전/Box format/BoxRoot/transaction은 변경하지 않았습니다. 이번 단계의 조회에는
  mutation, snapshot 생성, GC가 없습니다. 기존 선택 1개 캐시와 Box 인덱스를 재사용합니다.

D.1 검증: `firmware/tests/game_state/run.ps1` 전체 PASS (MSVC `/W4 /WX`).
짧은 chord의 release 순서/동시 release/단독 입력 억제, 기존 long/OK, BACK 계층,
실제 stats, 0~4 기술/빈 슬롯/미등록 ID fallback, 전투 Move/Ball 취소의 전체 serialized
GameState 및 NVS/Box 파일 불변성을 검사합니다. 기존 포획/복구/2048 Box 회귀도 PASS입니다.
로그: `build/game-state-tests/g5-c2-d1-results.txt`.

| D.1 ESP32 core 3.3.11 / Huge APP | Flash | 전역 RAM |
| --- | ---: | ---: |
| 애셋 포함 | 1,525,976 bytes (48%) | 81,560 bytes (24%) |
| 애셋 없는 fallback | 1,522,964 bytes (48%) | 81,552 bytes (24%) |

두 구성 모두 전체 compile/link PASS이며 전역 RAM은 D와 같습니다. 최종 빌드 소스 및
partition CSV/binary/MD5를 대조했습니다. LittleFS subtype 0x83 및 NVS 0x9000/0x5000,
나머지 partition offset/size는 그대로입니다. 기존 spiffs label/subtype 경고만 있습니다.
로그: `build/g5-c2-d1-asset-build.txt`, `build/g5-c2-d1-fallback-build.txt`.
`git diff --check` PASS. upload/실제 filesystem format/commit/push는 하지 않았습니다.

실기 확인: 짧은 L+R을 서로 다른 순서로 떼도 BACK 1회인지, 긴 L+R 전환 뒤 BACK이 없는지,
단독 짧게 ±1/길게 ±10과 OK가 유지되는지 확인합니다. Box 메뉴→개체→능력치/기술의 페이지,
OLED1 이름/sprite/성별/shiny, 목록 복귀 위치, Status/Party/지역선택 BACK도 확인합니다.
전투 기술/볼 메뉴 BACK 후 HP/PP/턴과 볼 수가 유지되고 정상 실행되는지, 결과 연출은
BACK으로 취소되지 않는지 확인합니다. DESK Timer/알림, GAME/DESK 전환과 재부팅 후
세이브 유지도 실기 확인 대상입니다.
