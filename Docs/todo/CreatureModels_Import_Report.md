# 그리핀·식충식물 에셋 이전 기록

후속 전투 통합: 몬스터 15501~15504와 공중형·고정형·지상형 AI, 본체 소환 스킬 및 기존 웨이브 연결은 [전투 통합 기록](CreatureCombat_Implementation_Report.md)을 따른다. 전투용 BP는 `/Game/DataCenter/CreatureCombat`에 있으며 아래 갤러리용 BP와 구분한다.

## 범위와 위치

Unity 원본의 Griffin Character와 Carnivorous plant character 두 팩을 Unreal Engine 5.8의 /Game/Art/CreatureModels에 이전했다. 원본 파일 90개의 SHA-256을 기록하고 원본 불변을 확인했다. 게임 몬스터 테이블·웨이브·기존 캐릭터는 수정하지 않는다.

| 모델 | 애니메이션 | 색상별 배치 BP | 스켈레톤 본 |
|---|---:|---:|---:|
| Griffin | 17 | Brown, Dark | 97 |
| MainPlant | 9 | V1, V2, V3 | 40 |
| EnemyPlant | 10 | V1, V2, V3 | 45 |
| EnemyRoot | 6 | V1, V2, V3 | 7 |

각 모델 폴더에는 SK_PG_<모델>, 전용 Skeleton·PhysicsAsset, Animations, Montages, Blueprints가 있다. 총 4메시·42 AnimSequence·25 AnimMontage·11 BP다. Griffin과 EnemyPlant에는 BS_PG_<모델>_Ground도 있다. 재질은 공용 마스터 1개와 색상 인스턴스 8개, 사용 텍스처 17개다.

## 사용

- 콘텐츠 브라우저에서 /Game/Art/CreatureModels/Maps/L_PG_CreatureModels를 열면 11가지 외형을 확인할 수 있다.
- 각 모델의 Blueprints/BP_PG_<모델>_<색상>을 레벨에 배치하면 원본 크기와 재질이 적용된 SkeletalMeshActor로 사용할 수 있다. 기본 대기 애니메이션은 반복 재생한다. 배치 BP의 충돌은 꺼져 있다.
- 실제 몬스터 구현에서는 해당 SK와 전용 Skeleton의 모션을 연결한다. BP는 외형 배치용이며 GAS 스킬·AI·비행 이동·피해 판정·스폰 등록을 구현한 전투 몬스터는 아니다.
- 이동 BlendSpace의 기본 속도 축은 그리핀 0/300/600cm/s, 이동형 식물 0/150/300cm/s다. 게임 이동 속도에 맞춘 발 미끄러짐 조정은 통합할 때 수행한다.
- 공격·피격·사망 등 비반복 동작은 Montages 폴더에 제공한다. 몽타주에 피해 Notify는 추가하지 않았다. EnemyRoot의 소멸 동작은 원본 이름 Exit다.

## 변환 규칙

- FBX의 원본 30fps Take 42개를 각각 분리한다. Unity 메타의 이름·구간·반복 여부는 Tools/Art/CreatureModels/manifest.json에 기록한다.
- 엔진 다중 Take 임포트가 마지막 결과만 반환하므로 생성된 AnimSequence 전체를 다시 조회하여 저장한다. Cast | Agr은 에셋 이름에서 Cast_Agr로 정규화한다.
- cm 단위·원본 리그를 보존한다. 별도 휴머노이드 리타게팅은 없다. 그리핀의 Zero/Non_Zero 비행과 이륙·착륙 높이 변화는 보존하며 Root Motion 추출·강제 Root Lock은 끈다.
- 프리팹 GUID로 모델별 재질을 대조한다. EnemyRoot는 EnemyPlant가 아니라 MainPlant의 재질 3종을 공유한다. 활성 Unity 재질의 BaseMap, BumpMap, MetallicGlossMap, EmissionMap과 EmissionColor를 사용한다. Metallic=R, Roughness=1-(A×Smoothness), Smoothness 원본값 0.5다. 사용하지 않는 AO 채널을 임의로 연결하지 않는다.
- 노멀은 Unreal에 맞게 Green을 반전하고, 마스크는 선형 Masks 샘플러로 읽는다. 텍스처의 원본 해상도는 유지하되 런타임 최대 해상도는 2048로 제한한다.
- URP 셰이더·Unity Animator·스크립트·연기 Particle은 Unreal 런타임으로 복사하지 않는다. 전용 PBR 재질을 새로 작성했다. 그리핀의 비활성 커스텀 셰이더 효과는 추가하지 않는다.

## 재현

엔진 내 Python 또는 일반 Python으로 다음을 실행한다.

~~~powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Art/CreatureModels/RunCreatureModels.py --step all
~~~

단계는 import/configure/validate/preview다. all은 순서대로 실행한다. Source 파일이 없으면 manifest의 Unity 원본 경로에서 해시가 일치하는 파일만 복사한다. 변경 단계 실행 전 기존 Content/Art/CreatureModels를 Saved/CreatureModels/Runs/<시간>/Backup에 백업한다. 실패 시 자동 원복은 하지 않으며 해당 백업으로 복원할 수 있다. 원본 FBX/TGA와 생성 Content는 기존 Git 제외 규칙을 유지하고 Python 도구·manifest만 추적하도록 예외를 추가했다.

## 검증과 한계

- 저장 후 새 UE 프로세스에서 4메시·11 BP 참조, 42클립의 길이·스켈레톤 일치·5시점 전체 본의 유한 변환과 움직임, 25몽타주와 2개 이동 BlendSpace 보간을 검사했다.
- 원본 FBX에는 사용되지 않는 Geometry와 bind pose 경고가 있다. 임포트는 성공했고, 이후 본 평가와 외형 렌더 검증을 별도로 수행했다.
- 엔진/네이티브 코드는 변경하지 않아 C++ 빌드나 전체 전투 회귀는 실행하지 않았다. 전투 통합·비행 AI·래그돌 튜닝·LOD 제작·밀집 전투 성능 검사는 이 에셋 이전의 범위 밖이다.

- 최종 configure5/validate5/preview6 프로세스가 모두 종료 코드 0으로 완료됐다. 재질 샘플러·거칠기 노드·SkeletalMesh 사용 플래그와 프리팹별 BP 재질 연결을 재로드 검사했다.
- 1280×720 렌더 16장(11색상+4공격+그리핀 비행)을 생성하고 색상·형태·공격 포즈를 확인했다. 네 모델의 대기/공격 이미지와 실제 컴포넌트 본 포즈가 각각 다르다. 최종 렌더 로그에 재질 컴파일 오류가 없다.
- Fly_Zero는 원본의 비행 기준 높이를 유지하므로 지면에 배치한 비행 컷에서 하체가 바닥에 가려진다. 실제 비행 액터는 고도를 올리거나 Non_Zero 클립을 선택해야 한다. 비행 이동 로직 검증을 뜻하지 않는다.
- 근거: Saved/CreatureModels/final.json, configure.json, validate.json, preview.json과 각 PNG. 원본 불변 검사는 source_check.json이다. 셰이더 워커의 임시 경로는 Saved/CreatureModels/Shaders로 분리했다.
