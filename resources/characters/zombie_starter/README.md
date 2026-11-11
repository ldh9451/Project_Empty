# Zombie Starter - MoCap Animation Pack

제공된 `FBX_Zombie_27_Starter`를 Godot 4.7.2용으로 정리했습니다. MotusMan v55 모델과 동작 26종이 있으며, 각 동작의 In Place / Root Motion 버전을 합쳐 애니메이션 52개를 제공합니다.

## 사용 방법

1. Godot에서 `scenes/preview.tscn`을 열고 **F6**을 누르면 대기·제자리 걷기·공격을 동시에 확인할 수 있습니다.
2. 게임 씬에는 `scenes/zombie_motusman.tscn`을 드래그합니다. 기본 자동 재생은 `in_place/Zombie_Idle_1_v2`입니다.
3. 캐릭터의 `AnimationPlayer`에서 원하는 클립을 선택합니다. `in_place/`와 `root_motion/`으로 구분하며, `RESET`과 `T_Pose`도 있습니다.

```cpp
auto *player = character->get_node<godot::AnimationPlayer>("AnimationPlayer");
player->play("in_place/Zombie_Walk_F_1_Loop");
player->play("in_place/Zombie_Atk_Loop_1");
```

캐릭터 루트는 Node3D, 뼈대 경로는 `Skeleton3D`입니다. 원본 미터 단위와 +Z 앞 방향을 유지했습니다. 런타임 GDScript는 없으며, 재질의 `.gdshader`는 GPU 셰이더입니다.

## 폴더

| 경로 | 내용 |
|---|---|
| `models/motusman.scn` | 공유 모델. 스킨 메시 1개, 뼈 73개 |
| `textures/` | 원본 2048² Diffuse / Specular JPG 2개 |
| `materials/` | 두 텍스처를 연결한 재질과 Godot 셰이더 |
| `animations/in_place.res` | 제자리 동작 26개 |
| `animations/root_motion.res` | 원본 이동·회전을 포함한 동작 26개 |
| `animations/rest.res` | RESET 및 원본 T 포즈 |
| `animations/clips.json` | 이름·길이·반복 설정·원본 파일·Hips 변위 |
| `scenes/` | 배치용 캐릭터 씬과 미리보기 씬 |
| `source/` | 원본 FBX 53개, Maya 리그, 텍스처, 애니메이션 목록 PDF |
| `source_manifest.json` | 원본 경로·크기·SHA-256 및 검증 요약 |

원본은 `.gdignore`로 Godot 자동 임포트에서 제외했습니다. 실제 씬은 Godot 네이티브 모델·애니메이션을 참조하며 원본 FBX의 문자 인코딩 경고와 텍스처 경로에 의존하지 않습니다. 원본은 탐색기나 VS Code에서 볼 수 있습니다.

## 이동 및 다른 캐릭터에 적용

- `in_place/`는 코드에서 이동을 제어할 때 사용할 버전입니다. 걷기 한 주기의 수평 위치 변화는 검증에서 약 0.00000075m였습니다. 모든 동작이 똑같은 위치 변화 특성을 가진다고 가정하지 말고 해당 클립을 확인하세요.
- `root_motion/`의 이동은 이 팩에서 **`Skeleton3D:Hips`**에 들어 있습니다. `Root` 뼈를 지정하면 걷기 이동을 얻지 못합니다. 걷기 한 주기에는 약 0.895m의 전진이 있습니다.
- 현재는 원본 뼈 이동을 그대로 재생합니다. CharacterBody3D의 충돌 이동이나 AnimationTree의 루트 모션 추출·누적 처리는 구현하지 않았습니다. 이동 클립을 반복하면 끝에서 시작 위치로 돌아갈 수 있습니다.
- 라이브러리는 **MotusMan의 뼈 이름·계층에 맞춘 것**입니다. 앞서 가져온 SciFi Beast06 Bull 등 다른 리그에 적용하려면 별도 리타게팅이 필요합니다.

## 변환 및 검증

- 내장 ufbx로 FBX를 읽고, 제작용 카메라와 중복 모델을 제거해 공유 모델·독립 애니메이션 라이브러리로 저장했습니다.
- 각 애니메이션의 219개 위치·회전·크기 트랙을 보존했습니다. 모델과 모든 동작의 뼈 계층 및 스킨 바인드 행렬이 일치하는지 확인했습니다.
- 원본 파일의 `Loop` 명칭 및 두 Idle 클립에 반복을 설정했습니다. 루프 설정으로 원본 트랙 보간이 바뀌지 않도록 원래 반복 여부에 맞춰 보간 경계 처리를 유지했습니다.
- 원본 Phong의 Specular 맵은 금속성/거칠기 맵과 의미가 다릅니다. 별도 셰이더에서 밝기를 반사 강도로 사용하고, Phong 지수 10은 GGX 거칠기 0.64로 근사했습니다. 색이 있는 Specular의 색조와 Phong 조명 모델을 완전히 재현하는 변환은 아닙니다.
- **52개 클립 × 4개 시점 × 73개 뼈 = 15,184개 자세**를 Godot로 임포트한 원본 FBX와 비교해 일치를 확인했습니다.
- `tests/zombie_starter_test.gd`에서 리소스·재질·바인딩·모든 클립의 트랙·유효한 자세·실제 발 움직임·두 걷기 버전의 변위를 검증합니다.
- D3D12 Forward+ 렌더링에서 대기·걷기·공격·기어가기·사망·빠른 추격 자세를 확인했습니다.

검증 실행은 프로젝트 루트에서 `Godot실행파일 --headless --path . --script tests/zombie_starter_test.gd`입니다. 화면 캡처를 포함하려면 `--headless`를 빼고 끝에 `-- --capture`를 붙입니다.
