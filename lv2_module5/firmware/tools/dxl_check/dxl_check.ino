// dxl_check.ino
// OpenCR + DYNAMIXEL(XM430) 확인용 스케치 (Protocol 2.0)
//
// 목적: 실제 장비의 ID · baud · 설정을 읽고, 토크를 끈 상태에서
//       손으로 돌리며 중심 · 범위 · 방향을 측정해 device.yaml에 기록한다.
// 안전: 이 스케치는 모터를 스스로 움직이는 명령(Goal 이동)을 보내지 않는다.
//
// 시리얼 모니터 115200 baud에서 한 글자 명령:
//   h : 도움말
//   s : ID 스캔 (여러 baud)
//   1 : 통신 baud 1,000,000 으로 설정
//   5 : 통신 baud 57,600 으로 설정
//   i : pan · tilt 설정값 읽기
//   m : 현재 위치 모니터 켜기/끄기 (10 Hz, 최솟값 · 최댓값 기록)
//   r : 최솟값 · 최댓값 초기화
//   o : 토크 OFF (손으로 돌릴 수 있음. tilt는 카메라가 떨어질 수 있으니 손으로 받칠 것)
//   n : 토크 ON  (먼저 goal = 현재 위치로 맞춘 뒤 켜서 튀지 않게 함)

#include <Dynamixel2Arduino.h>

#define DXL_SERIAL   Serial3   // OpenCR의 DYNAMIXEL 포트
#define DEBUG_SERIAL Serial    // USB 시리얼 (PC/Pi와 연결)

const int   DXL_DIR_PIN  = 84;     // OpenCR 방향 제어 핀
const float DXL_PROTOCOL = 2.0;

// 예상 ID. 스캔 결과가 다르면 여기를 고친다.
const uint8_t PAN_ID  = 11;
const uint8_t TILT_ID = 12;
const uint8_t IDS[2]  = {PAN_ID, TILT_ID};
const char*   NAMES[2] = {"pan", "tilt"};

const uint32_t SCAN_BAUDS[] = {57600, 1000000, 115200, 2000000};
uint32_t current_baud = 1000000;

Dynamixel2Arduino dxl(DXL_SERIAL, DXL_DIR_PIN);
using namespace ControlTableItem;

bool monitoring = false;
int32_t min_pos[2];
int32_t max_pos[2];
unsigned long last_print_ms = 0;

void printHelp() {
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.println(F("=== dxl_check ==="));
  DEBUG_SERIAL.println(F("h:help  s:scan  1:baud 1M  5:baud 57600  i:info"));
  DEBUG_SERIAL.println(F("m:monitor on/off  r:reset min/max  o:torque OFF  n:torque ON"));
  DEBUG_SERIAL.print(F("current baud = "));
  DEBUG_SERIAL.println(current_baud);
}

void setBaud(uint32_t baud) {
  current_baud = baud;
  dxl.begin(current_baud);
  dxl.setPortProtocolVersion(DXL_PROTOCOL);
  DEBUG_SERIAL.print(F("baud set to "));
  DEBUG_SERIAL.println(current_baud);
}

void resetMinMax() {
  for (int k = 0; k < 2; k++) {
    min_pos[k] = 2147483647;
    max_pos[k] = -2147483647;
  }
  DEBUG_SERIAL.println(F("min/max reset"));
}

void scan() {
  DEBUG_SERIAL.println(F("scanning... (10~20 s)"));
  bool found_any = false;
  for (uint32_t b : SCAN_BAUDS) {
    dxl.begin(b);
    dxl.setPortProtocolVersion(DXL_PROTOCOL);
    for (int id = 0; id <= 252; id++) {
      if (dxl.ping(id)) {
        found_any = true;
        DEBUG_SERIAL.print(F("  found  baud="));
        DEBUG_SERIAL.print(b);
        DEBUG_SERIAL.print(F("  id="));
        DEBUG_SERIAL.print(id);
        DEBUG_SERIAL.print(F("  model="));
        DEBUG_SERIAL.println(dxl.getModelNumber(id));
      }
    }
  }
  if (!found_any) {
    DEBUG_SERIAL.println(F("  nothing found: check motor power (12V), cable, connector"));
  }
  setBaud(current_baud);  // 원래 baud로 복귀
}

void printItem(const __FlashStringHelper* label, int32_t value) {
  DEBUG_SERIAL.print(F("    "));
  DEBUG_SERIAL.print(label);
  DEBUG_SERIAL.println(value);
}

