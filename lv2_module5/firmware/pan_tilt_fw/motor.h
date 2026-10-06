// motor.h — DYNAMIXEL 모터 제어 (초기화, 이동, 정지, 상태 읽기)
#pragma once
#include <stdint.h>

namespace motor {

// 부팅 시 한 번: 위치 모드(3), 모터 내부 제한, 속도 제한 설정 후
// goal = 현재 위치로 맞추고 토크 on. 두 모터 모두 성공하면 true.
bool init();

// 정지: 정지 순간의 현재 위치를 목표로 한 번만 고정한다.
// 이미 정지 중이면 아무것도 하지 않는다 (pitch 처짐 누적 방지).
void hold();

// 목표 위치 적용. 안전 범위 밖이면 잘라서 적용하고 limited() = true.
void applyGoal(long raw_yaw, long raw_pitch);

// 현재 위치 읽기. 하나라도 실패하면 false (실패한 축은 이전 값 유지).
bool readPresent();

// Hardware Error Status 확인. 이상이 있거나 읽기 실패면 true.
bool checkHwError();

bool    holding();
bool    limited();
int32_t present(int axis);

}  // namespace motor
