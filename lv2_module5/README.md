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


## OpenCR 업로드 (Raspberry Pi, SSH)

    ssh <user>@<pi-host>
    # 빌드 · 업로드 명령

업로드 중에는 시리얼 모니터를 닫는다. 제어 프로그램과 같은 포트를 동시에 점유하지 않는다.

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

| 항목 | 규약 |
|---|---|
| 목표 토픽 | `/target` · `geometry_msgs/msg/PointStamped` |
| `point.x` / `point.y` | 정규화 중심 오차 `ex` / `ey` (오른쪽 · 아래쪽 양수) |
| `point.z` | 면적비. **`z=0` = 미검출** — 이때 x · y로 제어하지 않는다 |
| `header.stamp` | 원본 영상 시각. 촬영 시각을 모르면 "영상 수신 시각"임을 명시 |
| 발행 | 영상 처리마다. 정상 영상의 미검출도 `z=0`으로 발행 |
| QoS | best-effort, depth 1 |
| 상태 토픽 | `/tracking_status` · 값: `IDLE` `TRACKING` `LOST` |
| 모터 명령 | <TBD — 위치 / 속도 방식 · 단위 · 부호 · 주기 · 정지 명령> |
| 입력 타임아웃 | 0.5 s |

    ex = (cx - W/2) / (W/2)
    ey = (cy - H/2) / (H/2)
    area_ratio = contour_area / (W * H)
    command = clamp(direction * Kp * ex, -speed_limit, +speed_limit)   # 속도형 예

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