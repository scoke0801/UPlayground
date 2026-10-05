# Hwarin · Arin · Yura 플레이어 프리팹 이전

## 위치

Unity `Assets/03.Prefabs/Actor/Player/Models/PlayerModel_{Hwarin,Arin,Yura}.prefab`의 표시 모델을 `/Game/Art/PlayerModels`에 가져온다. 원본 Unity 파일은 읽기만 한다.

| 프리팹 | 원본 조합 | 활성 메시 부위 | Unreal 메시 |
| --- | --- | ---: | --- |
| PlayerModel_Hwarin | Honoka + 프리팹 색상 변형 | 22 | `/Game/Art/PlayerModels/Hwarin/SK_PG_Hwarin` |
| PlayerModel_Arin | Nenmir + SaltLine_Marubody 의상 | 19 | `/Game/Art/PlayerModels/Arin/SK_PG_Arin` |
| PlayerModel_Yura | Hichi + Twin Bun Braids 헤어 | 21 | `/Game/Art/PlayerModels/Yura/SK_PG_Yura` |

각 폴더의 `BP_PG_<이름>_Toon`은 레벨 배치용 외형 블루프린트다. 확인 맵은 `/Game/Art/PlayerModels/Maps/L_PG_PlayerModels`다.

## 변환 범위

- 프리팹 루트의 비활성 상태는 모델 보관용으로 간주하고, 자식 계층의 활성 상태와 Renderer 활성 상태로 실제 부위를 선택한다. Hwarin의 긴 뒷머리, Arin의 원래 의상 및 비활성 무기, Yura의 원래 헤어를 제외한다.
- 프리팹의 재질 GUID를 우선한다. FBX와 Unity의 다중 재질 순서가 다른 부위는 이름으로 대응하며, 단순 배열 순서로 연결하지 않는다. Unity 추가 패스인 Arin의 헤어 FakeShadow는 별도 형상으로 복제하지 않는다.
- 프리팹의 기본 BlendShape 값을 베이스 형상에 반영한다. 표정 등 나머지 ShapeKey의 상대 변형은 유지한다. Blender/Unity의 전체 ShapeKey 수가 다른 부위는 기본값이 0인 추가 키를 그대로 둔다.
- 여러 FBX의 공유 본은 하나의 리그로 합친다. Yura의 추가 헤어 `head`는 Unreal의 대소문자 무시 이름 충돌을 막기 위해 `PG_HairRoot`로 바꾸고 `Head` 아래에 연결한다.
- FBX 바인드 포즈를 사용한다. Unity의 Animator, 런타임 스크립트, 제약·의상 물리는 변환하지 않는다. 최초 가져오기 후 추가 요청으로 Unreal의 플레이어 선택·리타게팅·기존 GAS 연결을 적용했으며 아래 후속 기록을 따른다.
- 기존 PG 월드 조명 툰 마스터, 부위별 프로필, 텍스처·알파 마스크·원본 색상을 사용한다. lilToon의 전체 패스를 동일하게 재현하는 변환은 아니다. 런타임 텍스처 크기는 기존 파이프라인과 동일하게 최대 2048이다.

## 재현

1. UE 내장 Python으로 `Tools/Art/PlayerModels/PrepareSources.py` 실행.
2. Blender 5.2의 background 모드로 `Tools/Art/PlayerModels/BuildModels.py` 실행.
3. UE 내장 Python으로 `Tools/Art/PlayerModels/RunImport.py --step configure`, `--step preview`, `--step validate` 순서로 실행.

`sources.json`은 읽은 프리팹과 참조 목록, `manifest.json`은 생성한 FBX·재질 대응·기본 Morph·원본 해시를 기록한다. `FBX`에 Unreal 재임포트용 합성 원본을 보관한다. 기존 메시가 있으면 configure는 재질만 갱신한다. FBX를 다시 생성한 경우에는 `RunImport.py --step configure --reimport <이름>`으로 해당 메시를 재임포트한다. Configure 실행 전에는 이 폴더의 기존 패키지를 `Saved/PlayerModels/Backups`에 보관한다.

실행 로그와 검증 결과, 전신·얼굴·쿼터뷰 렌더는 `Saved/PlayerModels`에 저장한다. 공용 배치 스크립트는 환경 변수로 새 매니페스트/출력 경로를 선택하며 기존 캐릭터 배치의 기본 경로를 유지한다.

## 검증

