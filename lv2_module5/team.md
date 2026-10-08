4인 협업 증거. 커밋 수나 코드 줄 수가 아니라 **실제 기여와 판단 근거**를 확인하는 문서.

## 역할과 기여

| 이름 / GitHub ID | 역할 | 담당 Issue | 병합된 본인 PR | 다른 PR 리뷰 |
|---|---|---|---|---|
| 송정혁 / [@cowsjh](https://github.com/cowsjh) | 팀장 · 테크리드 + 검증 · 문서화 | 작성 [#22][is22] (dt 추가 요청 → @developlcy-oss 배정, [#24][pr24]로 해결) | [#1][pr1] CONTRIBUTING 브랜치 규약 (승인 [@developlcy-oss][rv1]), [#3][pr3] README 패키지 목록 (승인 [@developlcy-oss][rv3]), [#4][pr4] README 인터페이스 · OpenCR 빌드 · 업로드 가이드 (승인 [@developlcy-oss][rv4]), [#28][pr28] README 갱신 (승인 [@jejeong5976][rv28-jej]) | 승인 17건: [#2][c2] 패키지 빌드 확인, [#5][c5] 빌드 확인, [#6][c6] 공용 환경 컴파일 · 업로드 확인, [#7][c7] 구조 확인 · launch 통합 요청, [#8][c8] depth 필드 작동 확인, [#9][c9] 펌웨어 동작 확인, [#10][c10] 모터 · 타겟 상태별 홀드 확인, [#12][c12], [#13][c13] Kp 파라미터 작동 확인, [#14][c14] 빌드 확인, [#20][c20], [#23][c23] home 원점 확인, [#24][c24] dt 수식 확인, [#25][c25] 타임아웃 reason 확인, [#26][c26] 수식 확인, [#27][c27] 파일 삭제 · README 확인, [#29][c29] 수식 확인. 절차: [#18][c18] · [#19][c19] Issue 브랜치 재작업 요청 |
| 한지훈 / [@polarnight1212](https://github.com/polarnight1212) | 제어 | 담당 [#16][is16] · 작성 [#17][is17] | [#6][pr6] 장치 설정 · 다이나믹셀 점검 스케치, [#9][pr9] pan-tilt 펌웨어(타임아웃 홀드 · 제한), [#23][pr23] 부팅 시 home 복귀 · pitch 제한, [#27][pr27] device.yaml 삭제 · 측정 기준 README 이동 | [#8 HSV 범위 변경(S 120→190, V 255→230)에 맞게 주석 · 근거 이미지 갱신 요청][p8], [#10][p10] motor state · status 확인 (승인), [#28][p28] 내용 확인. 받은 리뷰 반영: [#9][p9] fail-safe 수정 · 동작 확인 |
| 정조은 / [@jejeong5976](https://github.com/jejeong5976) | 인지 | 담당 [#15][is15] · 작성 [#16][is16] | [#5][pr5] 인지 C++ 패키지, [#8][pr8] depth · fill ratio 필터, [#11][pr11] center 노드 추적 상태, [#12][pr12] 카메라 구독 queue depth 1, [#14][pr14] bringup 패키지, [#20][pr20] 미사용 control.yaml 삭제, [#25][pr25] 타임아웃 로그 | [#9 `motor.cpp:52-54` 부팅 시 위치 읽기 실패 fail-safe 지적][j9] (반영됨), [#21][j21] tick 단위 반영 확인 (승인), [#23][j23] 변경 파일 확인 (승인), [#28][rv28-jej] 추가 수정 확인 (승인). 받은 리뷰 반영: [#8][j8] |
| 이창엽 / [@developlcy-oss](https://github.com/developlcy-oss) | 통합 | 담당 [#17][is17], [#22][is22] · 작성 [#15][is15] | [#2][pr2] ROS2 초기 구성, [#7][pr7] center · control 노드 · 설정, [#10][pr10] 파라미터 값 수정, [#13][pr13] Kp · launch, [#21][pr21] center.yaml 수치 수정, [#24][pr24] 제어식 dt 추가, [#26][pr26] P 제어 계산 수정, [#29][pr29] P 계산 수치 · yaml 정리 | [#1][rv1] (승인), [#3][rv3c] · [#3][rv3] (코멘트 · 승인), [#4][rv4] (승인), [#9][d9] 펌웨어 파일 구성 확인 (승인, 이후 새 커밋으로 무효), [#11][d11] 기존 파일 충돌 없음 확인 (승인), [#28][d28] (승인, 이후 새 커밋으로 무효) |

[is15]: https://github.com/cowsjh/Lv2_Horus_Assignment/issues/15
[is16]: https://github.com/cowsjh/Lv2_Horus_Assignment/issues/16
[is17]: https://github.com/cowsjh/Lv2_Horus_Assignment/issues/17
[is22]: https://github.com/cowsjh/Lv2_Horus_Assignment/issues/22
[pr1]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/1
[pr2]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/2
[pr3]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/3
[pr4]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/4
[pr5]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/5
[pr6]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/6
[pr7]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/7
[pr8]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/8
[pr9]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/9
[pr10]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/10
[pr11]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/11
[pr12]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/12
[pr13]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/13
[pr14]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/14
[pr20]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/20
[pr21]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/21
[pr23]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/23
[pr24]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/24
[pr25]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/25
[pr26]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/26
[pr27]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/27
[pr28]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/28
[pr29]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/29
[rv1]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/1#pullrequestreview-5389451738
[rv3c]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/3#pullrequestreview-5411641113
[rv3]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/3#pullrequestreview-5411665340
[rv4]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/4#pullrequestreview-5422569117
[rv28-jej]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/28#pullrequestreview-5450189857
[c2]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/2#pullrequestreview-5404765743
[c5]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/5#pullrequestreview-5423015052
[c6]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/6#pullrequestreview-5423680518
[c7]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/7#pullrequestreview-5424830419
[c8]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/8#pullrequestreview-5425227564
[c9]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/9#pullrequestreview-5425195534
[c10]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/10#pullrequestreview-5426799336
[c12]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/12#pullrequestreview-5427493371
[c13]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/13#pullrequestreview-5427993720
[c14]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/14#pullrequestreview-5436514619
[c20]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/20#pullrequestreview-5437045793
[c23]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/23#pullrequestreview-5437266572
[c24]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/24#pullrequestreview-5437430390
[c25]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/25#pullrequestreview-5437867131
[c26]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/26#pullrequestreview-5438249658
[c27]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/27#pullrequestreview-5450062382
[c29]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/29#pullrequestreview-5450900897
[c18]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/18#issuecomment-6029650253
[c19]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/19#issuecomment-6029656455
[p8]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/8#issuecomment-6011170270
[p9]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/9#issuecomment-6011587554
[p10]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/10#pullrequestreview-5426837396
[p28]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/28#issuecomment-6049908710
[j8]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/8#issuecomment-6011555309
[j9]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/9#issuecomment-6011168276
[j21]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/21#pullrequestreview-5437035609
[j23]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/23#pullrequestreview-5437247748
[d9]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/9#pullrequestreview-5425000754
[d11]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/11#pullrequestreview-5427266444
[d28]: https://github.com/cowsjh/Lv2_Horus_Assignment/pull/28#pullrequestreview-5450075669

**전원 필수**: 본인 PR 1건 이상 병합 + 타인 PR에 구체적인 리뷰 1건 이상. 팀장도 포함.
팀장 PR에 대한 타인의 승인도 링크로 연결한다.

## 역할별 책임

| 기술 역할 | 주요 책임 |
|---|---|
| 인지 | 카메라 입력, HSV · Contour, 미검출 처리, 검출 증거 |
| 제어 | 모터 연결, 오차→명령, 속도 · 위치 제한, 정지 |
| 통합 | ROS 2 인터페이스, 실행 구성, bag 기록 · 재생 |
| 검증 | 시험 조건, 측정 · 그래프, 결과 해석 · 발표 |

역할은 책임 구분이며 다른 영역을 몰라도 된다는 뜻이 아니다.

## 저장소 권한 · 보호 설정

| 담당 | 권한 |
|---|---|
| 팀장 | Admin · 설정 관리 · 최종 병합 · PR 승인 |
| 팀원 | Write · 작업 브랜치 push · PR 작성 · 리뷰 · PR 승인 |

### main 브랜치 보호

- main에 병합하기 전에 반드시 PR을 거친다.
- 작성자가 아닌 사람의 승인을 1개 이상 받아야 한다.
- 새 커밋을 push하면 이전 승인을 무효로 하고 다시 검토받는다.
- 병합 전에 리뷰 대화를 모두 해결해야 한다.
- main 갱신 권한을 팀장으로 제한한다. 작업 브랜치는 제한에서 제외한다.
- 위 규칙을 팀장도 우회할 수 없게 한다.
- force push를 금지한다.
- main 삭제를 금지한다.

적용 상태: 저장소는 팀장 개인 계정(`cowsjh`) 소유이며 공개(public)로 전환했다(팀 합의). Settings → Branches의 classic branch protection을 `main`에 적용했다.

| 항목 | 적용 |
|---|---|
| PR 필수 · 승인 1개 이상 · 새 커밋 시 승인 무효화 | 적용 |
| 리뷰 대화 해결 필수 | 적용 |
| 팀장도 우회 불가 | 적용 |
| force push · main 삭제 금지 | 적용 |
| main 갱신 권한을 팀장으로 제한 | **적용 불가** — 개인 계정 저장소에는 이 옵션(조직 전용)이 없다. 기술적으로 차단되지 않는다. |

팀원은 Write 권한이라 승인된 PR의 Merge 버튼을 누를 수 있다. 아래 운영 규칙으로 보완하며, 기술적으로 막은 것이 아니다.

운영 규칙 (팀장만 병합):

- main에 직접 push하지 않고 모든 변경은 PR로 반영한다 (디렉터리 셋업 단계 커밋은 예외).
- 승인(Approve)은 팀 전원(팀장·팀원 3명)이 할 수 있다. 작성자가 아닌 팀원 1명 이상이 Approve한 뒤 팀장이 병합한다. 팀장 본인 PR도 같다.
- 승인 후 코드가 바뀌면 다시 리뷰를 받는다.


### 보호 규칙 검증 기록

작은 문서 PR로 "승인 전 병합 불가 → 타인 승인 → 팀장 병합"을 확인한 기록.

| 항목 | 링크 / 결과 |
|---|---|
| 검증용 PR | [#1](https://github.com/cowsjh/Lv2_Horus_Assignment/pull/1) CONTRIBUTING.md 브랜치 규약 수정 (팀장 본인 PR) |
| 승인 전 병합 차단 확인 | Owner(팀장) 계정에서도 "Review required · Merging is blocked", Merge 버튼 비활성 — [캡처](results/images/pr1-merge-blocked.png) |
| 타인 승인 | [@developlcy-oss 승인](https://github.com/cowsjh/Lv2_Horus_Assignment/pull/1#pullrequestreview-5389451738) (2026-10-02 07:53 UTC) |
| 팀장 병합 | @cowsjh 병합 (2026-10-02 07:54 UTC) |

새 커밋 시 승인 무효화: [#28](https://github.com/cowsjh/Lv2_Horus_Assignment/pull/28)에서 @developlcy-oss 승인 후 새 커밋 `90235e5` push 시 승인이 자동 무효화(dismissed)되고, [@jejeong5976 재승인](https://github.com/cowsjh/Lv2_Horus_Assignment/pull/28#pullrequestreview-5450189857) 후 병합했다.

## 팀장 부재 시 대행

| 대행자 | 기간 | 위임한 권한 |
|---|---|---|
| 해당 없음 | 해당 없음 | 해당 없음 |

과제 기간 중 팀장 부재가 없어 대행자를 지정하지 않았다.

계정 · 비밀번호 · 토큰을 공유하여 대신 작업하지 않는다.

## 최종 통합 확인 (팀장)

| 항목 | 확인자 | 날짜 | 결과 |
|---|---|---|---|
| 4인 PR · 리뷰 · 기여 기록 확인 | 송정혁 | 2026-10-08 | 완료 (위 역할 표 · PR · 리뷰 링크) |
| 최종 main에서 실행 · 정지 · 재현 확인 (팀원 1명 동석) | 송정혁 | 2026-10-08 | 확인자 (이창엽)|
| `lv2-module5-submit` 태그 생성 · push | 송정혁 | 2026-10-08 | 완료 |
| 제출 채널 제출 (팀장 1명, 1회) | 송정혁 | 2026-10-08 | 완료 |
