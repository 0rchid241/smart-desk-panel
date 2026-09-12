# Smart Desk Panel

ESP32 기반 PC 상태 모니터링 및 제어용 스마트 데스크 패널입니다.

OLED 디스플레이와 물리 버튼을 이용해 PC 상태를 확인하고 간단한 PC 제어 기능을 제공하는 것을 목표로 합니다.

## 목표

### v1.0

- ESP32 + OLED 화면 구성
- 물리 버튼 입력
- PC의 CPU / RAM / 시간 등 상태 표시
- 미디어 재생/정지 등 간단한 PC 제어
- USB Serial 기반 PC ↔ ESP32 통신

### 이후 계획

- Wi-Fi 기반 무선 통신
- 자동 재연결
- 센서 연동
- Smart Desk 자동화 기능

## 프로젝트 구조

smart-desk-panel/
├─ firmware/
│  └─ smart_desk_esp32/   # ESP32 펌웨어
├─ pc-agent/              # Windows Python Agent
├─ docs/                  # 개발 및 하드웨어 문서
├─ .gitignore
└─ README.md

## Hardware

현재 v1 개발 예정 구성:

* ESP32 DevKitC V4
* SSD1306 0.96" I2C OLED
* Push Button
* Breadboard
* Jumper Wire

## Status

🚧 개발 준비 중