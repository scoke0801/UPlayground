# bOKUSEI 단계별 툰 셰이딩 비교 맵

## 사용법

콘텐츠 브라우저의 `Art/ToonTest/Maps`에서 `L_PGToon_Bokusei_ShadingComparison`을 연다.

기본 셰이딩 비교는 왼쪽부터 6단계다. 얼굴 SDF 테스트 에셋이 있으면 7/8단계도 함께 배치한다. Play(PIE) 후 뷰포트를 클릭하면 자유 카메라를 조작한다. 모든 모델은 같은 메시와 Leader Pose를 사용해 대기 모션을 공유한다.

현재 Bokusei의 실제 얼굴에는 SDF를 적용했다. 1–6단계는 적용 전 얼굴 Baseline을 사용해 비교 기준을 보존하며 7단계가 적용 결과다. 제작·게임 적용·최신 8단계 검증은 [얼굴 SDF 기록](BokuseiFaceSDF_Implementation_Report.md)을 따른다.

| 키 | 단계 | 이전 단계에서 달라지는 점 |
|---|---|---|
| 1 | 일반 조명 | 같은 베이스 텍스처·색상을 사용한 Default Lit 기준 |
| 2 | 셀 명암 | 모든 부위에 공통 의상 프로필의 3단 명암; 림·하이라이트 없음 |
| 3 | 부위별 명암 | 현재 Bokusei의 피부·얼굴·헤어별 명암색·강도·경계 부드러움 |
| 4 | 림·하이라이트 | 현재 재질의 윤곽 빛과 헤어·금속 하이라이트 |
| 5 | 외곽선 · 기존 툰 | CustomStencil 73 화면 공간 외곽선; SDF 적용 전 Bokusei 표현 |
| 6 | 월드 그림자 | 셀 명암·림·외곽선을 유지하고 Default Lit 월드 조명·수광과 머리카락 그림자 추가 |
| 7 | 얼굴 SDF | 5단계 기준에 모델 기반 얼굴 명암 추가; SDF 에셋 생성 시 표시 |
| 8 | 얼굴 SDF · 그림자 | 얼굴 명암과 월드 수광·헤어 그림자 결합; SDF 에셋 생성 시 표시 |

| 조작 | 동작 |
|---|---|
| WASD | 시선 기준 전후·좌우 이동 |
| Q / E | 월드 기준 하강 / 상승 |
| 우클릭 + 마우스 | 시선 회전 |
| 왼쪽 Shift | 3배 속도로 이동 |
| 1–8 | 해당 단계로 이동; 얼굴·쿼터뷰에서는 같은 시점 유지 |
| F | 선택 단계의 얼굴 근접 시점 |
| C | 선택 단계의 쿼터뷰 시점 |
| 0 / R | 전체 정면 비교로 복귀; 이후 1–8은 개별 정면 |
| H | 5–8단계의 같은 가림막 그림자를 켜기 / 끄기; 자체·바닥 그림자는 유지 |
| J | 6/8단계 머리카락 그림자를 켜기 / 끄기; 헤어 외형·월드 수광은 유지 |
| ← / → | 공통 주광원을 좌우로 회전; 누른 동안 초당 45° 조절 |
| ↑ / ↓ | 조명 높이 각도 조절; -85°부터 +85°까지 바닥·천정 조명 확인 |
| Z / X / V | 정면 0° / 측면 +60° / 역광 180°; 높이 35° |
| L | 현재 높이를 유지하며 초당 20° 자동 회전 켜기 / 끄기 |
| Backspace | 자동 회전을 끄고 재생 시작 시 저장된 주광원 방향 복원 |
| Shift + F1 | UE 기본 동작으로 마우스 해제 |

조작 안내는 재생 화면 하단에 계속 표시된다. `F → 2 → 3 → 4 → 5 → 6`으로 얼굴 시점을 유지하며 각 효과를 비교할 수 있다. 기본 상태는 헤어 그림자 켜짐·가림막 꺼짐이다. `6 → F → J`로 앞머리가 얼굴에 드리우는 그림자를 비교한다. `H`로 가림막 수광을 추가하고 5단계와 비교할 수 있다. 처음 재생한 상태에서 F/C를 누르면 6단계로 이동한다.

