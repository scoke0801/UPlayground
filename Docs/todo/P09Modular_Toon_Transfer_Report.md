# P09 Modular Humanoid — 기존 툰 머티리얼 연결

## 적용 범위

- Unity 원본: `C:/UsingProject/UnityProject/UPlayground/Assets/ExternalAssets/Character/P09_Modular_Humanoid`.
- Unreal 결과: `/Game/Art/P09Modular`.
- 기존 `M_PGToonWorld_multi`를 부모로 사용한다. 원본 셰이더 GUID에서 불투명/컷아웃을 구분하고 BaseTexture, BaseTint, OpacityTexture, 알파 모드/임계값을 연결한다. 기존 툰 마스터와 HLSL은 수정하지 않는다.
- 피부·얼굴·헤어·의상·금속 프로필은 `Tools/Art/ToonTest/shading_profiles.json`의 값을 재사용한다. 텍스처 최대 크기는 원본의 기본 사용 설정에 맞춰 2048이다.
- 원본 FBX 3종에서 **193개 메시(스켈레탈 174, 스태틱 19)**를 분리했다. 몸/방어구는 같은 Skeleton 에셋을 공유한다. 몸 1·헤어 12·활 1, 총 14개 Skeleton을 사용한다.
- **머티리얼 인스턴스 103개, 텍스처 82개**를 생성했다. 선택하지 않은 원본 색상 변형도 포함하며, 데모 UI RenderTexture 머티리얼은 캐릭터 이전 대상에서 제외한다.
- 기본 남성/여성은 No Physics Variant의 부모 활성 상태와 제거된 GameObject를 해석해 각각 7개 파츠를 선택한다. Armor007 조합을 추가해 헤어/망토가 다른 외형도 확인한다.

## 결과물

- 테스트 맵: `/Game/Art/P09Modular/Maps/L_PG_P09Modular_Toon`.
- 프리셋: `/Game/Art/P09Modular/Blueprints/BP_PG_P09_Female`, `BP_PG_P09_Male`, `BP_PG_P09_Female_Armor007`, `BP_PG_P09_Male_Armor007`.
- 개별 파츠: `/Game/Art/P09Modular/Meshes`.
- 머티리얼/텍스처: `/Game/Art/P09Modular/Materials`, `/Game/Art/P09Modular/Textures`.
- 원본 이름, 재질 GUID, 파츠별 FBX 슬롯, 선택 카탈로그, 프리셋, 원본 해시: `Tools/Art/P09Modular/manifest.json`.

## 재실행 도구

1. Blender 5.2 백그라운드에서 `Tools/Art/P09Modular/PrepareP09Modular.py`를 실행한다. 원본을 읽고 개별 FBX와 명세를 생성한다. 사용하지 않는 FBX 슬롯을 제외하고, `.fbx.meta`의 `externalObjects` 이름→GUID 매핑으로 재질을 결정한다. Unity 프리팹 배열 순서와 FBX 슬롯 순서를 직접 대응시키지 않는다. 단일 슬롯 프리팹의 색상 오버라이드는 그대로 유지한다.
2. UE 5.8 내장 Python으로 `Tools/Validation/RunP09Modular.py --step configure`를 실행한다. 기존 임포트 메시를 재사용하고 파츠별 원본 재질 GUID로 툰 인스턴스를 연결한다.
3. 같은 도구의 `--step preview`로 프리셋/맵을 저장하고 SM6 화면 7개를 캡처한다.
4. `--step validate`로 새 프로세스에서 저장된 에셋, 머티리얼 부모, 텍스처, 슬롯, Skeleton 및 프리셋 구성을 재검사한다.

각 실행의 명령줄/로그는 `Saved/P09Modular/Runs`, 최신 결과 JSON과 이미지는 `Saved/P09Modular`에 저장한다. 셰이더 컴파일 작업 폴더도 이 경로를 사용한다. 열려 있는 다른 에디터는 종료하지 않는다. 화면 확인용이며 동시 프로세스가 없는 성능 측정 결과로 해석하지 않는다.

## 검증 및 후속 범위

- 임포트와 머티리얼 연결: PASS. 새 UE 프로세스의 저장 재검사에서 **193개 메시·259개 슬롯·103개 머티리얼·프리셋 4종**을 확인했다. 기본 프리셋은 각각 7개, Armor007은 망토를 포함해 각각 8개 컴포넌트다.
- SM6 렌더 7장(전체, 남/여 전신, Armor007 남/여, 얼굴 근접, 쿼터뷰)을 직접 확인했다. 피부/의상 재질 역전 문제를 FBX 메타데이터 매핑으로 수정한 뒤 재캡처했으며, 새 프로세스 검증에도 다중 슬롯의 FBX 이름→GUID 검사를 추가했다. 현재 캡처에서 누락/기본 체크무늬 재질, 크게 어긋난 파츠는 보이지 않는다.
- 최종 실행 근거: `Saved/P09Modular/Runs/20261004T140242005658Z`의 configure PASS, `20261004T140400259630Z`의 preview CAPTURED, 최신 `Saved/P09Modular/validate.json`의 PASS. 원본 FBX·FBX 메타·프리팹·머티리얼 98개 파일의 해시 불변을 확인했다.
- FBX·프리팹·머티리얼 원본 해시를 비교한다. 기존 플레이어, 전투 데이터, 공용 툰 마스터는 변경하지 않는다.
- 프리셋은 **기본 포즈의 외형/머티리얼 확인용**이다. 장비 UI, GAS, 애니메이션 리타게팅, Leader Pose 런타임 초기화, 헤어의 Head 부착 및 MagicaCloth 대체 물리는 포함하지 않는다.
- 고정 조명의 셀 명암 방향은 P09 인스턴스에 저장한다. 기존 `PGToonPresentationComponent`는 얼굴 컴포넌트의 머리/광원 파라미터를 동기화한다. 이동하는 광원을 모든 파츠에 동기화하려면 모듈러 캐릭터 런타임 통합이 추가로 필요하다.
- 원본의 노멀/러프니스/MatCap 등 lilToon 전체 기능을 복제하지 않는다. 현재 프로젝트 커스텀 마스터가 제공하는 표현을 사용한다. 추가 맵은 원본 폴더에 유지된다.
- FBX 임포터가 일부 바인드 포즈를 재생성한다. 본/스킨 및 Morph Target은 분리 FBX에 보존하지만 애니메이션 변형, 표정, 장비 조합 전체의 관통, LOD와 다수 캐릭터 성능은 후속 검증 대상이다.
- `Hair_10`은 Unity 프리팹에서는 SkinnedMeshRenderer지만 Blender FBX 임포트 결과에서 Armature 연결이 검출되지 않아 StaticMesh로 이전했다. 나머지 헤어 12종은 개별 리그를 유지한다. 이 헤어를 애니메이션 캐릭터에 채택하기 전 원본 스킨 연결을 별도로 확인해야 한다.
