# 3Axis-Gimbal-Tracking_Firmware

카메라로 빨간 물체를 실시간 검출하고, **IMU(자이로+지자기) 자세값과 카메라 오프셋을 융합한 PID 제어**로 3축(Roll/Pitch/Yaw) 서보 짐벌을 자동 추적시키는 임베디드 시스템입니다. OTA(무선 펌웨어 업데이트)와 PC용 Qt 모니터링 앱까지 포함한 풀스택 구성입니다.

ESP32-CAM(영상처리) · STM32F411 IMU 보드(짐벌 제어 두뇌) · STM32F411 CAM 보조 보드(LCD 출력) 3개 보드가 SPI/UART로 역할을 나눠 맡는 구조이며, 이 저장소는 [Jimbal_IMU_STM32F411](https://github.com/KCCI-Ottogi/Jimbal_IMU_STM32F411) · [Jimbal_CAM_STM32F411](https://github.com/KCCI-Ottogi/Jimbal_CAM_STM32F411) 등 팀 저장소 4개를 `git subtree`로 하나로 모은 통합 저장소입니다.

## 시스템 아키텍처

```
[ESP32-CAM]                         [STM32F411 · IMU 보드]                    [PC]
 HSV 색상 검출 + 칼만필터   --SPI-->   자세 추정(상보필터) + 카메라 PID 융합   <--UART-->  [App-Qt]
 (cx, cy, area)                       --PWM--> Roll/Pitch/Yaw 서보 3개                   실시간 모니터/제어

[STM32F411 · CAM 보조 보드] --SPI(카메라)--> LCD에 추적 박스 표시
```

| 보드 | 역할 | 담당 |
|---|---|---|
| **STM32F411 (IMU)** | 자세 추정, IMU+카메라 PID 융합, 서보 3축 제어, CLI, OTA 클라이언트 | `Firmware-IMU/` — ★ 중점 구현 파트 |
| **ESP32-CAM** | HSV 색상 검출 + 칼만필터로 좌표 안정화, Wi-Fi 웹서버 | `Firmware-CAM/ESP32_CAM/` |
| **STM32F411 (CAM 보조)** | ESP32-CAM 좌표 SPI 수신 → LCD 출력 | `Firmware-CAM/Jimbal_CAM_MX/` |
| **PC (Qt)** | 시리얼 연결, 실시간 상태 모니터링, CLI 명령 전송 | `App-Qt/` |
| **STM32F411 (IMU, 부트로더 영역)** | OTA 수신 펌웨어를 앱 영역으로 복사 후 점프 | `Firmware-OTA/` (별도 보드 아님, 아래 참고) |

### OTA 메모리 구조 (IMU 보드 플래시를 부트로더/앱 영역으로 분할)

| 영역 | 주소 | 담당 프로젝트 |
|---|---|---|
| Sector 0-1 | `0x08000000` | `Firmware-OTA` (부트로더) |
| Sector 2-5 | `0x08008000` | `Firmware-IMU` (앱, 링커 스크립트 `ORIGIN`과 일치) |
| Sector 6 | `0x08040000` | ESP8266 다운로드 임시 영역 |

재부팅 시 플래그가 서 있으면 Sector 6 → Sector 2-5로 복사 후 점프, 없으면 바로 앱 실행.

## 전체 프로젝트 구조

```
.
├── Firmware-IMU/Jimbal_IMU_MX/MyApp/     # ★ 짐벌 제어 메인 (FreeRTOS, STM32CubeIDE)
│   ├── ap/
│   │   ├── ap.c                          # CLI 명령 등록, 태스크 초기화 진입점
│   │   ├── monitor.c                     # $ID:TYPE:VALUE# 패킷 스트리밍 (Qt 앱 연동)
│   │   └── service/
│   │       ├── gyro_service.c            # 상보필터로 Roll/Pitch/Yaw 계산
│   │       ├── camera_service.c          # 카메라 오프셋 PID 계산
│   │       └── gimbal_control.c          # IMU+카메라 값 합산 → 서보 명령 실행 (10ms 주기)
│   ├── bsp/bsp.c                         # HAL_Delay/millis 등 보드 지원 계층
│   └── hw/driver/
│       ├── gyro.c, mag.c                 # MPU6050(I2C) / HMC5883L(I2C) 드라이버
│       ├── servo.c                       # 서보 3축 PWM 제어
│       ├── cli.c                         # UART CLI 파서 (help/imu/gim/servo/ota/mon 등)
│       ├── esp8266.c                     # OTA용 Wi-Fi AT커맨드 + S3 다운로드 (프로토타입)
│       ├── uart.c, my_gpio.c, led.c, button.c, log.c
│
├── Firmware-CAM/
│   ├── ESP32_CAM/src/
│   │   ├── main.cpp                      # 카메라 초기화, 50ms 주기 추적 루프
│   │   └── srclib/
│   │       ├── color_tracker.c           # HSV 빨간색 블롭 검출 + 칼만필터
│   │       └── wifi_manager.cpp          # Wi-Fi 웹서버 (영상 확인용)
│   └── Jimbal_CAM_MX/MyApp/
│       ├── ap/ap.c                       # SPI로 카메라 좌표 수신, 핑퐁버퍼 처리
│       └── hw/driver/lcd.c               # 추적 박스 LCD 출력
│
├── Firmware-OTA/Core/Src/main.c          # 부트로더: 플래그 확인 → 플래시 복사 → 앱 점프
│
└── App-Qt/
    ├── mainwindow.cpp                    # 시리얼 연결, 패킷 파싱, 버튼→CLI 명령 전송
    └── mainwindow.ui
```

## Firmware-IMU · 짐벌 제어 (중점 구현 파트)

`bsp`(보드 지원) → `hw/driver`(센서/서보/통신 드라이버) → `ap/service`(제어 로직) 3계층으로 구성했습니다. 층을 나눈 이유는 보드가 바뀌어도(F411 → 다른 MCU) `ap` 로직은 그대로 두고 `hw/driver`만 갈아끼울 수 있게 하기 위해서입니다.

**통신 프로토콜** — IMU 보드가 상태를 스트리밍하는 패킷 포맷:

```
$개수,ID:타입:값,ID:타입:값,...#
예) $3,73:2:12.5,74:2:-3.2,75:2:90.0#   → Roll 12.5°, Pitch -3.2°, Yaw 90.0°
```

| ID | 의미 | ID | 의미 |
|---|---|---|---|
| 73 | Roll (자이로 X) | 51 | 모터 속도 |
| 74 | Pitch (자이로 Y) | 76 | IMU 전체 |
| 75 | Yaw (자이로 Z) | 77 | 짐벌 각도 |

**CLI 명령어**: `imu` `gim` `servo` `led` `cam` `mon` `ota` `info` `sys` `gpio` `md` `button` `log` `help` `cls` — UART로 입력해 개별 기능을 단독 테스트할 수 있습니다.

## Firmware-CAM · 컬러 트래킹

ESP32-CAM에서 HSV(색조·채도·명도) 색공간으로 빨간색 영역을 찾고, 칼만 필터로 좌표를 안정화합니다. 최소 검출 면적(`MIN_DETECT_AREA`, 50px) 이하는 노이즈로 버립니다. STM32F411 보조 보드는 이 좌표를 SPI로 받아 LCD에 추적 박스를 그리는 역할만 맡아 IMU 보드의 제어 루프와 분리했습니다.

## App-Qt · 데스크톱 모니터

시리얼 포트로 보드에 연결해 위 프로토콜 패킷을 실시간 파싱, Roll/Pitch/Yaw·카메라·LED 상태를 라벨로 표시하고 버튼 클릭으로 CLI 명령(`imu on`, `gim on`, `mon on 100` 등)을 전송합니다.

## 트러블슈팅

| 문제 | 원인 | 해결 |
|---|---|---|
| FreeRTOS 태스크 스택 오버플로우로 CLI 모니터 먹통 | `myTaskGyro` 스택 512워드로 부족, 자이로+상보필터 계산 중 스택 침범 | 스택 512→1024워드, `myTaskMag` 256→512워드, 힙 15360→51200바이트 확장 (`a8dd58e`, `316fd6b`) |
| MPU6050 연결 후 CLI 모니터 반복 먹통 | 인터럽트 핀 PA0(EXTI0)이 다른 용도와 충돌 추정 | 인터럽트 핀을 PA4(EXTI4)로 재배치 + 상보필터 추가 (`c5365e4`) |
| LCD에 카메라 영상 첫 프레임만 찍히고 멈춤 | SPI1 Prescaler 128 → 656kbit/s로 프레임 데이터 대비 전송 속도 부족 | Prescaler 128→4(21Mbit/s) + 핑퐁 버퍼·듀얼 DMA 적용 (`a9bf0f6`, `2eb68fd`) |
| CAM 보드 RAM 여유 부족 | LCD 프레임버퍼+힙(15360B) 합계가 SRAM 예산 압박 | 힙 15360→8192바이트로 축소 + 스택 오버플로우 감지 활성화 (`5a0816f`) |
| 피부색·그림자를 빨간 물체로 오검출 | HSV 임계값 범위가 넓음(S≥80, V≥50) | `S_MIN` 80→150, `V_MIN` 50→60, `H_MAX_LOW` 15→10으로 축소 |
| Pitch 축 서보가 반대 방향으로 동작 | 자이로→서보 변환 시 부호 반전 누락 | 부호 반전 수정 (`3b1fdf6`) |

## 빌드 & 실행

```bash
# Firmware-IMU / Firmware-CAM (STM32F411, STM32CubeIDE 또는 CMake+ARM GCC)
cmake --preset Debug && cmake --build build/Debug

# Firmware-OTA (부트로더, 동일 툴체인)
cmake --preset Debug && cmake --build build/Debug

# Firmware-CAM/ESP32_CAM (PlatformIO)
pio run -e esp32cam -t upload

# App-Qt (Qt 6/5, Widgets + SerialPort)
cmake -B build -S App-Qt && cmake --build build
```

## 알려진 제한 사항

- **Firmware-OTA**: 부트로더 로직은 완성됐지만, `esp8266_DownloadOTA()`는 코드 주석에 "개념적 구현"으로 명시돼 있음 — HTTP 헤더 파싱 생략, Wi-Fi 비밀번호 하드코딩, DMA 미적용으로 대용량 수신 시 데이터 유실 가능, CRC 무결성 검증 부재.

## 향후 개선 계획

- OTA 다운로드 로직 정식 구현 (DMA 링버퍼, HTTP 파싱, CRC 검증)
- PID 게인 자동 튜닝
- 다중 색상/다중 타겟 추적

## 팀

Dejavu-Jamaisvu(본인, 짐벌 서보 제어) · jmGim(IMU/OTA) · hsol0212(LCD) · jiihyeonn(CAM)
