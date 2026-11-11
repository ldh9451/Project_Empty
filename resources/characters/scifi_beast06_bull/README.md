# SciFi Beast06 Bull

Fab의 `scifi_beast06.unitypackage`에서 Godot 4.7.2용으로 변환한 캐릭터입니다.

## 바로 사용하기

1. Godot에서 `scenes/preview.tscn`을 열고 **F6**을 누르면 스킨 3종을 확인할 수 있습니다.
2. 게임 씬에는 `scenes/beast06_bull_skin1.tscn`, `scenes/beast06_bull_skin2.tscn`, `scenes/beast06_bull_skin3.tscn` 중 원하는 씬을 드래그합니다.
3. 각 캐릭터의 `AnimationPlayer`에서 애니메이션을 선택합니다. 기본 자동 재생은 `Idle1`이며 Inspector의 Autoplay에서 해제할 수 있습니다.

```cpp
auto *player = character->get_node<godot::AnimationPlayer>("AnimationPlayer");
player->play("WalkF");
```

캐릭터 루트는 `Node3D`, 뼈대는 `Skeleton3D`, 앞 방향은 원본과 같은 **+Z**입니다. 원본 미터 단위를 유지했습니다. 이동·전투·충돌·AI는 게임 시스템에서 붙이면 됩니다. 런타임 GDScript는 사용하지 않습니다.

## 폴더

| 경로 | 내용 |
|---|---|
| `models/beast06_bull.scn` | 3종이 공유하는 Godot 모델. 71개 뼈, 스킨 메시 15개 |
| `materials/skin1~3/` | 스킨별 Body / Armor StandardMaterial3D |
| `materials/shared/glass.tres` | FBX에서 가져온 공통 유리 재질 |
| `textures/skin1~3/` | BaseColor, Normal, Emissive, MetalGloss, Roughness PNG |
| `textures/shared/` | 모든 스킨이 공유하는 Body / Armor AO |
| `animations/beast06_animations.res` | 29개 클립 + 바인드 자세 복원용 RESET |
| `animations/clips.json` | 클립 이름·길이·반복 여부 |
| `scenes/` | 스킨별 배치용 씬 3개, 미리보기 씬 |
| `source/models/` | 동일한 원본 모델 FBX 3개를 하나로 중복 제거하여 보관 |
| `source/animations/` | 원본 애니메이션 FBX 29개 |
| `source/unity/` | Unity 재질·Prefab·Animator Controller·데모 씬 참고용 원본 |
| `source_manifest.json` | 원본 Unity GUID·경로·SHA-256·변환 리소스 대응표 |

`source/`는 `.gdignore`로 Godot 임포트에서 제외했습니다. 실제 게임은 위의 Godot 모델·재질·애니메이션을 사용하므로 원본 FBX의 옛 TGA 경로에 의존하지 않습니다. Godot 파일시스템에서는 보이지 않아도 VS Code나 탐색기에서 원본을 볼 수 있습니다.

## 애니메이션

- 반복: `Idle1`, `Idle2`, `WalkF`, `WalkF2`, `WalkB`, `WalkL`, `WalkR`, `Run`, `JumpLoop`.
- 단발: `Attack1`~`Attack6`, `Death1`, `Death2`, `GetHitB`, `GetHitB2`, `GetHitF`, `GetHitF2`, `GetHitL`, `GetHitL2`, `GetHitR`, `GetHitR2`, `JumpStart`, `JumpEnd`, `Rage`, `Turn180`.

반복 여부는 이름과 용도에 맞춰 설정했습니다. 이동 및 회전 트랙은 원본을 유지했으며, 루트 모션 추출이나 게임 이동 처리는 구현하지 않았습니다. Unity Animator Controller의 상태 전이는 Godot AnimationTree로 자동 변환하지 않았습니다.

## 재질 변환 및 검증

- 원본 26개 TGA를 해상도 변경 없이 PNG로 변환했습니다. 색상·노멀은 4096², 나머지는 2048²입니다.
- MetalGloss의 R을 금속성으로, `1 - A`를 별도 거칠기 맵 6개로 사용했습니다. 원본 GlossMapScale은 모두 1입니다.
- 원본 Albedo 색상 0.8, Normal 강도 1, AO 강도 1, Emission 색상 흰색을 적용했습니다. Emission은 색상과 텍스처를 곱합니다.
- 노멀은 원본 방향을 유지하고, 텍스처는 3D용 mipmap 및 VRAM 압축으로 임포트합니다. 디스크의 PNG 원본은 무손실입니다.
- 제작용 IK 보조 노드 트랙을 제외하고 실제 뼈대 트랙만 저장했습니다. 상수 트랙도 보존하여 클립 전환 시 이전 자세가 남지 않도록 했습니다.
- 29개 클립 × 4개 시점 × 71개 뼈 자세를 Godot에서 임포트한 원본 FBX와 비교해 일치를 확인했습니다.
- 스킨 3종의 메시·재질·트랙 경로·재생 검사를 통과했고, D3D12 Forward+에서 미리보기를 렌더링해 확인했습니다. Unity와의 픽셀 단위 외관 비교는 하지 않았습니다.

검증 스크립트: 프로젝트 루트의 `tests/scifi_beast06_test.gd`.
