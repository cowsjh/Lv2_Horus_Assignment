비전 기반 객체 추적 시스템 실행 가이드.

작성자가 아닌 팀원이 **이 문서만 보고 실행할 수 있어야** 한다 (문제 5 필수 항목). 경로 · 명령은 실제로 돌린 것을 붙여넣는다.

## 환경

| 항목 | 값 |
|---|---|
| 보드 · OS | Raspberry Pi 4 · Ubuntu 26.04 LTS |
| ROS 2 | Lyrical |
| OpenCV | OpenCV 4.10.0 (`libopencv-dev`, apt) |
| 접속 | SSH (PC는 접속과 결과 확인용) |


## 장비

| 항목 | 값 |
|---|---|
| 보드 | OpenCR `/dev/opencr` |
| 카메라 | Intel RealSense Depth Camera D435 |
| 카메라 해상도 | 640×480 |
| 카메라 설정 FPS | 30 (처리 FPS와 구분. 처리 FPS는 `report.md` 에서 실측) |
| 모터 모델 | XM430-W350 |
| 모터 ID | 11, 12 |
| 통신 속도 (baud) | 1000000 |
| 프로토콜 | protocol 2.0 |
| 제어 방식 (위치 / 속도) | 위치형 |
| 회전 범위 | yaw:-180, 180 / pitch: -30, 90 |
| 속도 상한 | <TBD> |
| 통신 방식 (USB 시리얼 / micro-ROS 중 하나) | USB 시리얼 |
| 전원 · 연결 · 브래킷 고정 확인 | 12V5A,DXL TTL-ID 11,12, 11 케이블 간섭 존재함  |

값은 **실제 장비에서 읽은 것**만 적는다. 다른 팀 값 복사 금지.


## 패키지

### 인지 (RealSense · OpenCV)

아래 패키지는 `ros-lyrical-ros-base` 가 설치된 것을 전제한다.

| 패키지 | 용도 | 의존성 (포함된 것) |
| --- | --- | --- |
| `ros-lyrical-realsense2-camera` | D435 드라이버 노드 | `ros-lyrical-librealsense2`, `ros-lyrical-realsense2-camera-msgs`, `ros-lyrical-image-transport`, `ros-lyrical-cv-bridge` |
| `ros-lyrical-cv-bridge` | `sensor_msgs/Image` → `cv::Mat` | `libopencv-dev` |
| `libopencv-dev` | OpenCV 4.10.0 (`cvtColor`·`inRange`·`findContours`·`moments`) | - |
| `ros-lyrical-compressed-image-transport` | 디버그 영상 JPEG 압축 → PC RViz 시연 토픽 `/debug_image` 용 | `ros-lyrical-cv-bridge`, `ros-lyrical-image-transport` |

```bash
sudo apt install ros-lyrical-realsense2-camera ros-lyrical-cv-bridge libopencv-dev ros-lyrical-compressed-image-transport
```

#### udev 규칙 (apt에 없음, 수동 설치)

librealsense 버전(`ros-lyrical-librealsense2` = 2.58.4)과 같은 태그의 파일을 사용한다.

