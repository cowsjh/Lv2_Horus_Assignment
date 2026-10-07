// config.h — 펌웨어 설정값
// 안전 범위는 config/device.yaml 과 반드시 일치시킬 것
#pragma once
#include <stdint.h>

namespace cfg {

constexpr int      DXL_DIR_PIN  = 84;        // OpenCR DYNAMIXEL 방향 제어 핀
constexpr float    DXL_PROTOCOL = 2.0f;
constexpr uint32_t DXL_BAUD     = 1000000;
constexpr uint32_t PC_BAUD      = 115200;

constexpr int     AXES = 2;                  // [0]=yaw(ID 11), [1]=pitch(ID 12)
constexpr uint8_t ID[AXES]       = {11, 12};
constexpr int32_t SAFE_MIN[AXES] = {698, 2230};    // pitch: 브래킷 변경 후 재측정 (2026-10-07)
constexpr int32_t SAFE_MAX[AXES] = {3502, 3033};

// 부팅 시 원점(기본 자세) 이동
//   토크를 현재 위치에서 켠 뒤 목표만 원점으로 바꾼다 → Profile Velocity 속도로 천천히 이동
//   전원을 켜거나 RESET 할 때만 동작 (통신 끊김 · 정지 상태에서는 원점으로 가지 않음)
constexpr bool    HOME_ON_BOOT    = true;
constexpr int32_t HOME_TICK[AXES] = {2046, 2553};  // config/device.yaml 의 center_tick 과 일치시킬 것

constexpr uint32_t PROFILE_VEL       = 20;   // x0.229 rpm = 약 0.48 rad/s (초기 시험용)
constexpr uint32_t CMD_TIMEOUT_MS    = 500;  // 제어 통신 중단 판정
constexpr uint32_t STATUS_PERIOD_MS  = 20;   // 50 Hz
constexpr uint8_t  HWERR_CHECK_EVERY = 10;   // 상태 보고 10번에 한 번 하드웨어 에러 확인

}  // namespace cfg

// S 메시지 flags (비트 합)
enum StatusFlag : uint8_t {
  F_HOLD     = 1,   // 정지 중
  F_TIMEOUT  = 2,   // 명령 타임아웃으로 정지
  F_LIMIT    = 4,   // 마지막 목표를 안전 범위로 잘라서 적용
  F_HW_ERROR = 8,   // 모터 통신 실패 또는 Hardware Error Status
};
