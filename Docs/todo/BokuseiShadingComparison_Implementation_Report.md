# bOKUSEI 툰 셰이딩 비교 맵

## 사용법

콘텐츠 브라우저의 `Art/ToonTest/Maps`에서 `L_PGToon_Bokusei_ShadingComparison`을 연다.

- 왼쪽: 툰 미적용, 일반 Default Lit 재질.
- 오른쪽: 현재 `DA_Bokusei` 메시의 Unlit 툰 재질과 CustomStencil 73 화면 공간 외곽선.
- 재생: 정면 비교 카메라에서 대기 모션을 반복한다. 두 모델은 Leader Pose로 같은 포즈를 공유한다.
- 다른 시점: 아웃라이너의 `비교 카메라` 폴더에서 `카메라 · 쿼터뷰 비교` 또는 `카메라 · 얼굴 비교`를 우클릭해 카메라를 파일럿한다.
- 조명 비교: `주광원 · 양쪽 공통`의 방향과 강도를 조절한다. PIE에서 툰 재질의 광원 방향도 기존 `PGToonPresentationComponent`로 동기화된다.

## 비교 기준

엔진은 실제 프로젝트의 UE 5.8.2다. 현재 bOKUSEI의 10개 메시 슬롯은 초기 Unlit 툰 마스터를 참조하므로 이 구성을 적용 기준으로 사용했다. 별도의 Advanced 월드 조명 툰 변형으로 대체하지 않았다.

일반 Lit 변형은 같은 BaseTexture, BaseTint와 알파 마스크 파라미터를 복사한다. 머리카락·얼굴 부위의 Translucent와 의상 등의 Masked 모드를 유지하며, 셀 명암·림·발광 채움·외곽선을 제거한다. 두 모델의 메시·크기·방향·LOD0·포즈·카메라·월드 광원·노출이 같다.

일반 Lit은 동일 베이스 텍스처를 사용한 비교 기준이다. 새로운 PBR 텍스처나 사실적 재질을 제작한 결과는 아니며, 텍스처에 그려진 명암은 양쪽에 남는다. 오른쪽 Unlit 툰은 엔진 조명 수광 대신 기존 셰이더의 분석적 명암을 사용하므로, 광원 강도 반응이 왼쪽과 다른 것은 현재 구현의 특성이다.

## 생성과 검증

- 맵: `/Game/Art/ToonTest/Maps/L_PGToon_Bokusei_ShadingComparison`
- 비교 전용 Lit 재질, 안내판, GameMode: `/Game/Art/ToonTest/BokuseiShadingComparison`
- 안내판 PNG와 재생성 원본: `Tools/Art/ToonTest/BokuseiShadingComparison`

UE 내장 Python으로 실행한다.

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunBokuseiShadingComparison.py
```

`--step configure`는 맵과 전용 에셋을 생성하고, `--step preview`는 저장된 결과를 새 UE 프로세스에서 다시 열어 검증·캡처한다. 다시 생성할 때 이전 비교 에셋과 맵을 `Saved/BokuseiShadingComparison/<실행 시각>/backup`에 보존한다. 원본 외형·재질·텍스처·대기 모션, 기존 갤러리와 게임 기본 맵은 변경하지 않는다.

2026-10-05 최종 `Runs/20261005T135551745138Z/run.json`의 configure/preview 모두 PASS:

- 저장 재로드 후 10개 슬롯의 베이스 텍스처·색상·투명도 파라미터와 BlendMode 일치 PASS.
- 원본 패키지 SHA-256 보존 PASS.
- PIE에서 비교 모델 2개, 불필요한 기본 Pawn 0개, 툰 MID 10개 확인 PASS.
- 머리·양손·양발의 포즈 차이 0cm, 대기 모션 진행 확인 PASS.
- SM6 정면·쿼터뷰·얼굴 렌더의 모델, 머리카락·얼굴 투명 부위, 한국어 안내판을 확인했다. 첫 고해상도 프레임의 투명 재질 준비 지연을 피하도록 준비용 프레임을 먼저 캡처하고 실제 3장을 기록한다.

최신 기계 검증 결과는 `Saved/BokuseiShadingComparison/configure.json`, `preview.json`이다. 최종 원본 캡처는 `Saved/BokuseiShadingComparison/Preview/20261005T135620081108Z`의 `Front.png`, `Quarter.png`, `Face.png`다. 전투·장비·공격 모션과 패키지 성능 검증은 이 시각 비교 맵의 범위에 포함하지 않는다.
