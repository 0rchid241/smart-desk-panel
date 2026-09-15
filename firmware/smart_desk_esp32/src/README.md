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
| `game/game_app` | Pikachu 프레임, Desk Pet, 게임 그래픽/메뉴, 임시 메뉴 입력 |

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
  `local_game_assets/`가 없으면 기존 `LOCAL ASSET NOT FOUND` 화면을 사용합니다.
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