```bash
curl -fL -o 99-realsense-libusb.rules https://raw.githubusercontent.com/IntelRealSense/librealsense/v2.58.4/config/99-realsense-libusb.rules
sudo cp 99-realsense-libusb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

#### 확인

- `rs-enumerate-devices -s` → D435 표시 (USB 3 포트 직결, 허브 X)
- 컬러 토픽: `/camera/camera/color/image_raw`

### 제어 (OpenCR · Arduino)

> RPi(arm64)에서 OpenCR 펌웨어를 Arduino 방식으로 빌드·업로드하려면 3가지 요소가 필요하다.
> 1. 코어
> 2. 컴파일러
> 3. 업로더
>
> `arduino-cli core install` 은 내부적으로 세 가지를 한꺼번에 설치한다.
> 하지만 공식 `arduino-cli core install OpenCR:OpenCR` 에 포함된 컴파일러·업로더는 x86-64용이라 라즈베리파이(arm64)와 아키텍처가 맞지 않으므로, 따로 설치해야 한다.

아래는 `build-essential`, `git` 이 설치된 것을 전제한다.

| 패키지 | 용도 | 설치 방법 | 버전 |
| --- | --- | --- | --- |
| `arduino-cli` | 헤드리스 빌드·업로드 | 공식 설치 스크립트 | 1.5.1 |
| `gcc-arm-none-eabi`, `libnewlib-arm-none-eabi`, `libstdc++-arm-none-eabi-newlib` | 펌웨어 컴파일러 (공식 gcc 5.4 대체) | apt | 14.2.1 |
| `opencr_ld` | OpenCR 업로더. 공식 배포 실행 파일은 x86-64 전용이라, 같은 소스를 RPi에서 직접 빌드 | 공식 소스를 arm64용으로 빌드 | 1.0.4 |
| OpenCR 코어 | OpenCR 보드 지원 (`OpenCR:OpenCR:OpenCR`) | 공식 릴리스 수동 설치 + `platform.local.txt` | 1.5.3 |
| `Dynamixel2Arduino` | XM430 위치 제어 (protocol 2.0) | `arduino-cli lib install` | 0.8.2 |
| `dialout` 그룹 | 브리지 노드의 시리얼 포트(`/dev/opencr`) 접근 권한 | `usermod` (재로그인 필요) | - |
| `99-opencr.rules` | OpenCR 고정 포트 `/dev/opencr` (ttyACM 번호 변동 대응) | 수동 작성 (`/etc/udev/rules.d/`) | - |

#### 설치 (순서대로)

```bash
# 1) arduino-cli
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/.local/bin sh -s 1.5.1
export PATH="$HOME/.local/bin:$PATH"   # 재로그인하면 ~/.profile 이 자동 추가

# 2) 컴파일러
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib

# 3) 업로더 opencr_ld 소스 빌드
mkdir -p ~/opencr_src && cd ~/opencr_src
git clone --depth 1 --filter=blob:none --sparse https://github.com/ROBOTIS-GIT/OpenCR.git
cd OpenCR && git sparse-checkout set arduino/opencr_develop/opencr_ld
cd arduino/opencr_develop/opencr_ld && make
mkdir -p ~/opencr_src/tools && install -m755 opencr_ld ~/opencr_src/tools/

# 4) OpenCR 코어 1.5.3 (확장자는 .tar.bz2 지만 실제로는 gzip → tar xzf)
cd ~/opencr_src
curl -L -o opencr.tar.bz2 https://github.com/ROBOTIS-GIT/OpenCR/releases/download/1.5.3/opencr.tar.bz2
tar xzf opencr.tar.bz2
mkdir -p ~/Arduino/hardware/OpenCR/OpenCR && cp -a opencr/. ~/Arduino/hardware/OpenCR/OpenCR/

# 5) 컴파일러·업로더 경로 지정
cat > ~/Arduino/hardware/OpenCR/OpenCR/platform.local.txt <<EOF
compiler.path=/usr/bin/
tools.opencr_ld.path.linux=$HOME/opencr_src/tools
tools.opencr_ld.path=$HOME/opencr_src/tools
EOF

# 6) 라이브러리
arduino-cli lib install Dynamixel2Arduino@0.8.2

# 7) 시리얼 권한 (재로그인 필요)
sudo usermod -aG dialout $USER

