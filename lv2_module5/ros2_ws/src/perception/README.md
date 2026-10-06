# perception — 인지 모듈 (C++)

RealSense D435 color 프레임에서 **파란색 단일 목표**를 HSV·Contour로 찾아 `/target`에 넣을 `(x, y, z)` 값을 만듭니다.
검출 로직은 ROS에 의존하지 않는 라이브러리(`perception_core`)이고, **`perception_node`** 가 D435 color 토픽을 구독해 이 라이브러리로 `/target`을 발행합니다.

## 구조

```
lv2_module5/
├── config/perception.yaml                 # HSV 범위·면적·blur·morph·bbox (튜닝 대상)
└── ros2_ws/src/perception/                # ament_cmake 패키지
    ├── CMakeLists.txt · package.xml
    ├── include/perception/detector.hpp    # 설정·결과 판정(0/NaN)·검출·오버레이 선언 (ROS 의존 없음)
    └── src/
        ├── detector.cpp                   # perception_core 라이브러리: 검출 + 오버레이
        └── perception_node.cpp            # ROS2 노드: color 구독 -> /target 발행
```

## 의존성

| 패키지 | 용도 |
| --- | --- |
| `rclcpp`, `sensor_msgs`, `geometry_msgs` | ROS2 노드, 토픽 타입 |
| `cv_bridge` | `sensor_msgs/Image`(rgb8 등) ↔ OpenCV BGR 변환 |
| `libopencv-dev` (apt) | 검출. C++은 pip `opencv-python`을 쓸 수 없으므로 apt OpenCV를 사용 |
| `yaml-cpp` | `perception.yaml` 읽기 |
| `ament_index_cpp` | 설치된 기본 설정 파일 위치 찾기 |

`ros2_ws`에서 한 번에 설치:

```bash
rosdep install --from-paths src -y --ignore-src
```

## 빌드

`lv2_module5/ros2_ws`에서:

```bash
colcon build --symlink-install --packages-select perception --cmake-args -DCMAKE_BUILD_TYPE=Release
```

`--symlink-install`을 권장합니다. 노드의 기본 설정(`install/perception/share/perception/config/perception.yaml`)이
`lv2_module5/config/perception.yaml` 원본 링크가 되어, yaml을 고치면 다시 빌드하지 않아도 반영됩니다.
이 옵션 없이 빌드하면 빌드 시점의 복사본을 읽으므로 yaml 수정 후 다시 빌드해야 합니다.

```bash
source install/setup.bash
```

## 목표 중심 = bbox 중심

영상 중심과 목표 bbox 중심을 맞추는 방식입니다(2축: `ex`, `ey` 모두 사용). bbox 형식은 `perception.yaml`의 `bbox_style`로 고릅니다.

| `bbox_style` | 함수 | 특징 |
| --- | --- | --- |
| `axis` | `cv::boundingRect` | 화면 축에 맞춘 사각형. 블록이 기울면 박스가 블록보다 커짐 |
| `rotated` (기본) | `cv::minAreaRect` | 블록 방향에 맞춰 회전한 사각형 |

화면 안에서 회전한 블록은 두 형식 모두 같은 중심을 줍니다. 블록이 앞뒤로 기울어 사다리꼴로 보이면 두 형식 모두 가까운 쪽으로 약간 치우칩니다.

## 검출 순서와 설정

