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
    Serial.print(F("I,error,no_response,id="));
    Serial.println(id);
    return false;
  }
  dxl.torqueOff(id);
  dxl.setOperatingMode(id, OP_POSITION);                       // 일반 위치 모드(3)
  writeIfDifferent(MIN_POSITION_LIMIT, id, cfg::SAFE_MIN[k]);  // 모터 내부 제한
  writeIfDifferent(MAX_POSITION_LIMIT, id, cfg::SAFE_MAX[k]);
  dxl.writeControlTableItem(PROFILE_VELOCITY, id, cfg::PROFILE_VEL);
  dxl.writeControlTableItem(BUS_WATCHDOG, id, 0);              // v1: 사용 안 함

  // 토크를 켜기 전에 goal = 현재 위치 → 켜는 순간 튀지 않음
  present_[k] = dxl.readControlTableItem(PRESENT_POSITION, id);
  writeGoal(k, clampToSafe(k, present_[k]));
  dxl.torqueOn(id);
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
  if (holding_) return;
  readPresent();
  for (int k = 0; k < cfg::AXES; k++) {
    writeGoal(k, clampToSafe(k, present_[k]));
  }
  holding_ = true;
  limited_ = false;
}

void applyGoal(long raw_yaw, long raw_pitch) {
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
