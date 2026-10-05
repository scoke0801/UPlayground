# Nenmir · Spi_Reien · Suiha · lili · Hichi 툰 이전

## 결과와 사용

Unity 프로젝트 `Assets/ExternalAssets/Character`의 요청한 5종 기본 FBX를 UE 5.8의 `/Game/Art/ToonCharacters`에 가져왔다. 스켈레탈 메시·Skeleton·PhysicsAsset, 툰 머티리얼 인스턴스 56개, 텍스처 38개를 저장한다. Unity 원본은 읽기만 한다.

| 캐릭터 | 원본 FBX | 메시 | 재질 슬롯 |
| --- | --- | --- | ---: |
| Nenmir | `Nenmir/FBX/MB_Nenmir.fbx` | `Nenmir/SK_PG_Nenmir` | 8 |
| Spi_Reien | `Spi_Reien/FBX/reien_fbx.fbx` | `Spi_Reien/SK_PG_Spi_Reien` | 18 |
| Suiha | `Suiha/FBX/Suiha.fbx` | `Suiha/SK_PG_Suiha` | 9 |
| lili | `lili/Models/lili.fbx` | `lili/SK_PG_lili` | 14 |
| Hichi | `Hichi/FBX/Hichi.fbx` | `Hichi/SK_PG_Hichi` | 7 |

표의 메시 경로는 `/Game/Art/ToonCharacters/` 기준이다. 각 캐릭터 폴더의 `BP_PG_<이름>_Toon`을 레벨에 배치할 수 있다. 전체 비교 맵은 `/Game/Art/ToonCharacters/Maps/L_PG_ToonCharacters`다. 기본 포즈 외형 확인용 블루프린트이며 `PGToonPreviewActor`의 기존 조명·머리 본 동기화를 사용한다.

## 커스텀 셰이딩

- 기존 `/Game/Art/ToonTest/Advanced/Materials/M_PGToonWorld_multi`와 `M_PGToonWorld_transparent`를 재사용한다. 월드 조명·그림자, 셀 명암, 림, 헤어 하이라이트와 얼굴 명암을 부위별 프로필로 설정했다.
- Unity 셰이더 GUID로 불투명/컷아웃/투명을 구분하고 기본 색상, 베이스 알파, 별도 알파 마스크, 불투명도를 연결한다. 색상과 선형 마스크 텍스처를 분리하며 런타임 최대 텍스처 크기는 2048로 설정한다. 원본 해상도는 임포트 소스에 보존한다.
- Suiha·Hichi의 원본 반투명 머리카락은 `_ZWrite=1`을 사용한다. UE 반투명 정렬에서 발생한 겹침 아티팩트를 근접 렌더로 확인해 해당 3슬롯만 깊이를 기록하는 컷아웃 마스터와 0.25 임계값으로 변환했다. 원본의 알파 텍스처/마스크를 유지하며 부드러운 반투명 그라데이션은 컷아웃 경계로 근사한다.
- FBX `.meta`의 외부 재질 GUID를 우선한다. Hichi는 해당 매핑이 비어 있어 원본 `Hichi.prefab`의 렌더러별 재질 GUID를 확인해 명시적으로 연결했다.
- 블루프린트는 CustomDepth/Stencil 73을 사용한다. 갤러리에 기존 `M_PGToonScreenOutline`을 사용하는 무한 범위 PostProcessVolume을 저장했다. 다른 맵에서도 외곽선을 쓰려면 해당 포스트 프로세스와 `r.CustomDepth=3`이 필요하다. `ToonPresentation.KeyLight`로 기준 DirectionalLight를 지정할 수 있다.

## 소스 누락과 범위

Suiha의 금속 2슬롯과 렌즈 1슬롯이 참조하는 아래 텍스처 GUID는 원본 캐릭터 폴더와 Unity `Assets` 검색에서 발견되지 않았다. 금속 GUID는 Packages/PackageCache에서도 발견되지 않았다. 흰 기본 텍스처에 원본 색상·렌즈 불투명도와 툰 금속 프로필을 적용했다. 이 대체 처리는 `configure.json`의 Suiha `warnings`에 남긴다.

- 금속: `bfc4d5931015d094bb8fd8ef2f023a16`
- 렌즈: `e48975e0e74a2eb46bdcb57746dc8fc8`

Unity lilToon 자체를 이식한 것은 아니다. Unity 전용 MatCap·스텐실 페이크 그림자·VRChat 동작은 프로젝트의 툰 표현으로 대체하거나 이전 범위에서 제외한다. 원본 FBX의 Bind Pose 재생성, 스무딩/법선, 최대 스킨 영향도 초과 경고가 있으며 실제 애니메이션 변형·옷/머리카락 물리는 별도 검수가 필요하다. 추가 의상·색상 변형·애니메이션 리타게팅·플레이어/GAS 교체는 포함하지 않는다.

## 재현과 검증

UE 5.8 내장 Python으로 아래 순서대로 실행한다.

```powershell
$uePython = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
& $uePython Tools/Validation/PrepareToonCharacterBatch.py
& $uePython Tools/Validation/RunToonCharacterBatch.py --step configure
& $uePython Tools/Validation/RunToonCharacterBatch.py --step preview
& $uePython Tools/Validation/RunToonCharacterBatch.py --step validate
```

`Tools/Art/ToonCharacters/manifest.json`이 소스 매핑과 해시를 보존한다. Configure 재실행 전에는 기존 새 캐릭터 에셋 폴더를 `Saved/ToonCharacters/Backups`에 복사한다. 기존 메시가 있으면 재임포트하지 않고 재질을 갱신한다. 원본 FBX 자체를 변경한 경우에는 별도 재임포트가 필요하다.

검증 산출물은 `Saved/ToonCharacters`의 `configure.json`, `preview.json`, `validate.json`과 `Runs/<실행 시각>` 로그다. 전신·얼굴·쿼터뷰 각 5장과 전체 갤러리 1장, 총 16장의 SM6 렌더를 생성한다. Validate는 새 에디터 프로세스에서 슬롯/마스터/텍스처/알파 파라미터, Skeleton, 블루프린트 생성, 갤러리 5개 액터, 머리 본, MID 초기화, 바운드, 소스 해시를 검사한다.

2026-10-04 최종 결과:

- Configure **PASS**: 5개 메시, 재질 슬롯 56개, 텍스처 38개. 실행 `Runs/20261004T142801089316Z`.
- Preview **CAPTURED**, 종료 코드 0: SM6 1280×720 PNG 16장. 전체 배치 및 5종 얼굴을 시각 확인했고, Suiha/Hichi의 투명 헤어 정렬 결함을 보정 후 재렌더했다. 실행 `Runs/20261004T142900494662Z`. 초기 결함 비교용 얼굴 캡처는 `BeforeHairFix`에 보존했다.
- Validate **PASS**, 종료 코드 0: 새 프로세스에서 5개 블루프린트, 56개 슬롯, 38개 텍스처 연결, 3개 헤어 컷아웃 설정, 저장된 갤러리와 MID 초기화 검증. 소스 파일 252개 해시 불변. 실행 `Runs/20261004T143312955644Z`.
- 신규 산출물은 `.uasset` 114개와 `.umap` 1개다. C++ 변경은 없으며 실제 플레이어 애니메이션·전투·패키징 성능 검증을 의미하지 않는다.