| 단계 | 내용 | 설정 키 |
| --- | --- | --- |
| 1. 프레임 검사 | 비었거나 3채널 8bit가 아니면 INVALID | — |
| 2. 리사이즈 | 폭 기준, 높이는 비율 유지 | `resize_width` (`null` = 그대로) |
| 3. HSV 마스크 | 가우시안 블러 → HSV → `inRange` | `blur_ksize` (0 = 끔), `hsv_ranges` |
| 4. 마스크 정리 | open 1회(점 잡음 제거) → close 2회(구멍 메우기), 횟수는 코드 고정값 | `morph_kernel` |
| 5~6. 후보 측정·필터 | 컨투어별 면적·bbox 중심, 작은 후보 제외(`too_small`), depth 사용 시 먼 후보 제외(`too_far`), 채움률(면적 / 회전 bbox 면적)이 낮은 후보 제외(`not_box`) | `min_area_px`, `bbox_style`, `max_distance_m`, `min_fill_ratio` |
| 7. 목표 선택 | 남은 후보 중 면적이 가장 큰 것 (고정) | — |
| 8. 결과 계산 | 정규화 오차·면적비 | — |

`perception.yaml`에 모르는 키가 있거나 값이 잘못되면 노드·명령이 원인을 출력하고 시작하지 않습니다.

## `/target` 값 규칙 (0과 NaN 판정)

| 상황 | `TargetStatus` | `to_xyz()` | 노드 동작 |
| --- | --- | --- | --- |
| 목표 검출 | `Detected` | `(ex, ey, area_ratio)`, z > 0 | 발행 |
| 정상 프레임인데 목표 없음 | `NoTarget` | `(0, 0, 0)` | z=0으로 발행 |
| 프레임 손상·형식 오류, 계산값 NaN/inf | `Invalid` | 값 없음 | **발행하지 않음** (이전 값 재사용 금지 → 제어 측 타임아웃이 동작) |

- `x = y = 0`이라도 `z > 0`이면 **화면 중앙의 검출**입니다. 미검출 여부는 `z`로만 판단합니다.
- 받는 쪽(제어·통합)은 같은 규칙을 적용합니다: NaN·inf → 무효, `z <= 0` → 미검출, `|x|`·`|y|`·`z`가 1을 넘으면 → 무효. NaN이 섞이면 z=0보다 무효 판정이 우선합니다.
  C++ 노드라면 `find_package(perception)` 후 `perception::perception_core`를 링크하고 `perception::classify_target(x, y, z)`를 그대로 쓸 수 있습니다.
- 오차: `(cx, cy)` = bbox 중심, `ex=(cx-W/2)/(W/2)`, `ey=(cy-H/2)/(H/2)` (오른쪽·아래쪽 +). 면적비: `contour_area/(W×H)`. W·H는 실제 처리한 프레임 크기입니다.
- 검출기는 프레임 간 상태를 저장하지 않습니다. 미검출 프레임에서 이전 좌표를 재사용하지 않습니다.

## perception_node

| 구분 | 토픽 | 타입 | 내용 |
| --- | --- | --- | --- |
| 구독 | `image_topic` (기본 `/camera/camera/color/image_raw`) | `sensor_msgs/msg/Image` | D435 color. `cv_bridge`로 BGR 변환 후 검출. QoS sensor data(best-effort) |
| 구독 (선택) | `depth_topic` (기본 `/camera/camera/aligned_depth_to_color/image_raw`) | `sensor_msgs/msg/Image` (16UC1, mm) | color에 정렬된 depth. `max_distance_m`이 있을 때만 구독, stamp 차이 50 ms 이내만 사용 |
| 발행 | `/target` | `geometry_msgs/msg/PointStamped` | x=`ex`, y=`ey`, z=면적비. `header` = 원본 영상 header. QoS best-effort depth 1 |
| 발행 (선택) | `/target/debug_image` | `sensor_msgs/msg/Image` (`bgr8`) | bbox·중심 오버레이. `publish_debug_image:=true`일 때만 |
| 발행 (선택) | `/target/debug_mask` | `sensor_msgs/msg/Image` (`mono8`) | open/close까지 끝난 최종 마스크. `publish_debug_mask:=true`일 때만 |

