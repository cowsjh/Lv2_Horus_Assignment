// motor.cpp
#include "motor.h"

#include <Arduino.h>
#include <Dynamixel2Arduino.h>

#include "config.h"

using namespace ControlTableItem;

namespace {

Dynamixel2Arduino dxl(Serial3, cfg::DXL_DIR_PIN);   // Serial3 = OpenCR DYNAMIXEL 포트

int32_t goal_[cfg::AXES]    = {0, 0};
int32_t present_[cfg::AXES] = {0, 0};
bool    holding_ = true;    // 부팅 직후는 정지 상태
bool    limited_ = false;
bool    ready_   = false;   // 두 모터 모두 초기화에 성공해야 true. false 면 명령을 무시한다

constexpr int kReadRetries = 3;

// 현재 위치를 최대 kReadRetries 번 읽는다. 통신 오류면 실패.
// require_single_turn = true 이면 값이 0~4095 밖일 때도 실패로 본다
// (읽기 실패 시 돌아오는 0 이나 이상한 값을 목표로 쓰지 않기 위해).
bool readPosition(uint8_t id, int32_t & out, bool require_single_turn) {
  for (int i = 0; i < kReadRetries; i++) {
    int32_t p = dxl.readControlTableItem(PRESENT_POSITION, id);
    bool comm_ok = (dxl.getLastLibErrCode() == 0);
    bool range_ok = !require_single_turn || (p >= 0 && p <= 4095);
    if (comm_ok && range_ok) {
      out = p;
      return true;
    }
    delay(10);
  }
  return false;
}

void reportInitError(const char * what, uint8_t id) {
  Serial.print(F("I,error,"));
  Serial.print(what);
  Serial.print(F(",id="));
  Serial.println(id);
}

void writeGoal(int k, int32_t g) {
  dxl.writeControlTableItem(GOAL_POSITION, cfg::ID[k], g);
  goal_[k] = g;
}

int32_t clampToSafe(int k, long v) {
  return constrain(v, cfg::SAFE_MIN[k], cfg::SAFE_MAX[k]);
}

// EEPROM 값은 다를 때만 쓴다. 토크 off 상태에서 호출해야 한다.
void writeIfDifferent(uint8_t item, uint8_t id, int32_t value) {
  int32_t now = dxl.readControlTableItem(item, id);
  if (dxl.getLastLibErrCode() != 0 || now != value) {
    dxl.writeControlTableItem(item, id, value);
  }
}

bool setupOne(int k) {
  const uint8_t id = cfg::ID[k];
  if (!dxl.ping(id)) {
    reportInitError("no_response", id);
    return false;
  }

  // 1) 토크를 건드리기 전에 위치를 읽을 수 있는지 먼저 확인한다.
  //    실패하면 아무 설정도 바꾸지 않는다 (토크 상태 그대로, 움직이지 않음).
  //    (확장 모드(4)였다면 값이 4095 를 넘을 수 있어 여기서는 통신만 확인한다)
  int32_t p;
  if (!readPosition(id, p, false)) {
    reportInitError("read_failed_before_setup", id);
    return false;
  }

  dxl.torqueOff(id);
  dxl.setOperatingMode(id, OP_POSITION);                       // 일반 위치 모드(3)
  writeIfDifferent(MIN_POSITION_LIMIT, id, cfg::SAFE_MIN[k]);  // 모터 내부 제한
  writeIfDifferent(MAX_POSITION_LIMIT, id, cfg::SAFE_MAX[k]);
  dxl.writeControlTableItem(PROFILE_VELOCITY, id, cfg::PROFILE_VEL);
  dxl.writeControlTableItem(BUS_WATCHDOG, id, 0);              // v1: 사용 안 함

  // 2) 모드 변경 후 위치를 다시 읽는다 (재시도 + 0~4095 확인).
  //    실패하면 토크를 켜지 않는다. 읽기 실패 값(0)이 SAFE_MIN 으로 잘려
  //    토크를 켜는 순간 범위 끝으로 이동하는 것을 막는다.
  if (!readPosition(id, p, true)) {
    reportInitError("read_failed_torque_left_off", id);
    return false;
  }

  // 3) goal = 현재 위치 → 토크 on (켜는 순간 튀지 않음)
  present_[k] = p;
  writeGoal(k, clampToSafe(k, p));
  dxl.torqueOn(id);

  // 4) 부팅 시 원점 이동: 토크를 켠 뒤 목표만 원점으로 바꾼다
  //    (순서가 중요: 원점을 먼저 쓰고 토크를 켜면 켜는 순간 세게 끌려감)
  //    holding_ 은 true 로 남아 있으므로 hold() 가 이 목표를 덮어쓰지 않고, 원점까지 간다
  if (cfg::HOME_ON_BOOT) {
    writeGoal(k, clampToSafe(k, cfg::HOME_TICK[k]));
  }
  return true;
}

}  // namespace

namespace motor {

bool init() {
#ifdef BDPIN_DXL_PWR_EN
  pinMode(BDPIN_DXL_PWR_EN, OUTPUT);       // OpenCR DYNAMIXEL 포트 전원
  digitalWrite(BDPIN_DXL_PWR_EN, HIGH);
#endif
  dxl.begin(cfg::DXL_BAUD);
  dxl.setPortProtocolVersion(cfg::DXL_PROTOCOL);

  bool ok = true;
  for (int k = 0; k < cfg::AXES; k++) {
    ok = setupOne(k) && ok;
  }
  holding_ = true;
  ready_ = ok;     // 한 축이라도 실패하면 명령을 받지 않는다 (반쪽 동작 방지)
  if (ok && cfg::HOME_ON_BOOT) {
    Serial.println("I,homing_on_boot");
  }
  return ok;
}

bool readPresent() {
  bool ok = true;
  for (int k = 0; k < cfg::AXES; k++) {
    int32_t p = dxl.readControlTableItem(PRESENT_POSITION, cfg::ID[k]);
    if (dxl.getLastLibErrCode() != 0) {
      ok = false;
    } else {
      present_[k] = p;
    }
  }
  return ok;
}

void hold() {
  if (!ready_ || holding_) return;
  readPresent();
  for (int k = 0; k < cfg::AXES; k++) {
    writeGoal(k, clampToSafe(k, present_[k]));
  }
  holding_ = true;
  limited_ = false;
}

void applyGoal(long raw_yaw, long raw_pitch) {
  if (!ready_) return;   // 초기화 실패 상태에서는 이동 명령을 무시 (holding 유지, HW_ERROR 보고)
  const long raw[cfg::AXES] = {raw_yaw, raw_pitch};
  limited_ = false;
  for (int k = 0; k < cfg::AXES; k++) {
    int32_t g = clampToSafe(k, raw[k]);
    if (g != raw[k]) limited_ = true;
    if (holding_ || g != goal_[k]) writeGoal(k, g);   // 바뀐 축만 전송
  }
  holding_ = false;
}

bool checkHwError() {
  bool err = false;
  for (int k = 0; k < cfg::AXES; k++) {
    int32_t e = dxl.readControlTableItem(HARDWARE_ERROR_STATUS, cfg::ID[k]);
    if (dxl.getLastLibErrCode() != 0 || e != 0) err = true;
  }
  return err;
}

bool    holding()          { return holding_; }
bool    limited()          { return limited_; }
int32_t present(int axis)  { return present_[axis]; }

}  // namespace motor
