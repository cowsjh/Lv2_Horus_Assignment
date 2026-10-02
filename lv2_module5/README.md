비전 기반 객체 추적 시스템 실행 가이드.

작성자가 아닌 팀원이 **이 문서만 보고 실행할 수 있어야** 한다 (문제 5 필수 항목). 경로 · 명령은 실제로 돌린 것을 붙여넣는다.

## 환경

| 항목 | 값 |
|---|---|
| Raspberry Pi OS | Raspberry Pi 4 · Ubuntu 26.04 LTS |
| ROS 2 | Lyrical |
| OpenCV | opencv-python 5.0.0.93 (pip) |
| 접속 | SSH (PC는 접속과 결과 확인용) |

## 장비

| 항목 | 값 |
|---|---|
| 카메라 | Intel RealSense Depth Camera D435 (컬러 영상만 사용) |
| 카메라 해상도 | 640×480 |
| 카메라 설정 FPS | 30 (처리 FPS와 구분. 처리 FPS는 `report.md` 에서 실측) |
| 모터 모델 | xm430-w350 |
| 모터 ID | 11, 12 |
| 통신 속도 (baud) | <TBD> |
| 프로토콜 | protocol 2.0 |
| 제어 방식 (위치 / 속도) | 위치형 |
| 회전 범위 | <TBD> |
| 속도 상한 | <TBD> |
| 통신 방식 (USB 시리얼 / micro-ROS 중 하나) | USB 시리얼 |
| 전원 · 연결 · 브래킷 고정 확인 | <TBD> |

값은 **실제 장비에서 읽은 것**만 적는다. 다른 팀 값 복사 금지.

## 대상

| 항목 | 값 |
|---|---|
| 목표물 (카드 · 공 · 블록 중 1개, 색) | 블록 · 파란색 |
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
| `config/vision.yaml` | HSV 범위, 최소 면적, 해상도 |
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