| 파라미터 | 기본값 | 설명 |
| --- | --- | --- |
| `config_path` | `""` (패키지 기본 설정) | 비우면 빌드 때 설치된 `lv2_module5/config/perception.yaml`. 다른 파일을 쓸 때만 지정. `~/...`, 상대 경로(실행 폴더 기준) 가능 |
| `image_topic` | `/camera/camera/color/image_raw` | 구독할 color 토픽 |
| `depth_topic` | `/camera/camera/aligned_depth_to_color/image_raw` | 구독할 정렬 depth 토픽 (`max_distance_m`이 있을 때만) |
| `publish_debug_image` | `false` | 오버레이 영상 발행. 처리 FPS를 잴 때는 끄기 |
| `publish_debug_mask` | `false` | 마스크 영상 발행. 처리 FPS를 잴 때는 끄기 |
| `debug_image_every_n` | `1` | 디버그 영상(오버레이·마스크)을 n 프레임마다 발행 |
| `stats_period_sec` | `5.0` | 처리 FPS·검출 수 로그 주기 (0이면 끔) |

발행 규칙: 영상이 들어올 때마다 한 번 발행하고(타이머 재발행 없음), 미검출은 `(0, 0, 0)`, 변환 실패·무효 프레임은 발행하지 않습니다(5초에 한 번 경고).

```bash
ros2 run perception perception_node
```

튜닝용 yaml을 따로 쓸 때:

```bash
ros2 run perception perception_node --ros-args -p config_path:=~/tuning.yaml
```

시작 로그로 실제 적용된 설정을 확인합니다. 기본 설정이 원본 링크면 `->` 뒤에 원본 위치가 나오고, 없으면 빌드 때 복사본입니다.

```
[INFO] [perception_node]: 구독: /camera/camera/color/image_raw -> 발행: /target
[INFO] [perception_node]: 설정: .../install/perception/share/perception/config/perception.yaml -> .../lv2_module5/config/perception.yaml (기본값)
[INFO] [perception_node]: 검출 설정: HSV [98,120,40]~[130,255,255], blur 5, morph 5 (open 1, close 2), min_area 300px, bbox rotated, resize 원본
```

오버레이·마스크 영상까지 켜기:

```bash
ros2 run perception perception_node --ros-args -p publish_debug_image:=true -p publish_debug_mask:=true -p debug_image_every_n:=3
```

PC에서 나란히 보기 (같은 네트워크·`ROS_DOMAIN_ID`): `rqt` 실행 → Plugins → Visualization → Image View를 두 번 추가하고 각각 `/target/debug_image`, `/target/debug_mask` 선택.
창을 따로 띄우려면 `ros2 run rqt_image_view rqt_image_view /target/debug_image`(마스크도 같은 방식).

bag 재처리(발제 문제 5) 때는 저장된 `/target`과 섞이지 않게 출력 토픽을 바꿉니다:

```bash
ros2 run perception perception_node --ros-args -r /target:=/target_replay
```

확인:

```bash
ros2 topic hz /target
```

```bash
ros2 topic echo /target
```

D435 토픽 이름은 `realsense2_camera` 버전에 따라 다를 수 있으니 `ros2 topic list`로 확인하고, 다르면 `-p image_topic:=...`으로 지정합니다.

## 남은 작업

- [ ] 라즈베리파이 ROS2 영상으로 HSV 값 재확인 (현재 값은 PC 촬영 영상 기준)
- [ ] 라즈베리파이에서 perception_node 실행 확인 (`ros2 topic hz /target`으로 처리 FPS 확인)
- [ ] 파란 옷 등 오검출 방지 조건 추가 (조건 확정 후 반영)
- [ ] 정상·대상 없음·완전 가림 3장면 원본·마스크·오버레이 저장 → `lv2_module5/results/images/`, CSV → `lv2_module5/results/logs/`
- [ ] 평가 프레임: 목표가 보이는 프레임 30장 이상, 목표 없는 프레임 10장 이상 정답 대조 → 검출률·배경 오검출
- [ ] 통합 담당과 `/target` 규약(위 표) 확인