# 8) OpenCR 고정 포트 /dev/opencr
sudo tee /etc/udev/rules.d/99-opencr.rules > /dev/null <<'EOF'
SUBSYSTEM=="tty", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="5740", ATTRS{serial}=="FFFFFFFEFFFF", SYMLINK+="opencr"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger --subsystem-match=tty
```

#### 확인

- `arduino-cli board listall OpenCR` → `OpenCR:OpenCR:OpenCR`
- `arduino-cli lib list` → `Dynamixel2Arduino 0.8.2`
- `arduino-cli compile --fqbn OpenCR:OpenCR:OpenCR <스케치 폴더>` → 빌드 테스트
- `groups` 에 `dialout`, `ls -l /dev/opencr` → `ttyACM*` 를 가리킴


## 대상

| 항목 | 값 |
|---|---|
| 목표물 | 블록 · 파란색 |
| 촬영 거리 | <TBD> |

## 설치 · 빌드

    # PC가 아니라 Raspberry Pi 개인 clone 폴더에서
    cd lv2_module5/ros2_ws
    colcon build --symlink-install
    source install/setup.bash

### 모터 측정값 (펌웨어 `firmware/pan_tilt_fw/config.h`의 근거)

측정 방법: `firmware/tools/dxl_check`, 토크 off 상태에서 손으로 회전하며 끝값 기록.
안전 범위 = 측정 끝값에서 100 tick(약 8.8°) 안쪽. 끝값은 브래킷 · 케이블이 닿기 직전이라 여유를 둔다.

| 축 | ID | 정면 (기본 자세) | 측정 끝값 | 안전 범위 | tick 증가 방향 | 측정일 |
|---|---|---|---|---|---|---|
| yaw | 11 | 2046 | 598 ~ 3602 | 698 ~ 3502 | 왼쪽 | 2026-10-06 |
| pitch | 12 | 2553 | 2130 ~ 3133 | 2230 ~ 3033 | 위쪽 | 2026-10-07 (브래킷 변경 후, 정면은 유지) |

- 공통: XM430-W350 (model 1020), Protocol 2.0, 1,000,000 bps
- 각도 변환: rad = (tick − 정면) × 2π / 4096 (1 tick ≈ 0.088°)
- 전원 · RESET 시 정면 위치로 천천히 이동 (`HOME_ON_BOOT`)
- 측정 로그: `results/logs/dxl_check_2026-10-06.txt`, `results/logs/dxl_check_pitch_bracket_2026-10-07.txt`
- 값을 바꾸면 `config.h`의 `SAFE_MIN` · `SAFE_MAX` · `HOME_TICK`을 함께 수정하고 펌웨어를 다시 업로드한다 (펌웨어는 이 표나 yaml을 읽지 않는다)

## OpenCR 업로드 (Raspberry Pi, SSH)

도구 설치는 위 [제어 (OpenCR · Arduino)](#제어-opencr--arduino) 절을 먼저 끝낸다.

| 항목 | 값 |
|---|---|
| 스케치 폴더 | `firmware/<TBD 스케치 이름>/` (폴더 이름 = `.ino` 파일 이름) |
| FQBN | `OpenCR:OpenCR:OpenCR` |
| 포트 | `/dev/opencr` (udev 고정 이름) |
| 시리얼 baud (RPi ↔ OpenCR) | <TBD — 펌웨어 `Serial.begin` 값과 같게> |
| 통신 타임아웃 `TIMEOUT_MS` | <TBD> |

### 1) 접속 · 코드 갱신

```bash
ssh <user>@<pi-host>
cd ~/<사용자 폴더>/Lv2_Horus_Assignment   # 본인 clone 폴더, 공용 폴더 X
git pull --ff-only
```

### 2) 장비 점유 확인 (하드웨어는 한 명씩)

```bash
ps -eo user,pid,etime,cmd | grep -E "ros2|arduino-cli" | grep -v grep
fuser /dev/opencr
```

두 명령 모두 출력이 없어야 진행한다. 다른 사람 프로세스가 있으면 끝날 때까지 기다린다(종료시키지 않는다).

### 3) 빌드

```bash
cd lv2_module5
arduino-cli compile --fqbn OpenCR:OpenCR:OpenCR firmware/<TBD 스케치>
```

### 4) 업로드

업로드 전에 제어 노드 · 시리얼 모니터가 꺼져 있는지 2)로 다시 확인한다.

```bash
arduino-cli upload -p /dev/opencr --fqbn OpenCR:OpenCR:OpenCR firmware/<TBD 스케치>
```

### 5) 시리얼 출력 확인

```bash
arduino-cli monitor -p /dev/opencr -c baudrate=<TBD>
# 확인 후 Ctrl+C 로 반드시 닫는다 (제어 노드와 포트 동시 점유 금지)
```

| 확인 | 기준 |
|---|---|
| 상태 출력 | `S,<yaw>,<pitch>,<flag>` 줄이 주기적으로 나온다 (형식은 [인터페이스](#인터페이스) 시리얼 프로토콜) |
| 통신 타임아웃 정지 | 명령 없이 `TIMEOUT_MS` 가 지나면 `flag` 가 정지(hold) 상태를 표시한다 |
| 모터 설정 | ID 11 · 12, baud 1000000 이 장비 표와 같다 |

업로드 중에는 시리얼 모니터를 닫는다. 제어 프로그램과 같은 포트를 동시에 점유하지 않는다.

### 업로드 · 시리얼 확인 기록

| 실행자 | 날짜 | 기준 커밋 | compile | upload | 시리얼 출력 (첫 줄) |
|---|---|---|---|---|---|
| | | | | | |

## 실행

    # 1. 카메라
    # 2. 인지 노드
    # 3. 제어 노드

## 중지

    # 정상 중지

## 설정

| 파일 | 바꾸는 값 |
|---|---|
| `config/perception.yaml` | HSV 범위, 최소 면적, 해상도 |
| `config/control.yaml` | Kp, direction, speed_limit, 회전 범위, 데드밴드, 제어 주기 |
| `config/safety.yaml` | 입력 타임아웃, 복귀 프레임 수, 상태 유예 |

## 인터페이스

| 발행 | 구독 | 토픽 | 타입 | 내용 | QoS | 근거 |
|---|---|---|---|---|---|---|
| 카메라 드라이버 | 인지 | `/camera/camera/color/image_raw` | `sensor_msgs/msg/Image` | 컬러 영상. stamp는 수신 시각임을 명시 | best-effort, 1 | 팀 |
| 인지 | 판단 | `/target` | `geometry_msgs/msg/PointStamped` | 위 규약 표 | best-effort, 1 | **발제** |
| 인지 | 기록용 | `/target_replay` | `geometry_msgs/msg/PointStamped` | bag 입력 재처리 출력. 저장된 `/target` 과 분리 | best-effort, 1 | 발제 (별도 토픽) |
| 인지 | - | `/debug_image` | `sensor_msgs/msg/Image` | 컨투어 · 중심 오버레이 (표시용) | best-effort, 1 | 팀 |
| 판단 | - | `/tracking_status` | `std_msgs/msg/String` | `IDLE` / `TRACKING` / `LOST` (선택 `SEARCHING`) | reliable, 10 | 토픽명 발제 예시, 타입 팀 |
| 판단 | 제어 | `/motor_cmd` | `sensor_msgs/msg/JointState` | `name=[yaw, pitch]`, `position` = 목표 각도 [rad] | best-effort, 1 | 팀 |
| 제어 | 판단 | `/motor_status` | `std_msgs/msg/String` | `OK` / `LIMIT` / `TIMEOUT_STOP` | reliable, 10 | 팀 |
| 제어 | 판단 | `/motor_state` | `sensor_msgs/msg/JointState` | `position` = **실측** 각도 [rad] | best-effort, 1 | 팀 |
| - | - | `/search` | action | 요청 · 진행 · 성공/실패 · 취소 | - | 발제 (선택) |

`/target`, `/motor_*` 는 양쪽 모두 best-effort 로 맞춘다.

## bag 재현

**실제 모터 출력을 비활성한 상태로** 재생한다.

    # 기록
    # 재생 (입력 재처리 — 저장된 /target 과 섞지 않도록 remap)
    # 재생 (결과 재분석)

시간 기준이 필요하면 `--clock`과 관련 노드의 `use_sim_time`을 함께 적용한다. bag 시각과 현재 벽시계를 섞어 타임아웃 · 지연을 계산하지 않는다.

## 결과 위치

| 내용 | 경로 |
|---|---|
| 검출 · 판정 이미지 | `results/images/` |
| 원본 CSV · 상태 로그 | `results/logs/` |
| 비교 그래프 | `results/plots/` |
| 회차별 성능표 | `results/metrics.csv` |
| bag · 영상 | `recordings/README.md` |

## 다른 팀원의 실행 확인

문제 5 필수 항목. 작성자가 아닌 사람이 이 문서만 보고 실행한 기록.

| 확인자 | 날짜 | 기준 커밋 | 결과 | 수정한 누락 항목 |
|---|---|---|---|---|
| | | | | |