void info() {
  for (int k = 0; k < 2; k++) {
    uint8_t id = IDS[k];
    DEBUG_SERIAL.print(F("[")); DEBUG_SERIAL.print(NAMES[k]);
    DEBUG_SERIAL.print(F("] id=")); DEBUG_SERIAL.println(id);
    if (!dxl.ping(id)) {
      DEBUG_SERIAL.println(F("    no response at current baud"));
      continue;
    }
    printItem(F("model number      : "), dxl.getModelNumber(id));
    printItem(F("operating mode    : "), dxl.readControlTableItem(OPERATING_MODE, id));
    printItem(F("drive mode        : "), dxl.readControlTableItem(DRIVE_MODE, id));
    printItem(F("homing offset     : "), dxl.readControlTableItem(HOMING_OFFSET, id));
    printItem(F("min position limit: "), dxl.readControlTableItem(MIN_POSITION_LIMIT, id));
    printItem(F("max position limit: "), dxl.readControlTableItem(MAX_POSITION_LIMIT, id));
    printItem(F("velocity limit    : "), dxl.readControlTableItem(VELOCITY_LIMIT, id));
    printItem(F("profile velocity  : "), dxl.readControlTableItem(PROFILE_VELOCITY, id));
    printItem(F("bus watchdog      : "), dxl.readControlTableItem(BUS_WATCHDOG, id));
    printItem(F("torque enable     : "), dxl.readControlTableItem(TORQUE_ENABLE, id));
    printItem(F("present position  : "), dxl.readControlTableItem(PRESENT_POSITION, id));
    printItem(F("temperature [C]   : "), dxl.readControlTableItem(PRESENT_TEMPERATURE, id));
    printItem(F("voltage [0.1V]    : "), dxl.readControlTableItem(PRESENT_INPUT_VOLTAGE, id));
  }
}

void torqueOff() {
  for (int k = 0; k < 2; k++) dxl.torqueOff(IDS[k]);
  DEBUG_SERIAL.println(F("torque OFF (support the camera by hand)"));
}

void torqueOn() {
  for (int k = 0; k < 2; k++) {
    uint8_t id = IDS[k];
    int32_t now = dxl.readControlTableItem(PRESENT_POSITION, id);
    dxl.writeControlTableItem(GOAL_POSITION, id, now);  // 현재 위치를 목표로 → 켜도 안 튐
    dxl.torqueOn(id);
  }
  DEBUG_SERIAL.println(F("torque ON (holding present position)"));
}

void monitorTick() {
  if (millis() - last_print_ms < 100) return;
  last_print_ms = millis();
  for (int k = 0; k < 2; k++) {
    int32_t p = dxl.readControlTableItem(PRESENT_POSITION, IDS[k]);
    if (dxl.getLastLibErrCode() != 0) {
      DEBUG_SERIAL.print(NAMES[k]);
      DEBUG_SERIAL.print(F("=ERR   "));
      continue;
    }
    if (p < min_pos[k]) min_pos[k] = p;
    if (p > max_pos[k]) max_pos[k] = p;
    DEBUG_SERIAL.print(NAMES[k]);
    DEBUG_SERIAL.print(F("="));
    DEBUG_SERIAL.print(p);
    DEBUG_SERIAL.print(F(" ("));
    DEBUG_SERIAL.print(p * 360.0f / 4096.0f, 1);
    DEBUG_SERIAL.print(F(" deg) min="));
    DEBUG_SERIAL.print(min_pos[k]);
    DEBUG_SERIAL.print(F(" max="));
    DEBUG_SERIAL.print(max_pos[k]);
    DEBUG_SERIAL.print(F("   "));
  }
  DEBUG_SERIAL.println();
}

void setup() {
  DEBUG_SERIAL.begin(115200);
  while (!DEBUG_SERIAL);  // 시리얼 모니터가 열릴 때까지 대기

#ifdef BDPIN_DXL_PWR_EN
  pinMode(BDPIN_DXL_PWR_EN, OUTPUT);   // OpenCR DYNAMIXEL 포트 전원 켜기
  digitalWrite(BDPIN_DXL_PWR_EN, HIGH);
#endif

  setBaud(current_baud);
  resetMinMax();
  printHelp();
}

void loop() {
  if (DEBUG_SERIAL.available()) {
    char c = DEBUG_SERIAL.read();
    switch (c) {
      case 'h': printHelp(); break;
      case 's': monitoring = false; scan(); break;
      case '1': setBaud(1000000); break;
      case '5': setBaud(57600); break;
      case 'i': monitoring = false; info(); break;
      case 'm':
        monitoring = !monitoring;
        DEBUG_SERIAL.println(monitoring ? F("monitor ON") : F("monitor OFF"));
        break;
      case 'r': resetMinMax(); break;
      case 'o': torqueOff(); break;
      case 'n': torqueOn(); break;
      default: break;  // 줄바꿈 등은 무시
    }
  }
  if (monitoring) monitorTick();
}
