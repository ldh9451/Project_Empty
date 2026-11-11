# SciFi Beast06 Bull 리소스 이관

2026-11-12, Godot 4.7.2에서 변환 및 검증.

리소스 위치: `resources/characters/scifi_beast06_bull/`.

## 결과

- 공유 모델 1개, 스킨 씬 3개, 미리보기 씬 1개.
- 재질 7개: 스킨별 Body/Armor 6개 + 공통 Glass 1개.
- 텍스처 32개: 원본 TGA 26개의 PNG 변환본 + 거칠기 맵 6개.
- 애니메이션 29개 + RESET. 캐릭터에 필요한 71개 뼈의 위치·회전·크기 트랙을 보존.
- 원본 FBX와 Unity 설정 파일은 `source/`에 보관. 실행에 필요한 데이터는 Godot 네이티브 리소스로 저장.

## 변환 시 주의한 점

1. 원본 모델 FBX 3개는 SHA-256이 같아 하나로 합쳤습니다. 스킨은 재질 오버라이드로 구분합니다.
2. Unity Standard Shader의 Smoothness는 Godot Roughness와 반대이므로 MetalGloss의 알파를 반전했습니다. 발광 연산은 곱셈을 사용합니다.
3. 애니메이션 전용 FBX의 시작 자세는 모델 바인드 자세와 다릅니다. 상수 트랙 제거를 꺼서 임포트하고, 누락 채널은 해당 애니메이션의 원본 자세를 사용했습니다.
4. IK 제어용 별도 뼈대와 노드는 모델에 없으므로 애니메이션 라이브러리에서 제외했습니다. 실제 `Skeleton3D` 뼈 트랙만 남겼습니다.
5. 변환한 모델은 원본 FBX 경로를 참조하지 않는 `.scn`, 애니메이션은 외부 의존성이 없는 `.res`입니다.

## 검증

- 원본 FBX와 29개 클립의 0%, 25%, 50%, 75% 시점 자세 비교 통과: 8,236개 뼈 자세 일치.
- `tests/scifi_beast06_test.gd`: 스킨 3개 로드, 메시/바인딩/재질 확인, 모든 클립의 213개 트랙 및 뼈 경로 확인, 유효한 자세와 Idle 움직임 확인.
- 실제 D3D12 Forward+ 렌더링 확인.

원본 Unity 데모 씬과 Animator 상태 전이, 게임용 충돌·이동·전투 로직은 이관 범위에 포함하지 않았습니다. 사용 방법과 애니메이션 목록은 리소스 폴더의 `README.md`에 있습니다.

참고: [Godot 3D 리소스 임포트 문서](https://docs.godotengine.org/en/stable/tutorials/assets_pipeline/importing_3d_scenes/available_formats.html), [StandardMaterial3D](https://docs.godotengine.org/en/stable/classes/class_standardmaterial3d.html).