에디터에서는 `비교 카메라` 폴더의 정면·쿼터뷰·얼굴 카메라를 파일럿할 수 있다. `주광원 · 모든 단계 공통`의 방향을 조절하면 PIE에서 기존 `PGToonPresentationComponent`가 각 툰 재질의 광원 방향을 동기화한다.

재생 중에는 방향키와 Z/X/V로 같은 주광원을 조절한다. 수동 조절·프리셋·초기화는 자동 회전을 정지하고, 카메라 이동·단계 선택·0/R 전체 보기는 조명을 유지한다. 하단에 현재 방향·높이 각도와 자동 회전 상태를 표시한다. 방향 0°는 모델 정면(+Y), 높이 0°는 수평이다. 방향광의 위치 이동은 명암에 영향을 주지 않으므로 방향과 높이 각도로 테스트한다. 보조광·광원 강도·Source Angle·원본 재질은 유지하며 조작한 값은 맵에 저장하지 않는다.

`Tools/PlayBokuseiShadingComparison.ps1`로 비교 맵을 바로 실행할 수 있다. `8 → F → Z/X/V → L`로 얼굴 SDF·월드 수광·헤어 그림자를 비교하고 Backspace로 원래 조명에 돌아온다. `PGShadingComparisonPawn`의 `LightRotationSpeed`와 `LightOrbitSpeed`는 에디터에서 조절한다.

## 비교 기준

엔진은 실제 프로젝트의 UE 5.8.2다. 현재 bOKUSEI의 10개 메시 슬롯은 초기 Unlit 툰 마스터를 참조하므로 1–5단계는 이 구성을 적용 기준으로 사용했다. 6단계에만 비교 전용 월드 조명 툰 변형을 추가했다.

일반 Lit 변형은 같은 BaseTexture, BaseTint와 알파 마스크 파라미터를 복사한다. 머리카락·얼굴 부위의 Translucent와 의상 등의 Masked 모드를 유지한다. 2·3단계 재질은 원본 인스턴스를 부모로 상속하고 후속 효과만 비활성화한다. 재생성 시 전용 인스턴스의 기존 오버라이드를 정리해 원본의 최신 값을 반영한다. 4·5단계는 원본 재질을 그대로 참조하며 5/6단계가 외곽선에 참여한다. 모든 모델의 메시·크기·방향·LOD0·포즈·월드 광원·노출이 같다.

6단계는 기존 월드 조명 툰 그래프를 비교 전용 폴더에 재생성하고 원본의 모든 선언된 Scalar/Vector/Texture 파라미터 실효값을 복사한다. 추가 얼굴 노멀 평탄화와 헤어 이방성은 0으로 둔다. `WorldLightingInfluence=0.65`로 실제 수광과 35% 발광 채움을 혼합해 그림자 안의 얼굴·의상 가독성을 남긴다. 이 값은 `MI_PGShadow_*`에서 조절한다. 월드 조명 반응도 함께 추가되므로 5단계와 밝기가 완전히 같지는 않다. 얼굴 슬롯은 미세한 자체 그림자 투사를 제외하고 다른 부위·가림막의 그림자는 받는다.

`그림자 비교` 폴더의 가림막은 6단계 구성에서 2개, SDF를 포함한 8단계 구성에서 4개이며 모델에 같은 상대 위치를 사용한다. 보이는 메시를 숨긴 채 실제 엔진 그림자를 투사하며 H는 이 투사 여부만 바꾼다. 월드 수광 변형은 비교 전용이다.

보이는 두 헤어 슬롯은 Translucent를 유지한다. 6단계 `HairShadowProxy`만 동일 메시·LOD0·Leader Pose에 Masked 알파 재질을 사용해 실제 광원 그림자를 투사한다. 헤어 슬롯의 BaseTexture/OpacityTexture와 알파 계산값은 원본에서 복사한다. 투사체의 `OpacityCutoff`는 0.35로 두어 원본 반투명 슬롯의 0/0.001 컷오프가 헤어 카드 전체를 막는 문제를 피한다. 나머지 8개 슬롯은 OpacityMask 0으로 제외한다. 이 컴포넌트는 화면 색상·깊이·외곽선에 참여하지 않고 별도 애니메이션도 평가하지 않는다. `MI_PGHairShadow_*`의 OpacityCutoff로 투사 윤곽을 조절할 수 있다. J는 투사 여부만 바꾸므로 카메라·포즈·머리카락 외형이 같은 상태로 비교한다.

