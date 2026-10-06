// protocol.h — 상위(라즈베리파이)와의 USB 시리얼 프로토콜
//
//   상위 → 보드
//     G,<yaw_tick>,<pitch_tick>   목표 위치로 이동 (0~4095 정수)
//     H                          정지
//   보드 → 상위
//     S,<yaw_tick>,<pitch_tick>,<flags>   상태 보고 (실측 위치)
//     I,...                              부팅 · 정보 메시지
//
// 줄 끝은 '\r' 또는 '\n'. 파싱에 성공한 G · H만 타임아웃 타이머를 리셋한다.
#pragma once
#include <stdint.h>

namespace protocol {

void begin();                   // 시리얼 시작, 부팅 메시지 출력
void poll();                    // 들어온 문자를 읽어 한 줄이 완성되면 처리
bool timedOut();                // 올바른 명령이 CMD_TIMEOUT_MS 동안 없었는가
void reportStatus(bool timed_out, bool init_ok);   // S 한 줄 출력

}  // namespace protocol
