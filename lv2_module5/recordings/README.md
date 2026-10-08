bag · 영상의 위치, 메타데이터, 체크섬, 재생 방법.

**bag 파일은 레포에 커밋하지 않는다.** 대표 bag(`success_01` · `fail_01` · `recording_01` · `recording_02`)은 [Drive `recordings`](https://drive.google.com/drive/folders/1w0o_TE5jin6PnPpRETwN2CTSciRm4nxe), 시험 기록 bag은 아래 각 링크에 `metadata.yaml`과 `.mcap`을 폴더째 올리고, 이 문서에서 링크와 SHA-256으로 연결한다.

Drive 구성 (`result/송정혁` 폴더: [링크](https://drive.google.com/drive/folders/1K771K-eEIXJ_XX0-L-Czj5EHuL6AmCt4)):

| 폴더 · 파일 | 내용 | 올린 사람 |
|---|---|---|
| [`horus_test_upload/`](https://drive.google.com/drive/folders/16n5mu4kALtGOVyiznzKoU5q1Jsuez-gp) | 제어 시험 bag (`s3/` Kp 비교, `s5/` 가림, `s6/` 중단 · 모의 입력) + 메모 · 타임라인, `center_log.txt` | 한지훈 |
| [`한지훈_시험결과_2026-10-08.md`](https://drive.google.com/file/d/1ZxXCqlmpjfauHtnMCoKwb8SNI966TuPd/view) | 제어 시험 조건 · 결과 · 무효 기록 (레포 사본: [`control_test_2026-10-08.md`](https://drive.google.com/file/d/1ZxXCqlmpjfauHtnMCoKwb8SNI966TuPd/view)) | 한지훈 |
| [`run1/`](https://drive.google.com/drive/folders/1mgMrO5zTb70CAQpCfJ_Xc6RaY3LmQw-w) · [`run2/`](https://drive.google.com/drive/folders/13UOgnmz3hgWHui-kgTE6Lv0kh3IlYQCK) | `recording_02` 입력 재처리 1차 · 2차 (재처리 bag, 원본 · 재처리 CSV, [`compare_result.txt`](https://drive.google.com/file/d/1ena8iebdDZGpU2AfpDpssNBBgL87h1oR/view), 1차는 [`perception_node.log`](https://drive.google.com/file/d/1xjNHA7FwoppYLvdk02OByxHUBtxm4F34/view) 포함) | 정조은 |
| [`s7_memo.txt`](https://drive.google.com/file/d/1iGOthntzHd6mILcthx7pY0fZarG1l-cM/view) · [`s7_replay_target.csv`](https://drive.google.com/file/d/1e18WerSBKFR8R4dQQZesQHLQKDxOpUWU/view) | 입력 재처리 조건 · 명령 · 결과, 프레임별 원본 `/target` 대 `/target_replay` 비교 (run1 기준, 447행) | 정조은 |
| [`recordings/recording_01/`](https://drive.google.com/drive/folders/1k68sCKGIDgC3D2X2TCMQIgybKopmOP4f) (팀 폴더 상위) | 추적 기록 원본 (영상 없음) | 송정혁 |
| [`recordings/recording_02/`](https://drive.google.com/drive/folders/19doHdXVlC-jyX4Li0xoxBHkLZhPcSGlg) (팀 폴더 상위) | 영상 포함 녹화 원본 | 송정혁 |

## bag 목록

크기 · SHA-256은 `.mcap` 데이터 파일 기준이다. 폴더 링크에 `metadata.yaml`이 함께 있다.

### 대표 장면 (발제 산출물 4: 성공 · 소실 장면 10~30 s)

| 용도 | bag | 기간 | 크기 | SHA-256 (`.mcap`) | 접근 위치 |
|---|---|---|---|---|---|
| 성공 장면 | `success_01` | 22.7 s | 558,260 B | `4ea2bb8fa0ceef548ea5cbddba4b5ffe5149e5628643c5a00eed24c5004bc28e` | [Drive](https://drive.google.com/drive/folders/1nI5ogXvCStUKdzxoeBcdEZFxO9zdq5b6) |
| 소실 장면 | `fail_01` | 28.8 s | 677,721 B | `9fea8a160249d0cbac95431848881271abaaa5b75a2587458616c9dbce03d8ee` | [Drive](https://drive.google.com/drive/folders/1V3R4RshzOpaQ_fuv0VQFtukzv4WBCi7o) |
| 추적 기록 (19:50:18~19:50:33, 영상 없음) | `recording_01` | 14.6 s | 161,215 B | `ca8e6c181bfba36a2fb2517ccd15e7c0a1db9dcc26ba4e4a787302d2f4ca8f82` | [Drive](https://drive.google.com/drive/folders/1k68sCKGIDgC3D2X2TCMQIgybKopmOP4f) |
| 영상 포함 녹화 (입력 재처리용, 18:22:37~18:22:52) | `recording_02` | 14.9 s | 686,586,741 B | `25116fa50b00bafa90e4f8eaec258ba388d97ccbd1b883181ff226a0fa1f4eb6` | [Drive](https://drive.google.com/drive/folders/19doHdXVlC-jyX4Li0xoxBHkLZhPcSGlg) |

### 시험 기록 (2026-10-08, 실행자 한지훈)

| 시험 | bag | 기간 | 크기 | SHA-256 (`.mcap`) | 접근 위치 |
|---|---|---|---|---|---|
| Kp 20 1회 | `s3_kp20_run1` | 17.0 s | 197,068 B | `4526e3e0f00feecabfb7df5fdccf32bcfe18590eac27bf103c4f8afe3c3f218a` | [Drive](https://drive.google.com/drive/folders/1g6uL3ryRw_RTfGbDrcrveQzDkypNLTEM) |
| Kp 20 2회 | `s3_kp20_run2` | 17.4 s | 192,187 B | `3e3e07076fff355012b70dc363f7df33d43ce6ce9e6ff228844d86c8d742a03d` | [Drive](https://drive.google.com/drive/folders/1po3HFnhZCL4_nil9YPEOtkGav1wXRG5k) |
| Kp 20 3회 | `s3_kp20_run3` | 17.2 s | 202,747 B | `e2f9860a798885b529bfb4355ced7268405de2bb61f192b99c7e82957d5c49ad` | [Drive](https://drive.google.com/drive/folders/1nOq_GEF7ZsPBSG-rcwr4VTqmTbd7aNZ7) |
| Kp 40 1회 | `s3_kp40_run1` | 17.5 s | 202,634 B | `3b64f16df2b536d4ad590aef31435504257658431845dbd75c45191c4786a660` | [Drive](https://drive.google.com/drive/folders/1f6BWV5FmhsAKZv4VCiD90nc6lipX2HvG) |
| Kp 40 2회 | `s3_kp40_run2` | 17.4 s | 205,842 B | `c5dcc5aaba776432abc571513e045576419abfe1a618384ec82fa5a70f761768` | [Drive](https://drive.google.com/drive/folders/1l02hOSA3bOInnVn_reF4gzKdtbfumGHp) |
| Kp 40 3회 | `s3_kp40_run3` | 16.6 s | 188,946 B | `39ef14dd98c21f46fea842554775dff71a7debe6769bce41f1102684de205a82` | [Drive](https://drive.google.com/drive/folders/1Lpeifc57l75zdjD3042dctA5V56P1SN5) |
| 2 s 가림 5회 | `s5_lost` | 27.1 s | 270,446 B | `574d8f34c47edc558789e94ff47495a2c9c252b4ddb365e476bb0de7c680d553` | [Drive](https://drive.google.com/drive/folders/1L6bgzE7bti7PMbR_RfrbpfhZRFII_jyL) |
| 토픽 중단 · 제어 통신 중단 | `s6_stop` | 199.8 s | 1,624,171 B | `8edc1efc517f05e569f5dd5b6e8b62fa0cdd3645576eb88df0b291ad02ce5a2f` | [Drive](https://drive.google.com/drive/folders/19rP4yo9OG1V4n8zWHsKnK_JtjE69NsMN) |
| 모의 입력 (0, ±0.3, z=0, 중단) | `s6_mock4` | 21.1 s | 153,867 B | `ac998d3078371d9f44da9b94bec342c38c5b65ea072c9f85d190f5a9198da809` | [Drive](https://drive.google.com/drive/folders/1pVJh0nyYhI0T7LXODmdJTsoAoGQ6DxUJ) |
| 모의 입력 (미검출 → 복귀) | `s6_resume2` | 18.4 s | 129,525 B | `68bee096f973366a9d74332719ec5452b67ad02d14c31b30a29bde1be0436cad` | [Drive](https://drive.google.com/drive/folders/1HTsLxSeu2GHhgu24XWhq_BUa3yKT13A7) |

공통 조건 ([`control_test_2026-10-08.md`](https://drive.google.com/file/d/1ZxXCqlmpjfauHtnMCoKwb8SNI966TuPd/view)): 기준 커밋 `c1d748c`, 설정 `config/control_kp20.yaml` · `control_kp40.yaml` (s5 · s6은 Kp 40, 모의 입력은 Kp 100), 기록 토픽 `/target` · `/tracking_status` · `/motor/command` · `/motor/state`, 영상 토픽 없음 → 결과 재분석만 가능. `s6_stop`은 기록 종료가 비정상이라 `ros2 bag reindex`로 정리한 bag이다.

실패 · 무효 시도(`s5_lost_fail1`, `s6_mock*`, `s6_resume`, `s6_stop_fail1~2` 등)도 같은 Drive 폴더에 남아 있다: [horus_test_upload](https://drive.google.com/drive/folders/16n5mu4kALtGOVyiznzKoU5q1Jsuez-gp). 무효 사유는 [`control_test_2026-10-08.md`](https://drive.google.com/file/d/1ZxXCqlmpjfauHtnMCoKwb8SNI966TuPd/view).

### 입력 재처리 결과 (2026-10-08, 실행자 정조은)

| 내용 | bag | 기간 | 크기 | SHA-256 (`.mcap`) | 접근 위치 |
|---|---|---|---|---|---|
| `recording_02` 재처리 1차 (`/target_replay`) | `replay_recording_02` | 14.8 s | 57,372 B | `6c5fb9a18d0b0f44767fa7fc8657ed93553ee029a60e4eddf0fcbc7fd279f410` | [Drive](https://drive.google.com/drive/folders/1IvGwmF2SBZQX9EvJ8mUHGTRO1eUKDBwl) |
| `recording_02` 재처리 2차 (`/target_replay`) | `replay_mine` | 14.8 s | 57,110 B | `72151988dc0e59216d407bdf3c6ca9770cdcb5a1c4b98cb4a00122a5735ebe04` | [Drive](https://drive.google.com/drive/folders/1_a9NbPHi4_r0FGpNBfAv650gDaKpmmM5) |

## 메타데이터

| 항목 | `success_01` | `fail_01` | `recording_01` | `recording_02` |
|---|---|---|---|---|
| 기록 토픽 (메시지 수) | `/target` 622, `/tracking_status` 23, `/motor/command` 518, `/motor/state` 1091, `camera_info` 683 | `/target` 753, `/tracking_status` 40, `/motor/command` 483, `/motor/state` 1376, `camera_info` 864 | `/target` 413, `/tracking_status` 15, `/motor/command` 294, `/motor/state` 728 | `color/image_raw` 447, `aligned_depth_to_color/image_raw` 446, `camera_info` 446, `/target` 446, `/tracking_status` 22, `/motor/command` 182, `/motor/state` 338 |
| 전체 메시지 수 | 2937 | 3516 | 1450 | 2327 |
| 기간 (s) | 22.73 | 28.80 | 14.55 | 14.86 |
| 영상 해상도 · FPS · encoding | 영상 없음 | 영상 없음 | 영상 없음 | 640×480 · 30 · color `rgb8`, depth `16UC1` (비압축) |
| 사용한 설정 파일 | 기본값 | 기본값 | 기본값 | `config/perception.yaml` (재처리 결과 446/446 일치로 같은 설정임을 확인, Drive [`s7_memo.txt`](https://drive.google.com/file/d/1iGOthntzHd6mILcthx7pY0fZarG1l-cM/view)) |
| 저장 형식 | mcap | mcap | mcap | mcap |

`success_01` · `fail_01` · `recording_01`에는 영상 토픽이 없어 **결과 재분석만** 가능하다. 입력 재처리(검출기 재실행)는 영상이 있는 `recording_02`로 한다.

`metadata.yaml`과 데이터 파일을 **함께** 보관한다. 하나만 있으면 재생되지 않는다.

## 기록 명령

    # 시험 기록 (s3 · s5 · s6). 백그라운드 실행 시 입력을 /dev/null 로 연결해야 멈추지 않는다
    ros2 bag record -o <이름> --topics /target /tracking_status /motor/command /motor/state < /dev/null

## 재생 명령

**실제 모터 출력을 비활성한 상태로** 재생한다 (`control_node` · `center_node` 실행 안 함, OpenCR 미연결).

입력 재처리 — bag 영상만 검출기로 전달, 저장된 `/target`과 섞지 않도록 출력 토픽을 바꾼다 (Drive [`s7_memo.txt`](https://drive.google.com/file/d/1iGOthntzHd6mILcthx7pY0fZarG1l-cM/view), 모든 터미널에 `FASTDDS_DEFAULT_PROFILES_FILE` 지정 후):

    ros2 run perception perception_node --ros-args -r /target:=/target_replay
    ros2 bag record -o replay_recording_02 --topics /target_replay
    ros2 bag play recording_02 --topics /camera/camera/color/image_raw /camera/camera/aligned_depth_to_color/image_raw

2차 재처리에서는 재생 직후 앞 영상 2장이 빠졌다. 노드끼리 연결되기 전에 재생된 영상을 best-effort · depth 1 구독이 받지 못한 것이다.

결과 재분석 — 저장된 `/target` · `/tracking_status`로 지표를 다시 계산한다. bag을 재생하지 않고 `rosbag2_py`로 파일을 읽기만 하므로 모터 명령이 나가지 않는다. 산식은 스크립트 머리말과 `report.md` "P 추적" 절:

    source /opt/ros/lyrical/setup.bash
    python3 results/logs/reanalysis/reanalyze.py <success_01 폴더> <fail_01 폴더> <recording_02 폴더> <recording_01 폴더> > results/logs/reanalysis/reanalysis.csv

`lv2_module5/`에서 실행한다. 출력은 `results/logs/reanalysis/reanalysis.csv`와 같다.

## 영상

| 내용 | 파일 | 길이 | 위치 |
|---|---|---|---|
| `recording_02` 컬러 영상 (bag에서 추출, 실시간 촬영 아님) | `recording_02_color.gif` | 14.9 s (447프레임, 8 fps · 320×240으로 축소) | 레포 [`results/images/recording_02_color.gif`](../results/images/recording_02_color.gif) |

`/camera/camera/color/image_raw` 447장을 `rgb8` 그대로 읽어 만들었다. 다른 bag에는 영상 토픽이 없다.

## 체크섬 확인

    sha256sum <bag 폴더>/*.mcap

## 다운로드 접근 확인

| 확인자 | 날짜 | 대상 | 결과 |
|---|---|---|---|
| 이창엽 | 2026-10-08 | 이 문서의 Drive bag 링크 | 다운로드 성공, `sha256sum` 값이 표와 일치 |
