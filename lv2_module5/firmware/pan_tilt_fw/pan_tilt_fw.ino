// pan_tilt_fw.ino  (v1) — 시작 파일
// arduino-cli 규칙상 폴더 이름과 같은 .ino가 필요하다.
// 실제 코드는 motor.cpp, protocol.cpp에 있다. 설정값은 config.h.

#include <Arduino.h>

#include "config.h"
#include "motor.h"
#include "protocol.h"

static bool     init_ok        = false;
static uint32_t last_status_ms = 0;

void setup() {
  protocol::begin();
  init_ok = motor::init();
  Serial.println(F("I,pan_tilt_fw,v1,ready"));
}

void loop() {
  protocol::poll();

  const bool timed_out = protocol::timedOut();
  if (timed_out) motor::hold();     // 제어 통신 중단 → 정지

  const uint32_t now = millis();
  if (now - last_status_ms >= cfg::STATUS_PERIOD_MS) {
    last_status_ms = now;
    protocol::reportStatus(timed_out, init_ok);
  }
}
