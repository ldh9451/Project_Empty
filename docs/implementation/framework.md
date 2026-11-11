# 기본 프레임워크 구현

- 상태: 구현 완료, Debug 빌드 및 자동 검증 완료
- 관련 기획: [기본 프레임워크](../planning/framework.md)
- 작성일: 2026-11-12

## 구성

| 파일 | 역할 |
|---|---|
| `cpp/src/game_system.h` | `IGameSystem`: Init / Update / Release 순수 가상 인터페이스 |
| `cpp/src/entry_point.h`, `.cpp` | 생명주기와 시스템 실행 순서 관리 |
| `cpp/src/game_input.h`, `.cpp` | 입력 상태, 프레임별 마우스 이동량, 커서 캡처·복원 |
| `cpp/src/game_camera.h`, `.cpp` | 자유 이동 카메라의 위치·회전 갱신 |
| `scenes/main.tscn` | EntryPoint, Input, Camera, 확인용 바닥·큐브·조작 안내 |
| `project.godot` | 기본 씬과 camera_* 입력 액션 |
| `tests/framework_test.gd` | 실제 C++ 확장을 불러오는 통합 테스트 |

## 실행 순서

```text
Godot _ready()     → EntryPoint.Init()    → Input.Init() → Camera.Init()
Godot _process(dt) → EntryPoint.Update()  → Input.Update(dt) → Camera.Update(dt)
Godot _exit_tree() → EntryPoint.Release() → Camera.Release() → Input.Release()
```

`EntryPoint` 아래 `Input`은 `GameInput`, `Camera`는 `GameCamera`여야 한다.
노드의 생성·삭제는 Godot 씬 소유권을 따른다. Release는 제어와 참조를 정리하며 노드를 직접 삭제하지 않는다.
씬에서 빠졌다 다시 들어오면 재초기화한다. 에디터에서는 Init을 실행하지 않는다.

## 입력과 카메라

- 이동: Project Settings의 Input Map에 있는 `camera_*` 액션을 사용한다.
- 우클릭과 시점 회전은 GUI가 처리하지 않은 입력에서 받는다.
- 우클릭 해제와 Esc는 GUI 처리 전에 받아 커서가 갇히지 않게 한다.
- 마우스 `screen_relative`를 한 프레임 동안 누적하고 한 번만 소비한다.
- 마우스 회전량에는 delta를 곱하지 않으며, 이동에는 delta를 곱한다.
- 기본 이동 속도 8, Shift 속도 24, 감도 0.0025 rad/pixel, 상하 회전 제한 ±89도.
- 이동은 카메라 로컬 축 기준이다. 카메라를 기울이면 전진·상승 방향도 함께 기울어진다.
- 창 포커스 상실, 일시정지, Release 시 누적 입력과 커서 캡처를 해제한다.

## 검증

프로젝트 루트에서 실행한다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\cpp\build.ps1
& "$env:USERPROFILE\Downloads\Godot_v4.7.2-stable_win64.exe\Godot_v4.7.2-stable_win64_console.exe" --headless --path . --script tests/framework_test.gd
```

- Debug 빌드 성공.
- 프레임워크 테스트: `FRAMEWORK_TEST: 0 failures`.
- D3D12 실화면 렌더링: 1280×800 바닥·큐브·조작 안내 표시 및 정상 종료 확인.
- 검사 항목: 시작 씬·카메라, 반복 Init/Release, 트리 재진입, 이동 거리·대각선 정규화·가속,
  마우스 누적·단일 소비·상하 회전 제한, 우클릭 해제·Esc·포커스 상실·일시정지.

## 범위와 제한

- 런타임 프레임워크는 C++이며 GDScript는 자동 테스트에만 사용한다.
- 기존 복셀 데모와 사용자가 수정한 복셀 코드는 보존했다.
- 바닥과 큐브는 방향·거리 확인용이며 충돌이 없다.
- Input과 Camera는 EntryPoint와 함께 살아 있는 고정 자식 노드로 사용한다.
- Release 배포 빌드, 게임패드, 실제 게임용 카메라 추적은 이번 검증 범위에 포함하지 않았다.
