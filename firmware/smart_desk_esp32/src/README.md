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
