# 3Axis-Gimbal-Tracking_Firmware

카메라로 빨간 물체를 실시간 검출하고, **IMU(자이로+지자기) 자세값과 카메라 오프셋을 융합한 PID 제어**로 3축(Roll/Pitch/Yaw) 서보 짐벌을 자동 추적시키는 임베디드 시스템입니다. OTA(무선 펌웨어 업데이트)와 PC용 Qt 모니터링 앱까지 포함한 풀스택 구성입니다.

ESP32-CAM(영상처리) · STM32F411 IMU 보드(짐벌 제어 두뇌) · STM32F411 CAM 보조 보드(LCD 출력) 3개 보드가 SPI/UART로 역할을 나눠 맡는 구조이며, 이 저장소는 [Gimbal_IMU_STM32F411](https://github.com/KCCI-Ottogi/Gimbal_IMU_STM32F411) · [Gimbal_CAM_STM32F411](https://github.com/KCCI-Ottogi/Gimbal_CAM_STM32F411) · [OTA_Gimbal](https://github.com/KCCI-Ottogi/OTA_Gimbal) · [Qt_Gimbal](https://github.com/KCCI-Ottogi/Qt_Gimbal) 팀 저장소 4개를 `git subtree`로 하나로 모은 통합 저장소입니다.

## 시스템 아키텍처

```
[ESP32-CAM]              [STM32F411 · CAM 보조보드]           [STM32F411 · IMU 보드]              [PC]
 HSV 색상검출+칼만필터              │                            상보필터 자세추정
 (영상 프레임 + cx,cy)   --SPI-->  38KB 프레임 수신 → LCD 출력    + 카메라 오차 PID 융합    <--UART-->  [App-Qt]
                          좌표만 추출(7B 패킷)  --UART-->  좌표 파싱 → PID 계산               실시간 모니터/제어
                                                            --PWM--> Roll/Pitch/Yaw 서보 3개
```

ESP32는 IMU 보드와 직접 연결돼 있지 않습니다. SPI로 넘어가는 데이터는 160×120 영상 프레임 전체(약 38KB)라 LCD가 달린 CAM 보조보드가 이를 받아 화면에 그리고, 그중 좌표(cx, cy, 감지여부)만 뽑아 7바이트 패킷으로 압축해 UART로 IMU 보드에 넘겨줍니다. 무거운 영상 데이터가 IMU 보드의 실시간 제어 루프를 방해하지 않도록 분리한 구조입니다 (근거: `Jimbal_CAM_MX/MyApp/hw/driver/uart.c`의 `// ESP32는 SPI로 받고, huart6는 송신 전용` 주석, `Jimbal_IMU_MX/.../camera_service.c`의 `// UART로부터 데이터 수신 (Board A -> Board B)` 주석).

| 보드 | 역할 | 담당 |
|---|---|---|
| **STM32F411 (IMU / Board B)** | 자세 추정, IMU+카메라 PID 융합, 서보 3축 제어, CLI, OTA 클라이언트 | `Firmware-IMU/` — ★ 중점 구현 파트 |
| **ESP32-CAM** | HSV 색상 검출 + 칼만필터로 좌표 안정화, SPI로 영상+좌표 송신 | `Firmware-CAM/ESP32_CAM/` |
| **STM32F411 (CAM 보조 / Board A)** | ESP32-CAM 영상 SPI 수신 → LCD 출력, 좌표만 추출해 IMU 보드로 UART 중계 | `Firmware-CAM/Jimbal_CAM_MX/` |
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
│   └── Jimbal_CAM_MX/MyApp/               # Board A: ESP32 영상 수신 + IMU 보드로 좌표 중계
│       ├── ap/ap.c                       # SPI2로 프레임 수신(핑퐁버퍼), 좌표 추출 → uartSendTrackData()
│       └── hw/driver/
│           ├── lcd.c                     # SPI1로 LCD에 영상+추적 박스 출력
│           └── uart.c                    # 좌표 7바이트 패킷을 UART6로 IMU 보드에 송신
│
├── Firmware-OTA/Core/Src/main.c          # 부트로더: 플래그 확인 → 플래시 복사 → 앱 점프
│
└── App-Qt/
    ├── mainwindow.cpp                    # 시리얼 연결, 패킷 파싱, 버튼→CLI 명령 전송
    └── mainwindow.ui
```

> `Jimbal_IMU_MX/`, `Jimbal_CAM_MX/` 폴더명은 STM32CubeIDE 프로젝트를 처음 만들 때 붙인 이름이 그대로 남아있는 것입니다. GitHub 저장소명은 이후 `Jimbal` → `Gimbal`(정확한 영문 표기)로 변경되었지만, 로컬 프로젝트 폴더명까지 바꾸면 CubeIDE `.ioc` 설정과 빌드 경로가 꼬일 수 있어 폴더명은 유지했습니다.

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

### 서보 제어 (담당 파트)

3축 서보를 `servo.c`(하드웨어 드라이버)와 `gimbal_control.c`(제어 로직) 두 층으로 나눴습니다. `servo.c`는 "각도를 받아 PWM으로 내보내는 것"만 알고, IMU 값을 어떻게 각도로 바꿀지는 전혀 모릅니다 — 그래야 짐벌 알고리즘이 바뀌어도 서보 드라이버는 손댈 필요가 없습니다.

**채널 구성** (`servo.h`)

| 채널 | 축 | 초기(Home) 각도 | 가동 범위 |
|---|---|---|---|
| CH0 | Roll | 110.0° | 0° ~ 180° |
| CH1 | Pitch | 65.0° | 0° ~ 130° |
| CH2 | Yaw | 70.0° | 0° ~ 180° |

**PWM 변환식** (`servoWrite()`) — 일반 RC 서보 규격(0.5ms~2.5ms 펄스폭)에 맞춘 선형 변환입니다.

```c
pulse_us = 500 + angle * (2500 - 500) / 180   // 0°→500us, 180°→2500us
```

`servoWrite()`는 각도를 넣기 전에 항상 `min_angle`/`max_angle`로 다시 한 번 잘라내는데(clamping), 이는 상위 로직(`gimbal_control.c`)에서 이미 범위를 제한해도 서보에 물리적으로 무리한 각도가 절대 나가지 않도록 하는 이중 안전장치입니다.

**부드러운 이동 — 지수 평활(Exponential Smoothing)**

서보에 목표각을 바로 꽂으면 순간적으로 홱 움직여 기체가 흔들립니다. 그래서 목표(`target_angle`)와 현재(`current_angle`)를 분리해두고, 제어 주기마다 그 차이의 일정 비율(`k`, 0~1)만큼만 이동시킵니다.

```c
current_angle += (target_angle - current_angle) * k;   // servoSmoothUpdate()
```

`k`가 클수록 빠르고 즉각적으로, 작을수록 느리고 부드럽게 움직입니다. `gimbal_control.c`에서는 초기엔 `k=0.3`으로 시작했다가 반응성이 부족해 `k=1.0`으로 조정한 이력이 있습니다(커밋 `db8d0c7`, "test: servo k (0.3 -> 1.0)") — `k=1.0`은 사실상 평활을 끄고 목표각으로 바로 이동하는 것과 같아, 최종적으로는 반응 속도를 우선한 설정입니다.

**목표 각도 계산 파이프라인** (`gimbalExecuteCombinedControl()`, 10ms 주기)

```
최종 목표각 = 서보 초기각(Home)
            + IMU 자세 보정값(자이로 Roll/Pitch, 부호 반전)
            + [카메라 추적 ON일 때] 카메라 PID 누적 오프셋(Pitch/Yaw)
            → min/max로 클램핑
            → servoSetTarget()에 전달 → servoSmoothUpdate()가 매 주기 조금씩 이동
```

이 함수는 원래 IMU/카메라 서비스가 값을 "밀어넣는(Push)" 콜백 구조였는데, 서로 다른 주기로 호출되는 두 서비스가 각각 제 타이밍에 값을 밀어넣다 보니 카메라 오프셋이 의도보다 여러 번 누적되어 짐벌이 한쪽으로 계속 쏠리는 문제가 있었습니다. 이를 `gimbalExecuteCombinedControl()`이 자기 주기마다 `gyroServiceGetLatestAngles()`/`cameraServiceGetPIDOffset()`을 직접 "당겨오는(Pull)" 구조로 바꿔, 제어 주기당 정확히 한 번씩만 값을 반영하도록 리팩터링했습니다(커밋 `5832647`, "change gimbal control to pull-based"). 동시에 서보 각도 제한값도 매크로 하드코딩에서 `servoGetMinAngle()`/`servoGetMaxAngle()` 런타임 조회로 바꿔, CLI로 가동 범위를 즉시 재설정할 수 있게 했습니다.

## Firmware-CAM · 컬러 트래킹

ESP32-CAM에서 HSV(색조·채도·명도) 색공간으로 빨간색 영역을 찾고, 칼만 필터로 좌표를 안정화합니다. 최소 검출 면적(`MIN_DETECT_AREA`, 50px) 이하는 노이즈로 버립니다.

이 결과(영상 프레임 + 좌표)는 SPI로 **CAM 보조보드(Board A)** 에 전달되어 LCD에 그려지고, Board A는 좌표(cx, cy, 감지여부)만 뽑아 `STX·CX·CY·감지·ETX` 7바이트 패킷으로 압축해 UART6로 **IMU 보드(Board B)** 에 넘깁니다. IMU 보드는 이 좌표를 오차(중심 80,60 대비 편차)로 변환해 PID 제어를 돌리고, 물체와의 거리(면적)에 따라 게인(Kp/Ki/Kd)을 다르게 적용합니다 — 가까울수록(면적↑) 감도를 낮추고 제동을 강하게, 멀수록 민감하게 반응하도록 튜닝돼 있습니다 (`camera_service.c`의 `cameraServicePIDUpdate()`).

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
| 카메라 추적 중 짐벌이 한쪽으로 계속 쏠림 | IMU/카메라 서비스가 서로 다른 주기로 오프셋을 Push하며 중복 누적 | `gimbalExecuteCombinedControl()`이 매 주기 직접 최신값을 Pull하는 구조로 리팩터링 (`5832647`) |

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