3단계 얼굴 보정은 현재 Unlit 재질의 명암 파라미터 튜닝이다. Advanced 재질의 얼굴 노멀 평탄화나 다른 셰이딩 모델을 추가하는 단계가 아니다. 림·하이라이트도 원본 값을 유지하므로 부위와 시선에 따라 변화량이 작을 수 있다.

일반 Lit은 동일 베이스 텍스처를 사용한 비교 기준이다. 새로운 PBR 텍스처나 사실적 재질을 제작한 결과는 아니며, 텍스처에 그려진 명암은 모든 단계에 남는다. 2–5단계 Unlit 툰은 엔진 조명 수광 대신 기존 셰이더의 분석적 명암을 사용하므로, 광원 강도 반응이 1/6단계와 다른 것은 현재 구현의 특성이다.

## 생성과 검증

- 맵: `/Game/Art/ToonTest/Maps/L_PGToon_Bokusei_ShadingComparison`
- 비교 전용 Lit·툰 수광 재질, 안내판, GameMode: `/Game/Art/ToonTest/BokuseiShadingComparison`
- 안내판 PNG와 재생성 원본: `Tools/Art/ToonTest/BokuseiShadingComparison`

UE 내장 Python으로 실행한다.

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunBokuseiShadingComparison.py
```

`--step configure`는 맵과 전용 에셋을 생성하고, `--step preview`는 저장된 결과를 새 UE 프로세스에서 다시 열어 검증·캡처한다. `--step preview --input-only`는 에디터 스크린샷을 건너뛰고 저장 재로드·PIE 포즈·카메라 입력만 검사한다. 다시 생성할 때 이전 비교 에셋과 맵을 `Saved/BokuseiShadingComparison/<실행 시각>/backup`에 보존한다. 원본 외형·재질·텍스처·대기 모션, 기존 갤러리와 게임 기본 맵은 변경하지 않는다.

## 재생 중 조명 조절 · 2026-10-08

`PGShadingComparisonPawn`이 비교 모델의 `PGToonPresentationComponent.KeyLight`를 한 번 찾아 공통 주광원의 방향을 조절한다. Pawn이 맵 액터보다 먼저 시작될 수 있어 다음 틱에 연결하고 한국어 안내를 갱신한다. 이후에는 광원 검색이나 추가 MID 생성 없이 기존 표현 컴포넌트가 툰·얼굴 SDF를 동기화한다. 카메라·보조광·광원 강도·Source Angle·원본 외형과 맵 에셋은 유지한다. 외부 각도 입력은 유한값만 허용하고 높이를 ±85°로 제한한다.

- UE 5.8 Development 에디터 빌드 PASS: `Saved/BokuseiShadingComparison/LightControlsBuild.log`. 기존 엔진 deprecated API·툴체인 경고는 유지한다.
- 최종 새 프로세스 재로드·PIE·입력 검사: `Saved/BokuseiShadingComparison/Runs/20261008T142420775947Z/run.json` PASS. `Preview/20261008T142438089103Z/preview.json`의 8개 모델·10개 슬롯·원본 패키지 해시, 35개 본 포즈 오차 약 `5.68e-14 cm`, 헤어 투사체 포즈 오차 0cm와 입력 **55개** PASS.
- 조명 검사 **19개**: 방향키 4축, 정면·측면·역광, 자동 회전 시작/정지 및 수동 입력·프리셋·초기화로 정지, 카메라 전체 보기에서 조명 유지, ±85° 제한·360° 순환, NaN/Infinity 거부, 시작 방향 복원 PASS. 각 검사에서 70개 MID의 `LightDirection`을 실제 주광원과 비교했고 최대 성분 오차는 `2.87e-8`이다. 조명 입력으로 카메라 위치·방향이 바뀌지 않는 것도 검사했다.
- 최종 `PlayableFace.png`와 `light_front.png` / `light_side.png` / `light_back.png`에서 8단계 얼굴 SDF·헤어 그림자의 방향 변화와 초기 연결·현재 각도·조작 안내를 시각 확인했다. 이 실행은 입력 전용이므로 에디터 단계별 렌더는 아래 별도 실행을 따른다.
- 전체 단계 렌더·H/J 픽셀은 `Preview/20261008T141807064247Z`에 보존한다. `shadow_pixels.json` PASS: H의 얼굴 명도 감소는 에디터 `2.928/255`, PIE `2.595/255`; J는 정면 `12.540/255`, 비스듬한 시점 `12.942/255`, PIE `14.858/255`다. 이 첫 전체 실행은 기존 33개 입력을 통과한 뒤 새 검사의 단계 선택 시점 기대값에서 FAIL했다. 쿼터뷰 유지 동작에 맞춰 전체 보기 복귀를 명시하고 입력 전용으로 재검증했다. 첫 렌더에서 발견한 시작 연결 안내 문제도 다음 틱 초기화로 보완했다.
- Python 구문·PowerShell 실행 도구 구문·`git diff --check` PASS. `Tools/PlayBokuseiShadingComparison.ps1`는 `.uproject`의 엔진 버전으로 해당 비교 맵을 직접 실행한다.

재검증 명령:

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunBokuseiShadingComparison.py --step preview --input-only
& Tools/Art/ToonTest/BokuseiShadingComparison/MeasureShadows.ps1 -PreviewDirectory Saved/BokuseiShadingComparison/Preview/20261008T141807064247Z
```

