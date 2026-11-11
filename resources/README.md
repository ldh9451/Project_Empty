# 게임 리소스

캐릭터별 리소스는 `characters/<캐릭터명>/`에 모읍니다. 모델·재질·텍스처·애니메이션·배치용 씬을 각각 구분합니다.

- [SciFi Beast06 Bull](characters/scifi_beast06_bull/README.md): 스킨 3종, 애니메이션 29개.
- [Zombie Starter](characters/zombie_starter/README.md): MotusMan 모델, 좀비 동작 26종의 제자리/이동 포함 버전 총 52개.

## Git LFS

이 폴더의 `.fbx`, `.png`, `.jpg`, `.scn`, `.res`, `.ma` 파일은 Git LFS로 관리합니다. 코드·씬 텍스트(`.tscn`)·재질(`.tres`)·문서는 일반 Git으로 관리합니다.

Git LFS가 설치된 환경에서 평소처럼 커밋·푸시하면 대용량 파일도 함께 업로드됩니다. `.gitattributes`를 반드시 함께 커밋하세요.

다른 컴퓨터에서 저장소를 받은 뒤에는 프로젝트 폴더에서 다음을 실행합니다.

```powershell
git lfs install --local
git lfs pull
```

모델을 임포트하기 전에 LFS 다운로드를 마쳐야 합니다. LFS 포인터 파일만 받은 상태에서는 Godot가 모델·텍스처를 읽을 수 없습니다.