- Configure **PASS**, 종료 코드 0: 스켈레탈 메시 3개, 재질 슬롯/인스턴스 41개(Hwarin 15, Arin 18, Yura 8), 텍스처 36개. 참조 소스 102개의 해시가 유지됐다. 최종 저장 실행: `Saved/PlayerModels/Runs/20261005T083020_configure`.
- Yura 리본은 원본 `_AlphaMaskMode=1`이지만 텍스처 참조가 비어 있다. 원본 셰이더의 기본 흰 마스크를 엔진 흰 텍스처로 명시해 연결했다. 외부 파일 누락에 대한 임의 색상 대체는 없다.
- 임포트 로그에 바인드 포즈 재생성 및 퇴화한 면의 0 길이 법선 경고가 있다. 원본 모프의 부위 숨김과 복합 리그에서 발생하는 항목이며, 애니메이션 변형·래그돌 품질 검증은 별도다.
- 첫 재로드 검사에서 Yura의 수정 전 헤어 본이 남은 것을 검출했다. 수정된 FBX로 재임포트하면서 해당 전용 Skeleton을 재생성했고, 후속 새 프로세스에서 `PG_HairRoot`와 부모 `Head`를 확인했다. 본 구조 변경 시 Unreal이 찍은 스켈레톤 병합 진단 때문에 재임포트 명령렛은 종료 코드 1이었으나, 이후 일반 configure 저장과 validate는 종료 코드 0으로 통과했다. 원래 실패 로그도 `Runs`에 보존한다.
- 재임포트 후 Validate **PASS**: 3개 BP, 41개 재질 참조/알파 파라미터, 텍스처 36개, 갤러리/머리 본/동적 머티리얼 초기화 및 원본 해시 검사. 본 수는 Hwarin 465, Arin 387, Yura 579다. 실행: `Runs/20261005T082939_validate`.
- 최종 Preview **CAPTURED**, 종료 코드 0: `Runs/20261005T083054_preview`에서 SM6 1280×720 렌더 10장(전체 1 + 전신/얼굴/쿼터뷰 각 3)을 갱신했다. 세 모델의 부위·색상·눈·헤어 실루엣을 시각 확인했고, Yura의 헤어 본 수정 후 전신/얼굴도 다시 확인했다.
- 최종 Validate **PASS**, 종료 코드 0: `Runs/20261005T083341_validate`에서 최종 저장/렌더 뒤 새 프로세스로 모든 참조, Yura 헤어 부모 본, BP/MID, 갤러리, 이미지 10장과 소스 해시 불변을 다시 검증했다.
- 신규 산출물은 `.uasset` 89개와 `.umap` 1개다. 리타게팅·전투·물리·패키지 성능 수용을 의미하지 않는다.

## 2026-10-05 플레이어 연결 및 지정 일러스트 적용

- `DA_Hwarin`, `DA_Arin`과 각각의 Source/Target IK Rig·Retargeter를 추가했다. `DA_Hichi`는 표시 이름 `Yura`, 메시 `SK_PG_Yura`로 교체하고 기존 저장 ID `Hichi`를 유지한다. 카탈로그는 기존 순서 뒤에 Hwarin·Arin을 더한 총 9종이다.
- 합성 FBX의 루트 배율 100과 변경된 로컬 본 축에 맞춰 Target 포즈를 다시 정렬하고 `bReconstructScaledTranslations`를 켰다. 기존 원본 메시·GAS·장비 구조를 공유하며 손·발·손가락의 표시 본 매핑을 연결했다. 추가 부위는 이미 합성 메시에 포함되어 `Parts`를 중복 연결하지 않는다.
- 초상화는 사용자가 지정한 `Tools/Art/MoonlitUI/AdditionalPortraits_20261005/T_Yura_v2.png`, `T_Hwarin.png`, `T_Arin.png`다. Yura 텍스처의 에셋 ID는 기존 참조를 유지하는 `T_Hichi`다. PNG 원본은 수정하지 않았으며 UI 재생성도 `PlayableCharacterCatalog.portrait_source`를 사용한다.
- `RunConnection.py`는 변경 패키지·폴리싱 JSON을 백업하고 적용 후 별도 UE 프로세스에서 검증한다. 실패하면 에디터 종료 뒤 백업을 복구한다. 이후 일반 재생성 원본은 `PlayableCharacterPolish.json`의 52개 에셋이며 나머지 플레이어 6종과 적 4종의 의미 데이터는 유지했다.
- 에디터 빌드 **PASS**: `Saved/PlayerModels/connection-build.log`. 연결·52개 에셋의 저장 의미 비교 **PASS**: `Saved/PlayerModels/Connections/20261005T084926354448Z`. 초상화 9종의 저장 참조·지정 PNG SHA-256·UI 설정 재로드 **PASS**: `Saved/PlayerModels/PortraitValidation`.
- 실제 게임 렌더 및 자동 검사 **PASS**: `Saved/PlayableCharacters/Runs/20261005T085059_runtime`. 플레이어 9종의 선택, 저장 실패 원자성, 공격 중 변경 차단, GAS 일반 공격, 표시 본 변형·신체 비율, 무기 매핑 캐시와 P09 4종의 AI/GAS를 검사했다. 신규 3종의 대기·공격 화면을 시각 확인했다. PG 자동화 **PASS**: `Runs/20261005T085142_automation`.
- 검사는 격리 프로필과 자동 입력 경로를 사용한다. 손가락별 그립 미세 조정, 의상·헤어 물리, 모든 전투 모션의 클리핑 및 패키지 성능 검수는 별도 범위다.
- ?? ???? ?? ??? ?????? ??????? ?? ??? 1920?1080 ?? **PASS**: `Saved/QA/20261005T085249Z_5f501c7f_inventory`. Yura(Hichi ID)?Hwarin?Arin ? ??? ?? ??? 10?(?? 9 + ?? 1), ?? 0??. PG ???? ? 45? ??, ?? 0??.
