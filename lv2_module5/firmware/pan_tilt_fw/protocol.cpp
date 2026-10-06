// protocol.cpp
#include "protocol.h"

#include <Arduino.h>
#include <stdlib.h>

#include "config.h"
#include "motor.h"

namespace {

char     line_buf[40];
uint8_t  line_len      = 0;
bool     line_overflow = false;
uint32_t last_cmd_ms   = 0;
uint8_t  status_count  = 0;
bool     hw_error      = false;

bool parseLong(const char*& p, long& out) {
  char* end;
  out = strtol(p, &end, 10);
  if (end == p) return false;
  p = end;
  return true;
}

void handleLine(const char* s) {
  if (s[0] == 'H' && s[1] == '\0') {
    motor::hold();
    last_cmd_ms = millis();
    return;
  }
  if (s[0] == 'G' && s[1] == ',') {
    const char* p = s + 2;
    long a, b;
    if (!parseLong(p, a) || *p != ',') return;
    p++;
    if (!parseLong(p, b) || *p != '\0') return;
    if (a < 0 || a > 4095 || b < 0 || b > 4095) return;
    motor::applyGoal(a, b);
    last_cmd_ms = millis();
    return;
  }
  // 그 밖의 줄은 무시하고, 타이머도 리셋하지 않는다
}

}  // namespace

namespace protocol {

void begin() {
  Serial.begin(cfg::PC_BAUD);   // 상위 연결을 기다리지 않는다 (단독으로도 안전하게 동작)
  // 명령이 오기 전까지는 타임아웃 정지 상태로 시작
  last_cmd_ms = millis() - cfg::CMD_TIMEOUT_MS - 1;
}

void poll() {
  while (Serial.available()) {
    char c = Serial.read();
    // '\r' 와 '\n' 모두 줄 끝으로 본다.
    // (\r 로만 끝나는 잡음, 예: ModemManager 의 "AT\r" 가 다음 명령과 붙어 함께 버려지는 것을 막음)
    if (c == '\r' || c == '\n') {
      if (!line_overflow && line_len > 0) {   // 빈 줄("\r\n" 의 두 번째 끝)은 무시
        line_buf[line_len] = '\0';
        handleLine(line_buf);
      }
      line_len = 0;
      line_overflow = false;
    } else if (line_len < sizeof(line_buf) - 1) {
      line_buf[line_len++] = c;
    } else {
      line_overflow = true;   // 너무 긴 줄은 통째로 버림
    }
  }
}

bool timedOut() {
  return (millis() - last_cmd_ms) > cfg::CMD_TIMEOUT_MS;
}

void reportStatus(bool timed_out, bool init_ok) {
  bool read_ok = motor::readPresent();

  if (++status_count >= cfg::HWERR_CHECK_EVERY) {
    status_count = 0;
    hw_error = motor::checkHwError();
  }

  uint8_t f = 0;
  if (motor::holding())                     f |= F_HOLD;
  if (timed_out)                            f |= F_TIMEOUT;
  if (motor::limited() && !motor::holding()) f |= F_LIMIT;
  if (hw_error || !read_ok || !init_ok)     f |= F_HW_ERROR;

  Serial.print(F("S,"));
  Serial.print(motor::present(0)); Serial.print(',');
  Serial.print(motor::present(1)); Serial.print(',');
  Serial.println(f);
}

}  // namespace protocol