조작값은 재생 중에만 적용된다. 사용자 정의 조명 프리셋 저장과 장시간 자동 회전·패키지 실행 검증은 후속 폴리싱 범위다.

## 머리카락 그림자 품질 개선 · 중단 작업 재개 · 2026-10-07

중단된 작업에는 VSM 샘플·해상도·바이어스, 투사체 슬롯·양면·알파, 반투명 수광과 광원 크기를 나눈 진단이 남아 있었다. 마지막 깊이 보정 실행은 `Saved/BokuseiHairShadowQuality/20261007T132959403293Z`다. 당시 후보 JSON의 컷오프 0.9·단면·광원 6°는 생성 도구에 연결되지 않았으며, 캡처 완료의 PASS는 시각 품질 수용을 뜻하지 않았다.

재개 후 10/15/20° 및 별도의 16 ray/8 sample 진단을 비교했다. 최종값은 공통 주광원의 **Source Angle 20°**다. 컷오프 0.35·양면 투사·법선 안쪽 0.08cm는 유지한다. 광원 각도는 비교 맵에만 저장한다. 게임 외형·원본 얼굴 SDF·보이는 헤어 재질·전역 렌더 설정과 VSM의 기본 8 ray/4 sample은 유지한다. H 비교 가림막은 모든 5–8단계에서 같은 130×50×14cm로 맞춰 넓은 광원에서도 수광 차이를 읽을 수 있게 했다.

설정 원본은 `Tools/Art/ToonTest/BokuseiShadingComparison/hair_shadow_settings.json`이다. 생성 보고서에 설정·원본 SHA-256을 저장하고 새 프로세스에서 주광원·투사체 설정을 대조한다. `--final` 검사는 파일에 저장된 컷오프·양면·Inset을 확인하고, 예전 진단 기본값으로 덮어쓰지 않는다.

단일 `HighResShot`에서는 넓은 광원의 SMRT 노이즈가 두드러졌다. 실제 품질 비교는 **1600×900 고정 PIE 뷰포트의 `Shot`**과 안정된 TSR 이력으로 수행한다. 모델 포즈와 카메라를 고정하고 가림막을 끈 상태에서 기존 3°/개선 20°/헤어 투사 꺼짐을 비교한다. 런타임 광원·포즈 동작 검사는 별도의 기존 비교 맵 검증을 사용한다.

