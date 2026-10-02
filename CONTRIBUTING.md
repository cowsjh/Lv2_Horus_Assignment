# 기여 규약

Horus 팀 저장소 작업 규칙.

## 브랜치

`main` → `<기능>/<버전>`

| 예시 | 용도 |
|---|---|
| `perception/01` | 인지 기능의 첫 작업 |
| `control/02` | 제어 기능의 두 번째 작업 |

- 한글 · 공백 · 대문자 금지. 소문자와 `-`, 기능과 버전 사이 `/` 하나만.
- 브랜치는 항상 최신 `main`에서 딴다. PR의 base도 `main`이다.
- `perception` 같은 기능 이름만 있는 브랜치는 만들지 않는다. git은 `perception`과 `perception/01`을 동시에 만들 수 없다.

## 커밋 메시지

| type | 쓸 때 |
|---|---|
| `chore` | 디렉토리 셋업 |
| `feat` | 기능 추가 |
| `fix` | 수정 |

- **token, SSH, password 커밋 금지**