# Zombie Starter FBX 이관

- 상태: 변환 및 검증 완료
- 작성일: 2026-11-12
- 리소스: `resources/characters/zombie_starter/`
- 엔진: Godot 4.7.2, D3D12 Forward+

## 결과

원본 모델 FBX 1개와 동작 FBX 52개를 가져왔습니다. 동작은 26종의 In Place / Root Motion 버전입니다. 실제 게임용으로는 공유 모델 1개, 애니메이션 라이브러리 3개(In Place, Root Motion, RESET/T_Pose), 재질 1개, 원본 2K 텍스처 2개, 캐릭터 씬과 미리보기 씬을 만들었습니다.

## 호환성 처리

- 외부 FBX2glTF 프로그램이 필요한 구형 임포터 대신 Godot 내장 ufbx를 명시했습니다.
- 원본 FBX에 Bad UTF-8 string 경고가 있어 보관용 원본을 `.gdignore`로 제외하고 실행 리소스를 `.scn` / `.res`로 저장했습니다. 원본 파일 자체는 변경하지 않았습니다.
- 원본 제작 카메라와 애니메이션 FBX마다 중복된 메시를 게임용 씬에서 제거했습니다.
- 상수 트랙을 보존하고 원본의 반복 보간 설정을 유지하여 클립 전환 및 반복 설정에 따른 자세 변화를 방지했습니다.
- Phong Specular 텍스처는 GPU 셰이더의 반사 강도로 연결했습니다. 반사 색조와 Phong → GGX 변환은 근사이며, 원본 제작 도구와 동일한 픽셀 출력을 보장하지 않습니다.

## 검증

- 모든 동작에서 73개 뼈의 이름·부모 계층 및 스킨 바인드 행렬 일치.
- 원본 FBX와 52개 클립의 0%, 25%, 50%, 75% 시점에서 15,184개 뼈 자세 일치.
- `tests/zombie_starter_test.gd`: 52개 동작 + RESET/T_Pose, 각 클립의 219개 트랙, 텍스처·바인딩, 실제 발 움직임 검증.
- 걷기 수평 변위: In Place 약 0.00000075m, Root Motion 약 0.89472m.
- 대기·걷기·공격 및 기어가기·사망·빠른 추격의 실제 렌더링 확인.

## 사용 시 남은 작업

원본 이동은 `Skeleton3D:Hips`에 저장되어 있습니다. 게임용 충돌 이동 및 루트 모션 추출·누적, 다른 리그로의 리타게팅, AI/전투 로직은 별도 작업입니다. 기본 사용법은 리소스 폴더의 README를 참고하세요.

재질 참고: [Godot Spatial shader의 SPECULAR / ROUGHNESS](https://docs.godotengine.org/en/stable/tutorials/shaders/shader_reference/spatial_shader.html).
