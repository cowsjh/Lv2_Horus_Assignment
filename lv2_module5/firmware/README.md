# firmware

OpenCR에서 DYNAMIXEL XM430-W350 두 개(yaw ID 11, pitch ID 12)를 제어하는 펌웨어.
ROS 2 쪽 제어 노드가 USB 시리얼(`/dev/opencr`)로 명령을 보내고 상태를 받는다.

빌드 · 업로드 도구 설치와 절차는 [`lv2_module5/README.md`의 "제어 (OpenCR · Arduino)" · "OpenCR 업로드"](../README.md) 절을 따른다.

## 파일

| 경로 | 내용 |
|---|---|
| `pan_tilt_fw/pan_tilt_fw.ino` | 시작 파일. `setup()` · `loop()`만 있음 (arduino-cli 규칙상 폴더명과 같은 .ino 필요) |
| `pan_tilt_fw/config.h` | 설정값 (ID, 안전 범위, 속도, 타임아웃, flags) |
| `pan_tilt_fw/motor.h` · `motor.cpp` | 모터 초기화, 이동, 정지(hold), 클램프, 상태 읽기 |
| `pan_tilt_fw/protocol.h` · `protocol.cpp` | G · H 파싱, 타임아웃 판정, S 상태 보고 |
| `tools/dxl_check/` | 장비 확인용 스케치 (ID 스캔, 설정 읽기, 손으로 돌리며 범위 측정) |

언어: C++ (Arduino 코어, arm-none-eabi-g++). 같은 폴더의 .cpp · .h는 arduino-cli가 함께 빌드한다.

## 시리얼 프로토콜

USB 시리얼 115200, 한 줄 = 한 메시지, 줄 끝은 `\r` 또는 `\n` (빈 줄은 무시). 위치 단위는 모터 tick(0~4095).

| 메시지 | 방향 | 형식 | 의미 |
|---|---|---|---|
| G | 상위 → 보드 | `G,<yaw_tick>,<pitch_tick>` | 목표 위치로 이동 |
| H | 상위 → 보드 | `H` | 정지. 정지 순간의 현재 위치를 목표로 고정 |
| S | 보드 → 상위 | `S,<yaw_tick>,<pitch_tick>,<flags>` | 50 Hz 상태 보고. 위치는 실측값 |
| I | 보드 → 상위 | `I,...` | 부팅 · 정보 메시지 (무시해도 됨) |

flags (비트 합, 예: 3 = HOLD + TIMEOUT)

| 값 | 이름 | 의미 |
|---|---|---|
| 1 | HOLD | 정지 중 |
| 2 | TIMEOUT | 올바른 G · H가 500 ms 동안 없어서 정지 |
| 4 | LIMIT | 마지막 G가 안전 범위 밖이라 잘라서 적용 |
| 8 | HW_ERROR | 모터 통신 실패 또는 Hardware Error Status 발생 |

규칙

- 상위는 추적 중이든 정지 중이든 G 또는 H를 **주기적으로**(예: 30 Hz) 보낸다.
- 보드는 **파싱에 성공한** G · H만 받은 것으로 친다. 형식이 틀린 줄은 무시하고 타이머도 리셋하지 않는다.
- 500 ms 동안 올바른 명령이 없으면 hold. 끊긴 뒤 자동으로 원점 복귀하지 않는다.
- 범위 밖 목표는 안전 범위로 잘라서 적용하고 LIMIT을 표시한다.
- 정지 여부는 명령이 아니라 S의 실측 위치가 변하지 않는 것으로 판단한다.

## 설정값

| 항목 | 값 | 비고 |
|---|---|---|
| 스케치 폴더 | `firmware/pan_tilt_fw` | |
| 시리얼 baud (RPi ↔ OpenCR) | 115200 | |
| 명령 타임아웃 `CMD_TIMEOUT_MS` | 500 ms | |
| 상태 보고 주기 | 20 ms (50 Hz) | |
| 동작 모드 | 위치 제어(3), 두 축 모두 | 부팅 시 설정 |
| 안전 범위 [tick] | yaw 698~3502, pitch 2230~3033 | `config.h`. `config/device.yaml`과 일치시킬 것. 모터 Min/Max Position Limit에도 기록. pitch는 브래킷 변경 후 재측정 |
| 부팅 시 원점 이동 | 켜짐, 원점 yaw 2046 · pitch 2553 | `HOME_ON_BOOT`, `HOME_TICK`. 전원 · RESET 때만 Profile Velocity 속도로 이동. 통신 끊김 · 정지 상태에서는 원점으로 가지 않음 |
| Profile Velocity | 20 (약 0.48 rad/s) | 초기 시험용 낮은 값 |
| Bus Watchdog | 0 (사용 안 함) | v1. 위치 모드라 보드가 멈춰도 마지막 목표 근처에서 정지 |

## 단독 시험 (ROS 없이)

시작 전: ModemManager가 `ttyACM` 장치를 모뎀으로 착각해 잡음을 보내므로 끈다.

```bash
sudo systemctl stop ModemManager && sudo systemctl disable ModemManager
```

카메라를 손으로 받칠 수 있는 상태에서, 낮은 속도로 진행한다.
시작 전 `fuser /dev/opencr`로 다른 프로세스가 포트를 쓰고 있지 않은지 확인한다.

1. 상태 확인

   ```bash
   picocom -b 115200 --omap crlf /dev/opencr
   ```

   `S,<yaw>,<pitch>,3`이 50 Hz로 나오면 정상 (명령이 없으니 HOLD + TIMEOUT).
   부팅 직후에는 위치가 원점(2046, 2553)으로 천천히 이동하는 것이 보인다 (`I,homing_on_boot`).
   `--omap crlf`가 없어도 Enter(`\r`)가 줄 끝으로 인식된다. 종료: `Ctrl+A` 후 `Ctrl+X`.

2. 타임아웃 확인: picocom에서 `G,2100,2553` 입력 후 Enter
   - yaw가 조금 움직이다가 약 0.5 s 뒤 멈추고 flags가 0 → 3으로 바뀌면 정상.

3. 연속 명령 · 통신 중단 확인 (picocom 종료 후, 터미널 두 개)

   ```bash
   # 터미널 A: 상태 보기 (로그 저장)
   stty -F /dev/opencr 115200 raw -echo
   cat /dev/opencr | tee fw_test_$(date +%Y%m%d_%H%M%S).txt
   ```

   ```bash
   # 터미널 B: 20 Hz로 같은 목표 보내기
   while true; do echo "G,2100,2553"; sleep 0.05; done > /dev/opencr
   ```

   - 목표에 도달해 머물고 flags = 0이면 정상.
   - 터미널 B에서 `Ctrl+C` → 0.5 s 안에 flags = 3, 위치 변화 없음 → 제어 통신 중단 정지 확인.

4. 범위 제한 확인: 터미널 B에서 `G,2046,3500` (pitch 안전 최대 3033 초과)
   - pitch가 3033 부근에서 멈추고 flags = 4이면 정상.

5. 기본 자세로 복귀: `G,2046,2553`