- 후보 렌더: `Saved/BokuseiHairShadowQuality/20261007T141216357207Z/quality.json`, 15장 PASS. 같은 폴더의 `Comparison.png`는 8단계 정면 전후다.
- `MeasureHairShadowQuality.ps1`의 피부 표본 검사: 같은 폴더 `shadow_quality_pixels.json` PASS. 헤어 그림자를 뺀 명도장의 인접 기울기 RMS는 6단계 정면/비스듬한 시점에서 각각 **31.5%/28.1%**, 8단계에서 **30.5%/29.0%** 감소했다. 얼굴 명도 감소는 12.77–13.51/255, 변경 피부 비율은 56.5–64.9%로 실제 투사도 유지했다. 이 지표는 해당 포즈·카메라에서 경계가 얼마나 급하게 변하는지 측정하며 전체 게임 품질 점수는 아니다.
- 초기 검사에서 사용한 절대 기울기 합은 단조 경계를 부드럽게 해도 보존되는 값이다. RMS 기울기로 수정했으며 초기 결과는 `total_variation_metric_fail.json`에 보존했다. 단일 고해상도 캡처의 노이즈 문제도 `20261007T135150833273Z/shadow_quality_pixels.json` FAIL로 보존한다.
- 최종 저장 설정 재로드·PIE 각도 렌더: `Saved/BokuseiHairShadowQuality/20261007T143252236435Z/quality.json` PASS. 6/8단계의 정면·비스듬한 시점 J 전후와 -60/0/+60° 조명 14장을 확인했다. 원본 패키지 해시를 보존했고, 셰이더 컴파일 오류는 없었다.
- 첫 전체 회귀 `Saved/BokuseiShadingComparison/Runs/20261007T142345948648Z`는 재로드·35개 본·헤어 포즈·33개 입력 PASS 후 H 픽셀 검사에서 FAIL했다. 주광원의 부드러움으로 PIE 가림막 명도 차이가 1.864/255로 낮아졌다. 픽셀 기준은 유지하고 비교 가림막의 깊이를 32→50cm로 확장했다.
- 최종 생성·새 프로세스 전체 회귀: `Saved/BokuseiShadingComparison/Runs/20261007T144225973588Z/run.json` configure/preview PASS. `Preview/20261007T144253345537Z/preview.json`의 8개 모델·10개 슬롯 저장값, 원본 패키지, 35개 본 오차 약 `5.68e-14 cm`, 헤어 투사체 포즈 오차 0cm와 실제 입력 33개 PASS. 같은 폴더 `shadow_pixels.json`에서 H 명도 감소는 에디터 2.918/255·PIE 2.626/255, J 얼굴·비스듬한 시점·PIE는 12.419/13.029/14.489로 모두 PASS다. Python 구문 및 `git diff --check`도 통과했다.

