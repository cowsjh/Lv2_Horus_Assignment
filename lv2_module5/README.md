비전 기반 객체 추적 시스템 실행 가이드.

## 목차

- [환경 · 장비](#환경--장비)
- [설치](#설치)
- [실행](#실행)
- [설정 · 인터페이스](#설정--인터페이스)
- [결과 · bag](#결과--bag)
- [다른 팀원의 실행 확인](#다른-팀원의-실행-확인)

## 환경 · 장비

| 항목 | 값 |
|---|---|
| 보드 · OS | Raspberry Pi 4 · Ubuntu 26.04 LTS |
| ROS 2 | Lyrical |
| OpenCV | OpenCV 4.10.0 (`libopencv-dev`, apt) |
| 접속 | SSH (PC는 접속과 결과 확인용) |
| 보드 | OpenCR `/dev/opencr` |
| 카메라 | Intel RealSense Depth Camera D435 |
| 카메라 해상도 | 640×480 |
| 카메라 설정 FPS | 30 (처리 FPS와 구분. 처리 FPS는 `report.md` 에서 실측) |
| 모터 모델 | XM430-W350 |
| 모터 ID | 11, 12 |
| 통신 속도 (baud) | 1000000 |
| 프로토콜 | protocol 2.0 |
| 제어 방식 | 위치형 |
| 회전 범위 (안전) | pan(yaw, ID 11): 698 ~ 3502 tick, 원점 2046 기준 약 -118° ~ +128° (+ = 왼쪽) / tilt(pitch, ID 12): 2230 ~ 3033 tick, 원점 2553 기준 약 -28° ~ +42° (+ = 위쪽). 출처 `config/device.yaml` · `config.h` (tilt 는 2026-10-07 브래킷 변경 후 재측정) |
| 속도 상한 | Profile Velocity 20 (× 0.229 rpm ≈ 0.48 rad/s), 초기 시험용. 출처 `firmware/pan_tilt_fw/config.h` |
| 통신 방식 | USB 시리얼 |
| 전원 · 연결 · 브래킷 고정 확인 | 12V5A,DXL TTL-ID 11,12, 11 케이블 간섭 존재함  |
| 목표물 | 블록 · 파란색 |
| 촬영 거리 | 1.0 m (Kp 비교 · 가림 · 중단 시험 기준, 출처 [`control_test_2026-10-08.md`](https://drive.google.com/file/d/1ZxXCqlmpjfauHtnMCoKwb8SNI966TuPd/view)) |

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

## 설치

### RPi: 패키지 설치

`ros-lyrical-ros-base`, `build-essential`, `git` 이 설치된 것을 전제한다.

| 패키지 | 용도 | 설치 방법 | 버전 |
| --- | --- | --- | --- |
| `ros-lyrical-realsense2-camera` | D435 드라이버 노드 (`librealsense2` · `image-transport` · `cv-bridge` 포함) | apt | librealsense 2.58.4 |
| `ros-lyrical-cv-bridge` | `sensor_msgs/Image` → `cv::Mat` | apt | - |
| `libopencv-dev` | OpenCV (`cvtColor`·`inRange`·`findContours`·`moments`) | apt | 4.10.0 |
| `ros-lyrical-compressed-image-transport` | 디버그 영상 JPEG 압축 재발행 ([디버그 실행](#디버그-실행-image_transport--pc-rqt)) | apt | - |
| `99-realsense-libusb.rules` | D435 USB 접근 권한 | 수동 설치 (`/etc/udev/rules.d/`) | v2.58.4 |
| `arduino-cli` | 헤드리스 빌드·업로드 | 공식 설치 스크립트 | 1.5.1 |
| `gcc-arm-none-eabi`, `libnewlib-arm-none-eabi`, `libstdc++-arm-none-eabi-newlib` | 펌웨어 컴파일러 (공식 gcc 5.4 대체) | apt | 14.2.1 |
| `opencr_ld` | OpenCR 업로더. 공식 배포 실행 파일은 x86-64 전용이라, 같은 소스를 RPi에서 직접 빌드 | 공식 소스를 arm64용으로 빌드 | 1.0.4 |
| OpenCR 코어 | OpenCR 보드 지원 (`OpenCR:OpenCR:OpenCR`) | 공식 릴리스 수동 설치 + `platform.local.txt` | 1.5.3 |
| `Dynamixel2Arduino` | XM430 위치 제어 (protocol 2.0) | `arduino-cli lib install` | 0.8.2 |
| `dialout` 그룹 | `control_node` 의 시리얼 포트(`/dev/opencr`) 접근 권한 | `usermod` (재로그인 필요) | - |
| `99-opencr.rules` | OpenCR 고정 포트 `/dev/opencr` (ttyACM 번호 변동 대응) | 수동 작성 (`/etc/udev/rules.d/`) | - |

> RPi(arm64)에서 OpenCR 펌웨어를 Arduino 방식으로 빌드·업로드하려면 3가지 요소가 필요하다.
> 1. 코어
> 2. 컴파일러
> 3. 업로더
>
> `arduino-cli core install` 은 내부적으로 세 가지를 한꺼번에 설치한다.
> 하지만 공식 `arduino-cli core install OpenCR:OpenCR` 에 포함된 컴파일러·업로더는 x86-64용이라 라즈베리파이(arm64)와 아키텍처가 맞지 않으므로, 따로 설치해야 한다.

#### 설치 (순서대로)

**1) ROS · OpenCV 패키지**

```bash
sudo apt install ros-lyrical-realsense2-camera ros-lyrical-cv-bridge libopencv-dev ros-lyrical-compressed-image-transport
```

**2) RealSense udev 규칙** (librealsense 버전과 같은 태그의 파일)

```bash
curl -fL -o 99-realsense-libusb.rules https://raw.githubusercontent.com/IntelRealSense/librealsense/v2.58.4/config/99-realsense-libusb.rules
sudo cp 99-realsense-libusb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

**3) arduino-cli**

```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/.local/bin sh -s 1.5.1
export PATH="$HOME/.local/bin:$PATH"   # 재로그인하면 ~/.profile 이 자동 추가
```

**4) 컴파일러**

```bash
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```

**5) 업로더 opencr_ld 소스 빌드**

```bash
mkdir -p ~/opencr_src && cd ~/opencr_src
git clone --depth 1 --filter=blob:none --sparse https://github.com/ROBOTIS-GIT/OpenCR.git
cd OpenCR && git sparse-checkout set arduino/opencr_develop/opencr_ld
cd arduino/opencr_develop/opencr_ld && make
mkdir -p ~/opencr_src/tools && install -m755 opencr_ld ~/opencr_src/tools/
```

**6) OpenCR 코어 1.5.3 (확장자는 .tar.bz2 지만 실제로는 gzip → tar xzf)**

```bash
cd ~/opencr_src
curl -L -o opencr.tar.bz2 https://github.com/ROBOTIS-GIT/OpenCR/releases/download/1.5.3/opencr.tar.bz2
tar xzf opencr.tar.bz2
mkdir -p ~/Arduino/hardware/OpenCR/OpenCR && cp -a opencr/. ~/Arduino/hardware/OpenCR/OpenCR/
```

**7) 컴파일러·업로더 경로 지정**

```bash
cat > ~/Arduino/hardware/OpenCR/OpenCR/platform.local.txt <<EOF
compiler.path=/usr/bin/
tools.opencr_ld.path.linux=$HOME/opencr_src/tools
tools.opencr_ld.path=$HOME/opencr_src/tools
EOF
```

**8) 라이브러리**

```bash
arduino-cli lib install Dynamixel2Arduino@0.8.2
```

**9) 시리얼 권한 (재로그인 필요)**

```bash
sudo usermod -aG dialout $USER
```

**10) OpenCR 고정 포트 /dev/opencr**

```bash
sudo tee /etc/udev/rules.d/99-opencr.rules > /dev/null <<'EOF'
SUBSYSTEM=="tty", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="5740", ATTRS{serial}=="FFFFFFFEFFFF", SYMLINK+="opencr"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger --subsystem-match=tty
```

#### 확인

- `rs-enumerate-devices -s` → D435 표시 (USB 3 포트 직결, 허브 X)
- 컬러 토픽: `/camera/camera/color/image_raw`
- `arduino-cli board listall OpenCR` → `OpenCR:OpenCR:OpenCR`
- `arduino-cli lib list` → `Dynamixel2Arduino 0.8.2`
- `arduino-cli compile --fqbn OpenCR:OpenCR:OpenCR firmware/pan_tilt_fw` → 빌드 테스트
- `groups` 에 `dialout`, `ls -l /dev/opencr` → `ttyACM*` 를 가리킴

### RPi: 코드 받기 · 빌드

**Raspberry Pi 본인 clone 폴더**에서 한다. 위 [RPi: 패키지 설치](#rpi-패키지-설치) apt 설치를 먼저 끝낸다.

#### 1) 코드 받기 (최초 1회)

```bash
mkdir -p ~/<github_id> && cd ~/<github_id>
git clone https://github.com/cowsjh/Lv2_Horus_Assignment.git
```

이후에는 `cd ~/<github_id>/Lv2_Horus_Assignment && git pull --ff-only` 로 갱신한다. RPi에서는 commit · push 하지 않는다.

#### 2) 의존성 확인

```bash
source /opt/ros/lyrical/setup.bash
cd ~/<github_id>/Lv2_Horus_Assignment/lv2_module5/ros2_ws
rosdep install --from-paths src --ignore-src -y   # rosdep 최초 사용 시: sudo rosdep init && rosdep update
```

| 패키지 | 빌드 의존성 (`package.xml`) |
|---|---|
| `perception` | `rclcpp`, `sensor_msgs`, `geometry_msgs`, `cv_bridge`, `ament_index_cpp`, OpenCV, `yaml-cpp` |
| `center` | `rclcpp`, `std_msgs`, `geometry_msgs` |
| `control` | `rclcpp`, `std_msgs`, `geometry_msgs` |
| `bringup` | `realsense2_camera`, `perception`, `center`, `control` (실행 의존) |

#### 3) 빌드

```bash
source /opt/ros/lyrical/setup.bash          # 새 셸마다 먼저
cd ~/<github_id>/Lv2_Horus_Assignment/lv2_module5/ros2_ws
colcon build --symlink-install
source install/setup.bash
```

- 한 패키지만: `colcon build --symlink-install --packages-select perception`
- `--symlink-install` 이면 `config/perception.yaml` · `config/center.yaml` 수정이 재빌드 없이 반영된다(설치 경로가 원본 링크). 이 옵션 없이 빌드했다면 yaml 을 고친 뒤 다시 빌드한다.
- `build/`, `install/`, `log/` 는 커밋하지 않는다.

#### 4) 확인

```bash
ros2 pkg list | grep -E '^(perception|center|control|bringup)$'   # 4줄
ros2 pkg executables perception                                   # perception perception_node 포함
```

### RPi: OpenCR 업로드

도구 설치는 위 [RPi: 패키지 설치](#rpi-패키지-설치) 절을 먼저 끝낸다.

| 항목 | 값 |
|---|---|
| 스케치 폴더 | `firmware/pan_tilt_fw/` (폴더 이름 = `.ino` 파일 이름, 상세는 [`firmware/README.md`](firmware/README.md)) |
| FQBN | `OpenCR:OpenCR:OpenCR` |
| 포트 | `/dev/opencr` (udev 고정 이름) |
| 시리얼 baud (RPi ↔ OpenCR) | 115200 (`config.h` `PC_BAUD`) |
| 통신 타임아웃 `CMD_TIMEOUT_MS` | 500 ms (`config.h`). 올바른 `G` 명령이 이 시간 동안 없으면 hold |
| 상태 보고 주기 | 20 ms (50 Hz) |

#### 1) 접속 · 코드 갱신

```bash
ssh <user>@<pi-host>
cd ~/<사용자 폴더>/Lv2_Horus_Assignment   # 본인 clone 폴더, 공용 폴더 X
git pull --ff-only
```

#### 2) 장비 점유 확인 (하드웨어는 한 명씩)

```bash
ps -eo user,pid,etime,cmd | grep -E "ros2|arduino-cli" | grep -v grep
fuser /dev/opencr
```

두 명령 모두 출력이 없어야 진행한다. 다른 사람 프로세스가 있으면 끝날 때까지 기다린다(종료시키지 않는다).

#### 3) 빌드

```bash
cd lv2_module5
arduino-cli compile --fqbn OpenCR:OpenCR:OpenCR firmware/pan_tilt_fw
```

#### 4) 업로드

업로드 전에 제어 노드 · 시리얼 모니터가 꺼져 있는지 2)로 다시 확인한다.

```bash
arduino-cli upload -p /dev/opencr --fqbn OpenCR:OpenCR:OpenCR firmware/pan_tilt_fw
```

#### 5) 시리얼 출력 확인

```bash
arduino-cli monitor -p /dev/opencr -c baudrate=115200
# 확인 후 Ctrl+C 로 반드시 닫는다 (제어 노드와 포트 동시 점유 금지)
```

| 확인 | 기준 |
|---|---|
| 상태 출력 | `S,<pan>,<tilt>,<flags>` 줄이 50 Hz 로 나온다. 명령이 없으면 flags = 3 (HOLD + TIMEOUT). 형식은 [`firmware/README.md` 시리얼 프로토콜](firmware/README.md#시리얼-프로토콜) |
| 통신 타임아웃 정지 | 명령 없이 `CMD_TIMEOUT_MS` (500 ms) 가 지나면 flags 에 TIMEOUT(2) 비트가 켜지고 위치가 변하지 않는다 |
| 모터 설정 | ID 11 · 12, baud 1000000 이 장비 표와 같다 |

업로드 중에는 시리얼 모니터를 닫는다. 제어 프로그램과 같은 포트를 동시에 점유하지 않는다.

#### 업로드 · 시리얼 확인 기록

| 실행자 | 날짜 | 기준 커밋 | compile | upload | 시리얼 출력 (첫 줄) |
|---|---|---|---|---|---|
| | | | | | |

### PC: rqt 설치 · 코드 받기

PC 는 영상 · 토픽 확인용이다. ROS 2 Lyrical 이 설치된 Ubuntu, RPi 와 같은 네트워크. 빌드는 하지 않는다.

**1) rqt · compressed 플러그인 설치 (최초 1회)**

```bash
sudo apt install ros-lyrical-rqt ros-lyrical-rqt-image-view ros-lyrical-compressed-image-transport
```

**2) UDP 버퍼 상한 올리기 (재부팅마다)**

```bash
sudo sysctl -w net.core.rmem_max=4194304 net.core.wmem_max=4194304
```

**3) 코드 받기 (Fast DDS 설정 파일용, 최초 1회)**

```bash
git clone https://github.com/cowsjh/Lv2_Horus_Assignment.git
```

## 실행

### 일반 실행

**1) 장비 점유 확인** — 두 명령 모두 출력이 없어야 진행한다. 시리얼 모니터가 열려 있으면 닫는다.

```bash
ps -eo user,pid,etime,cmd | grep -E "ros2|arduino-cli" | grep -v grep
fuser /dev/opencr
```

**2) 실행 (터미널 1)**

```bash
source /opt/ros/lyrical/setup.bash
source ~/<github_id>/Lv2_Horus_Assignment/lv2_module5/ros2_ws/install/setup.bash
ros2 launch bringup bringup.launch.py
```

**3) 확인 (터미널 2)**

```bash
source /opt/ros/lyrical/setup.bash
ros2 topic hz /target              # 노드 로그의 처리 FPS 와 비슷
ros2 topic echo /tracking_status   # IDLE / TRACKING / LOST (상태가 바뀔 때 + 1초마다)
ros2 topic echo /motor/state       # point.z = flags, 2 비트(TIMEOUT) = 500 ms 동안 명령 없음
```

`center_node` 는 TRACKING 이 아니거나 미검출 · 데드밴드 안이면 `/motor/command` 를 보내지 않는다. 그 구간이 500 ms 를 넘으면 보드가 hold 하고 flags 에 TIMEOUT 이 켜진다 (목표가 중앙에 있을 때도 포함).

`bringup.launch.py` 가 순서대로 띄우는 것: Fast DDS 설정(`FASTDDS_DEFAULT_PROFILES_FILE`) → 카메라(color · 정렬 depth 640x480@30) → `perception_node` → `center_node`(`config/center.yaml`) → `control_node`(OpenCR 시리얼).

| 인자 | 기본값 | 용도 |
|---|---|---|
| `use_camera` | `true` | `false` = 카메라 드라이버 끔 |
| `use_control` | `true` | `false` = `control_node` 끔. **모터에 명령이 나가지 않는다** |
| `port` | `/dev/opencr` | OpenCR 시리얼 포트 |
| `debug_image` | `false` | `/target/debug_image` 발행 |
| `debug_mask` | `false` | `/target/debug_mask` 발행 |

노드를 따로 띄울 때(인지 단독 시험 등)는 [`ros2_ws/src/perception/README.md`](ros2_ws/src/perception/README.md) 를 본다.

### 디버그 실행 (image_transport · PC rqt)

모터 없이(`use_control:=false`) 인지 결과 영상을 PC 에서 본다. 먼저 [PC: rqt 설치 · 코드 받기](#pc-rqt-설치--코드-받기)를 끝낸다.

**1) RPi: 실행 (터미널 1)**

```bash
source /opt/ros/lyrical/setup.bash
source ~/<github_id>/Lv2_Horus_Assignment/lv2_module5/ros2_ws/install/setup.bash
ros2 launch bringup bringup.launch.py use_control:=false debug_image:=true debug_mask:=true
```

**2) RPi: image compressed 재발행 (터미널 2)**

```bash
source /opt/ros/lyrical/setup.bash
export FASTDDS_DEFAULT_PROFILES_FILE=~/<github_id>/Lv2_Horus_Assignment/lv2_module5/config/fastdds_shm_big.xml
ros2 run image_transport republish --ros-args -p in_transport:=raw -p out_transport:=compressed -r in:=/target/debug_image -r out/compressed:=/target/debug_image/compressed
```

**3) RPi: mask compressed 재발행 (터미널 3)**

```bash
source /opt/ros/lyrical/setup.bash
export FASTDDS_DEFAULT_PROFILES_FILE=~/<github_id>/Lv2_Horus_Assignment/lv2_module5/config/fastdds_shm_big.xml
ros2 run image_transport republish --ros-args -r __node:=mask_republisher -p in_transport:=raw -p out_transport:=compressed -r in:=/target/debug_mask -r out/compressed:=/target/debug_mask/compressed
```

**4) PC: image 보기 (PC 터미널 1)**

```bash
source /opt/ros/lyrical/setup.bash
export FASTDDS_DEFAULT_PROFILES_FILE=<PC clone 경로>/lv2_module5/config/fastdds_shm_big.xml
export ROS_DOMAIN_ID=<RPi 와 같은 값>
ros2 run rqt_image_view rqt_image_view /target/debug_image/compressed
```

**5) PC: mask 보기 (PC 터미널 2)**

```bash
source /opt/ros/lyrical/setup.bash
export FASTDDS_DEFAULT_PROFILES_FILE=<PC clone 경로>/lv2_module5/config/fastdds_shm_big.xml
export ROS_DOMAIN_ID=<RPi 와 같은 값>
ros2 run rqt_image_view rqt_image_view /target/debug_mask/compressed
```

### 중지

```bash
# 실행한 터미널에서 Ctrl+C (launch 의 모든 노드 종료)
```

- `control_node` 가 종료되면 명령이 끊기고, OpenCR 이 `CMD_TIMEOUT_MS`(500 ms) 뒤 현재 위치에서 hold 한다. 원점으로 돌아가지 않는다.
- 종료 확인: `ps -eo user,pid,cmd | grep -E "perception_node|center_node|control_node|realsense" | grep -v grep` 출력 없음, `fuser /dev/opencr` 출력 없음.

## 설정 · 인터페이스

### 설정

| 파일 | 읽는 곳 | 바꾸는 값 |
|---|---|---|
| `config/perception.yaml` | `perception_node` (설치 경로 `share/perception/config/`, `config_path` 파라미터로 바꿀 수 있음) | HSV 범위, 최소 면적, 블러 · 모폴로지 커널, bbox 형식, 리사이즈 폭, 거리 상한(`max_distance_m`, depth 사용), 채움률 하한 |
| `config/center.yaml` | `center_node` (bringup 이 `share/bringup/config/` 로 설치해 전달) | 회전 범위 `yaw_min/max` · `pitch_min/max` [tick], `yaw_kp` · `pitch_kp`, 명령 1회 최대 이동 `max_delta_tick` [tick] (현재 yaml 키가 `max_delta_tick_` 라 적용되지 않고 코드 기본값 100 사용), `deadband` [정규화 오차], `input_timeout_sec`(0.5), `resume_frames`(3), `state_check_period_sec` |
| `config/device.yaml` | 노드가 읽지 않음 (장비 실측 기록) | 축별 ID · center · 측정/안전 범위 [tick], 부호. `config.h` 의 `SAFE_MIN/MAX` · `HOME_TICK` 과 같은 값 |
| `config/fastdds_shm_big.xml` | bringup 이 `FASTDDS_DEFAULT_PROFILES_FILE` 로 지정 | 공유 메모리 · UDP 버퍼 크기 (640x480 영상 프레임 손실 방지) |
| `firmware/pan_tilt_fw/config.h` | OpenCR 펌웨어 | 보드 측 안전 범위 `SAFE_MIN/MAX` · 원점 `HOME_TICK` · 속도 `PROFILE_VEL` · `CMD_TIMEOUT_MS` · 상태 주기 (`device.yaml` 과 일치시킴, 바꾸면 재업로드) |
| (launch 인자) | `control_node` | `port`(기본 `/dev/opencr`), `baudrate`(기본 115200, 노드 파라미터) |

### `/target` 규약

| 필드 | 값 | 근거 |
|---|---|---|
| 타입 · QoS | `geometry_msgs/msg/PointStamped`, best-effort, depth 1 | 발제 |
| `point.x`, `point.y` | 정규화 오차 `e = (c − W/2) / (W/2)`, -1 ~ 1, 오른쪽 · 아래쪽이 + | 발제 |
| `point.z` | 면적비 (컨투어 면적 / 영상 면적). **0 = 미검출, 이때 x · y 로 제어 금지** | 발제 |
| `header` | 원본 영상 header 그대로 (stamp · frame_id) | 발제 |
| 주기 | 영상 1장 처리마다 1회. 타이머 재발행 없음 | 팀 |
| 무효 프레임 | 변환 실패 · NaN/inf · 범위 밖이면 발행하지 않음 (구독 측 입력 타임아웃으로 처리) | 팀 |

### 토픽

현재 코드 기준. 노드 이름은 `perception_node` (인지), `center_node` (판단), `control_node` (제어). yaw = pan (ID 11), pitch = tilt (ID 12).

| 발행 | 구독 | 토픽 | 타입 | 내용 | QoS (코드) | 근거 |
|---|---|---|---|---|---|---|
| 카메라 드라이버 | 인지 | `/camera/camera/color/image_raw` | `sensor_msgs/msg/Image` | 컬러 영상 | 구독: `SensorDataQoS` (best-effort), depth 1 | 팀 |
| 카메라 드라이버 | 인지 | `/camera/camera/aligned_depth_to_color/image_raw` | `sensor_msgs/msg/Image` | color 에 정렬된 depth. `max_distance_m` 사용 시 | 구독: `SensorDataQoS` (best-effort), depth 1 | 팀 |
| 인지 | 판단 | `/target` | `geometry_msgs/msg/PointStamped` | 위 규약 표 | best-effort, 1 (양쪽) | **발제** |
| 인지 | - | `/target/debug_image` | `sensor_msgs/msg/Image` (`bgr8`) | 컨투어 · 중심 오버레이 (표시용). `publish_debug_image:=true` 일 때만 | best-effort, 1 | 팀 |
| 인지 | - | `/target/debug_mask` | `sensor_msgs/msg/Image` (`mono8`) | open/close 까지 끝난 최종 마스크. `publish_debug_mask:=true` 일 때만 | best-effort, 1 | 팀 |
| 판단 | - | `/tracking_status` | `std_msgs/msg/String` | `IDLE` / `TRACKING` / `LOST`. 상태가 바뀔 때 + 1초마다 | reliable, 10 | 토픽명 발제 예시, 타입 팀 |
| 판단 | 제어 | `/motor/command` | `std_msgs/msg/Float64MultiArray` | `data[0]` = yaw, `data[1]` = pitch **절대 목표 위치 [tick]**. `center.yaml` 범위로 clamp. TRACKING · 검출 · 데드밴드 밖일 때만 발행 | reliable, 10 | 팀 |
| 제어 | 판단 | `/motor/state` | `geometry_msgs/msg/PointStamped` | 보드 `S` 줄 변환. `point.x` = yaw, `point.y` = pitch **실측 위치 [tick]**, `point.z` = flags, `frame_id` = `motor`, stamp = RPi 수신 시각. 약 50 Hz | reliable, 10 | 팀 |

`control_node` ↔ OpenCR 은 USB 시리얼 115200 텍스트 프로토콜이다: `/motor/command` 1건 → `G,<yaw_tick>,<pitch_tick>` 1줄, 보드 `S,<yaw_tick>,<pitch_tick>,<flags>` → `/motor/state`. 형식 · flags 는 [`firmware/README.md` 시리얼 프로토콜](firmware/README.md#시리얼-프로토콜).

## 결과 · bag

### 결과 위치

| 내용 | 경로 |
|---|---|
| 검출 · 판정 이미지 | `results/images/` |
| 원본 CSV · 상태 로그 | `results/logs/` |
| 비교 그래프 | `results/plots/` |
| 회차별 성능표 | `results/metrics.csv` |
| bag · 영상 | `recordings/README.md` |

### bag 재현

**실제 모터 출력을 비활성한 상태로** 재생한다.

기록 (백그라운드 실행 시 입력을 `/dev/null`로 연결해야 멈추지 않는다):

    ros2 bag record -o <이름> --topics /target /tracking_status /motor/command /motor/state < /dev/null

입력 재처리 — `control_node` · `center_node`를 실행하지 않고 OpenCR을 연결하지 않은 상태에서, 영상 · depth만 재생하고 검출 출력은 `/target_replay`로 바꾼다 (모든 터미널에 `FASTDDS_DEFAULT_PROFILES_FILE` 지정 후):

    ros2 run perception perception_node --ros-args -r /target:=/target_replay
    ros2 bag record -o replay_recording_02 --topics /target_replay
    ros2 bag play recording_02 -d 2 --topics /camera/camera/color/image_raw /camera/camera/aligned_depth_to_color/image_raw

결과 재분석 — bag을 재생하지 않고 저장된 `/target` · `/tracking_status` · `/motor/*`를 `rosbag2_py`로 읽어 지표를 다시 계산한다. 산식은 `report.md` 각 절.

bag 목록 · 링크 · 체크섬은 [`recordings/README.md`](recordings/README.md).

시간 기준이 필요하면 `--clock`과 관련 노드의 `use_sim_time`을 함께 적용한다. bag 시각과 현재 벽시계를 섞어 타임아웃 · 지연을 계산하지 않는다.

## 다른 팀원의 실행 확인

작성자가 아닌 팀원이 이 문서만 보고 실행한 기록.

| 확인자 | 날짜 | 기준 커밋 | 결과 | 수정한 누락 항목 |
|---|---|---|---|---|
| | | | | |
