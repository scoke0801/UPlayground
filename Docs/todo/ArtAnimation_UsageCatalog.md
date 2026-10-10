# Art 애니메이션 보유 목록과 사용 후보

2026-10-10 Unreal Engine 5.8 AssetRegistry 조회. `/Game/Art` 전체를 재귀 탐색해 AnimSequence, AnimMontage, BlendSpace, AnimBlueprint 등 실제 애니메이션 클래스를 수집했다. `Animation`, `Animations`, `AnimationTests` 및 다른 이름의 하위 폴더까지 포함한다.

**17,376개 애니메이션 관련 에셋**을 [전체 사용 후보 CSV](assets/MotionExpansion/art-animation-inventory.csv)에 기록했다. [집계 JSON](assets/MotionExpansion/art-animation-summary.json)과 `Saved/DarkKnightBoss/inspect01`의 엔진 실행 로그가 근거다. CSV는 모든 에셋의 정확한 패키지 경로, 클래스, 직접 참조 수와 참조자를 제공한다.

## 폴더별 보유량

| 위치 | 애니메이션 관련 에셋 | 활용 방향 |
|---|---:|---|
| ToonTest/Bokusei | 8,646 | Bokusei 대상 변환본. 다른 리그에 연결할 때는 원본에서 대상별 변환 |
| AnimationTests/RPGAnimations | 6,336 | 무기·마법·이동·반응 원본 후보 |
| AnimationTests/FrankSlash | 1,291 | 무기별 공격, 방향 이동, 회피·가드 후보 |
| AnimationTests/GrruzamSword | 959 | 카타나·대검 단발/연격·가드/반격 후보 |
| AnimationTests/AnimeKatana | 60 | 카타나 진입·찌르기·강타·도약 후보 |
| CreatureModels/Griffin | 26 | 시퀀스 17개와 몽타주·이동 에셋. 공격·비행·등장 후보 |
| CreatureModels/MainPlant | 15 | 시퀀스 9개와 몽타주. 공격·독액·소환·각성 후보 |
| CreatureModels/EnemyPlant | 17 | 시퀀스 10개와 몽타주·이동 에셋. 공격·등장·이동 후보 |
| CreatureModels/EnemyRoot | 11 | 시퀀스 6개와 몽타주. 솟아오름·공격·소멸 후보 |
| ToonTest/Inori | 7 | 대상 리타게팅 검수본 |
| ToonTest/BokuseiShadingComparison | 6 | 셰이딩 비교용 모션. 비교 맵 참조와 게임 사용을 구분 |
| PlayerCombatFX | 2 | AS_PGPlayerDash와 AM_PGPlayerDash, 플레이어 대시 연결 |
| **합계** | **17,376** | 원본·변환본·방향·루트 변형의 중복 의미를 포함 |

클래스별로 AnimSequence 17,348개, AnimMontage 26개, BlendSpace1D 2개다. 원본과 변환본을 합친 파일 수이므로 서로 다른 모션/스킬 수가 아니다. 같은 이름의 원본과 파생본을 임의 삭제하거나 양쪽을 신규 스킬로 중복 집계하지 않는다.

## 참조 상태를 읽는 방법

| CSV 상태 | 에셋 수 | 의미 |
|---|---:|---|
| referenced_outside_art | 24 | Art 바깥 패키지의 직접 참조 존재. DT·전투 BP일 수도, 테스트 맵일 수도 있음 |
| referenced_within_art | 258 | Art 내부에서 직접 참조. 몽타주·갤러리 연결 등을 포함 |
| no_registry_referencer_candidate | 17,094 | 조회한 레지스트리에서 직접 참조자를 찾지 못함. 미사용 검토 후보 |

위 분류는 **실제 전투 사용 확정이나 삭제 가능 판정이 아니다**. 코드의 문자열 로드, 동적 생성, 리타게팅 도구의 입력은 참조 그래프에 드러나지 않을 수 있다. 반대로 갤러리나 미진입 맵의 참조는 플레이 사용을 보장하지 않는다. 전투 연결은 스킬/적 ID와 실행 검증을 추가해 확정한다.

## 작업에 사용하는 목록

- **플레이어 확장**: AnimeKatana의 Run_Thrust/Run_Attack_V1, Grruzam 카타나의 Revenge_Guard 세트, Frank 방향 회피를 [확장 기획](MotionExpansion_AI_Design.md)의 스킬 후보로 유지한다. 해당 대상에서의 접점·그립 검증 전에는 채택 확정으로 올리지 않는다.
- **몬스터 확장**: CreatureModels의 현재 전투 연결 23개와 미연결 후보 19개를 [기존 대조 CSV](assets/MotionExpansion/inventory.csv)에서 확인한다. 생성 코드의 연결 기준이며 전체 참조 분류와 목적이 다르다.
- **기존 인간형 보스**: Art/AnimationTests의 AnimeKatana·GrruzamSword·FrankSlash 원본을 사용한 검성 15401은 기존 적용 기록을 따른다.
- **신규 Dark Knight 보스**: 사용자가 지정한 `ExternalAssets/Animations/BossyEnemy`를 별도 전용 원본으로 사용한다. 이 팩은 Art 집계 바깥에 있으며 `/Game/DataCenter/DarkKnightBoss`에 대상 변환본을 생성한다. [보스 제작 기록](DarkKnightBoss_Implementation_Report.md)의 실제 연결 목록과 구분한다.

Art의 전체 개별 모션 설명은 기존 [팩별 상세 카탈로그](../Design/Animation/Bokusei_Motion_Catalog.md)를 함께 사용한다. 새 CSV는 현재 엔진 클래스와 참조 상태를 보완하며 과거 문서의 재생 검수 상태를 자동 승격하지 않는다.

## 재현

`Tools/Validation/RunDarkKnightBoss.py --step inspect`를 UE 5.8 포함 Python으로 실행한다. 검사기는 원본 게임 에셋을 수정하지 않으며 후보 CSV/집계와 실행별 `inspection.json`을 갱신한다. 새 검사 후 수량이 바뀌면 이 표도 같이 갱신한다.