재현 순서:

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunBokuseiShadingComparison.py
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunBokuseiHairShadowQuality.py --candidate --temporal
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunBokuseiHairShadowQuality.py --final --temporal
```

첫 명령은 이전 비교 에셋·맵을 백업하고 생성·새 프로세스 재로드·PIE·H/J 피부 픽셀을 검사한다. 두 번째 명령은 비교 전후 픽셀 검사와 한국어 비교 시트도 생성한다. 세 번째는 저장 설정으로 각도별 렌더를 검증한다. 이번 변경은 C++ 수정이 없어 에디터 재빌드가 필요하지 않았다.

물리적 헤어 카드의 좁은 그림자는 일부 측면 광원에서 남는다. 광원 크기와 SMRT의 겹친 투사체 한계는 [UE 5.8 VSM 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine)의 Soft Shadows/Limitations에 설명돼 있다. 광원 크기를 더 키우거나 전역 샘플 수를 늘리는 방식은 채택하지 않았다. GPU 비용·밀집 전투·패키지 장시간 품질은 별도 확인 범위다.

## 5단계·자유 카메라 검증 · 2026-10-06

아래는 그림자 추가 전의 기록이다. 최신 검증은 다음 6단계 기록을 따른다.

- UE 5.8 에디터 Development C++ 빌드 PASS. 엔진의 기존 deprecated API·툴체인 경고는 남아 있다.
- 생성: `Runs/20261006T124358349644Z/run.json` PASS. 이전 맵·비교 에셋 백업과 원본 패키지 SHA-256 보존 확인.
- 새 프로세스 저장 재로드: 5개 모델의 10개 슬롯 텍스처·색상·알파 일치, 공통 셀 프로필과 부위별 프로필 상속, 4/5단계 원본 재질 일치, 마지막 모델만 외곽선 사용 PASS.
- SM6 원본 렌더: `Preview/20261006T124503086390Z`의 `Front.png`, `Quarter.png`, `Face.png`, `Stage1Face.png`–`Stage5Face.png`. 전체 배치와 한국어 안내판, 피부 색상·명암과 헤어 하이라이트의 단계 차이를 시각 확인했다.
- 초기 전체 렌더 실행은 캡처 후 PIE 검증 도구의 Python API 호출(`get_pawn`)에서 FAIL했다. 이후 `get_controlled_pawn`과 `Key.key_name`으로 수정했다. 단일 마우스 이벤트에 0을 즉시 덧붙이던 검증도 실제 드래그처럼 여러 프레임의 델타를 전달하도록 수정했다. 초기 실패 보고서는 보존한다.
- 최종 입력 전용 실행: `Runs/20261006T124910394682Z/run.json`, `Preview/20261006T124929655224Z/preview.json` PASS. 소유된 자유 카메라 Pawn 1개·안내 위젯 1개·툰 MID 40개, 머리·양손·양발의 20개 비교 표본 포즈 오차 약 `2.84e-14 cm`, 대기 모션 진행 확인.
- 실제 PlayerController 입력 경로 21개 PASS: 1–5 단계 선택, F 얼굴, 얼굴 시점 유지한 단계 변경, C 쿼터뷰, R/0 전체 복귀, WASD/QE 이동, Shift 최대 속도 300→900cm/s, 우클릭 없는 회전 차단·우클릭 좌우/상하 회전. 원본 3D 화면과 조작 안내를 `PlayableFace.png`, `PlayableQuarter.png`, `PlayableOverview.png`로 확인했다.

이 5단계 작업의 마지막 보고서는 입력 전용이므로 전체·단계별 에디터 렌더는 위 별도 실행의 PNG를 따른다. 전투·장비·공격 모션, 이동 중 장시간 화면 품질 및 패키지 성능은 검증 범위에 포함하지 않는다. Content는 별도 Git 저장소이므로 비교 전용 폴더와 맵을 소스 변경과 함께 관리한다.

## 6단계·실제 그림자 수광 검증 · 2026-10-06

- UE 5.8 에디터 Development 빌드 PASS. 생성·새 프로세스 전체 렌더/PIE는 `Runs/20261006T125705561546Z/run.json`의 configure/preview 모두 PASS.
- schema 3 맵 재로드: 6개 모델·10개 슬롯의 텍스처/색상/알파와 BlendMode 일치, 6단계 Default Lit 및 원본 셀 파라미터 유지, 5/6단계 외곽선, 같은 상대 위치의 숨겨진 그림자 투사체 2개 확인 PASS. 원본 패키지 해시 보존 PASS.
- PIE: 자유 카메라 1개·안내 위젯 1개·툰 MID 50개, 25개 본 포즈 비교 최대 오차 약 `5.68e-14 cm`, 대기 모션 진행 PASS. 기존 이동·회전·시점 전환에 6단계 선택과 H 꺼짐/켜짐을 더한 실제 입력 경로 24개 PASS.
- SM6 캡처는 `Preview/20261006T125737366931Z`: 전체 정면·쿼터뷰·5/6 비교 얼굴, 1–6 단계 얼굴, 5/6단계 가림막 그림자 꺼짐, PIE 얼굴·쿼터뷰·전체와 H 전후를 기록했다. 얼굴과 의상의 실제 수광, 가림막 메시가 보이지 않는 점과 한국어 토글 상태 안내를 시각 확인했다.
- `MeasureShadows.ps1`은 고정 포즈/카메라의 얼굴·헤어 중앙 영역을 읽는다. 원본 5단계 그림자 전후 평균 RGB 차이 `0.123/255`, 6단계 `7.994/255` 및 변경 표본 `33.7%`, PIE H 전후 `8.461/255` 및 `34.1%`로 수광 PASS. 두 수광 비교에서 그림자 켜짐의 평균 명도가 각각 7.83/7.68 낮아졌다. 결과는 같은 폴더의 `shadow_pixels.json`이다. 이후 전체 runner도 이 픽셀 검증을 수행한다.

위 기록은 헤어 투사체 추가 전의 월드 그림자 검증이다. 최신 기계 보고서는 `Saved/BokuseiShadingComparison/configure.json`, `preview.json`이다. 얼굴 가독성의 세부 폴리싱, 장시간 이동 중 품질·패키지 성능은 별도다.

## 머리카락 그림자 검증 · 2026-10-06

- `PGToonPreviewActor.HairShadowProxy`는 6단계와 얼굴 SDF가 있을 때의 8단계에만 메시를 지정한다. 숨김 그림자·Masked 알파와 포즈 공유를 사용하며 J로 두 투사체를 함께 전환한다. 가림막은 기본 꺼짐이다.
- 헤어의 투사체 컷오프 0.35와 `ShadowInset=-0.08 cm`를 사용한다. 0.8mm 안쪽 보정으로 보이는 헤어와 투사체가 같은 표면에 겹쳐 생기는 미세한 자기 그림자 얼룩을 줄였다. 보이는 헤어의 반투명 재질·텍스처·알파는 유지한다.
- 에디터 Development C++ 빌드 PASS: `Saved/BokuseiShadingComparison/HairShadowBuild.log`. 새 프로세스 저장 재로드에서 헤어 슬롯 2개·나머지 슬롯 제외, 동일 메시/LOD/Leader Pose, 화면 색상·깊이·외곽선에 참여하지 않는 투사체 설정을 확인했다.
- 최종 SM6 캡처: `Preview/20261006T132945165930Z`. `Stage6Face.png`/`Stage6HairShadowOff.png`, `Stage6HairQuarterOn.png`/`Off.png`, `PlayableHairShadowOn.png`/`Off.png`에서 얼굴의 실제 헤어 그림자를 확인했다. 모든 헤어 비교는 가림막을 끈 고정 카메라·포즈다.
- 같은 폴더의 `shadow_pixels.json` PASS. 노출된 피부만 선택한 정면 4,376개 표본의 평균 명도 감소 `12.453/255`, 변경 비율 `52.1%`; 비스듬한 얼굴 `12.760/255`·`61.2%`; PIE J 전후 `15.100/255`·`57.5%`. 기존 Unlit 기준의 가림막 전후 차이는 `0.125/255`이며 월드 수광·H 토글도 PASS다.
- 전체 렌더 실행은 캡처·PIE·H/J 입력 성공 뒤 7단계 추가 카메라 검사에서 FAIL했다. 전체 보기로 돌아온 뒤의 단계 선택을 얼굴 시점으로 잘못 기대한 검증을 수정했다. 최종 입력 전용 실행 `Runs/20261006T133442347872Z/run.json`, `Preview/20261006T133502459944Z/preview.json` PASS: 8개 모델·35개 본 비교·헤어 투사체 2개의 포즈 오차 0cm·자유 카메라 1개·입력 33개. 렌더/픽셀 검증과 입력 재검증의 근거를 각각 위 실행에 보존한다.

현재 비교 맵의 앞머리 투사와 카메라 이동·포즈 동기화를 검증했다. 애니메이션식 그림자 모양의 추가 연출 튜닝과 다수 캐릭터·장시간·패키지 성능은 별도다.

## 이전 2개 모델 비교 검증 · 2026-10-05

초기 `Runs/20261005T135551745138Z/run.json`의 configure/preview 모두 PASS:

- 저장 재로드 후 10개 슬롯의 베이스 텍스처·색상·투명도 파라미터와 BlendMode 일치 PASS.
- 원본 패키지 SHA-256 보존 PASS.
- PIE에서 비교 모델 2개, 불필요한 기본 Pawn 0개, 툰 MID 10개 확인 PASS.
- 머리·양손·양발의 포즈 차이 0cm, 대기 모션 진행 확인 PASS.
- SM6 정면·쿼터뷰·얼굴 렌더의 모델, 머리카락·얼굴 투명 부위, 한국어 안내판을 확인했다. 첫 고해상도 프레임의 투명 재질 준비 지연을 피하도록 준비용 프레임을 먼저 캡처하고 실제 3장을 기록한다.

당시 원본 캡처는 `Saved/BokuseiShadingComparison/Preview/20261005T135620081108Z`의 `Front.png`, `Quarter.png`, `Face.png`다. 최신 사용법·검증은 위 5단계 기록을 따른다.
