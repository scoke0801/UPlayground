# Dark Knight + BossyEnemy 보스 제작 기록

2026-10-10. Unreal Engine 5.8. 신규 보스 **흑철의 처형자(15601)**를 `/Game/DataCenter/DarkKnightBoss/BP_15601`로 추가했다. 후속 요청으로 6스테이지 자동 스폰을 검성 15401에서 Dark Knight 15601 한 마리로 변경했다. 검성 에셋과 별도 조우 기능은 유지한다. 절차 생성 던전도 같은 스테이지 테이블을 사용한다.

## 모델과 실제 사용 모션

본체는 `ExternalAssets/Characters/Dark_Knight/Dark_Knight_Male/Meshes/SKM_DKM_Full`, 무기는 같은 폴더의 `SM_DKM_Sword`다. 검 일체형 메시에서는 검이 손을 따르지 않아 본체와 검을 분리했다. 기존 `PGEnemyPresentationData.Armor`로 오른손 `hand_r`에 부착하고 손바닥 그립 위치를 보정했다. 가드/회복의 부착 변환은 동일하다.

BossyEnemy의 UE4 마네킹 모션을 Dark Knight 리그에 IK 리타게팅했다. 대상마다 전용 시퀀스와 몽타주를 생성했으며 원본은 수정하지 않는다. 입력 시퀀스의 실제 프레임 수와 샘플링 레이트를 유지하고 루트 이동 및 외부 Notify를 제거했다. 피해 시점은 기존 공격 프로필의 논리 시계가 관리한다.

BossyEnemy 원본 루트는 `/Game/ExternalAssets/Animations/BossyEnemy/Animations/`다.

| 용도 | 원본 상대 경로 | 생성 시퀀스 |
|---|---|---|
| 대기 | Boss_Idle | AS_Idle |
| 걷기 | InPlace/Movement/Boss_Walk_F_InP | AS_Walk |
| 달리기 | InPlace/Movement/Boss_Run_F_InP | AS_Run |
| 횡베기 | InPlace/Attacks/Boss_Attack_Swing_InP | AS_Sweep |
| 올려베기 | InPlace/Attacks/Boss_Attack_Uppercut_InP | AS_Uppercut |
| 검격과 분쇄 | InPlace/Attacks/Boss_Attack_SwingAndSlam_InP | AS_Double |
| 파쇄 연격 | InPlace/Attacks/Boss_Attack_HandAndSwordSwing_InP | AS_Fury |
| 앞/뒤/좌/우 피격 | Boss_ReactionHit_F / B / L / R | AS_HitFront / Back / Left / Right |
| 사망 | Dark_Knight_Male/Animations/Anim_DKM_Death (모델 전용 원본) | AS_Death |

총 BossyEnemy 11개와 모델 전용 사망 1개를 연결했다. 각 `AS_`에 대응하는 `AM_` 몽타주가 있으며 이동은 `BS_Locomotion`의 대기 0 / 걷기 150 / 달리기 290으로 구성한다. 정확한 원본→생성본 경로는 [실제 사용 CSV](assets/MotionExpansion/dark-knight-use-manifest.csv), 접점은 [설정 JSON](../../Tools/Validation/Data/DarkKnightBoss.json)이 기준이다.

## 스킬과 AI

| ID | 공격 | 타격 시각(시작 기준) | 쿨다운 | 후딜 | 페이즈 |
|---|---|---|---|---|---|
| 15611 | 흑철 횡베기 | 1.10초 | 4초 | 1.3초 | 1부터 |
| 15612 | 처형 올려베기 | 1.30초 | 5초 | 1.6초 | 1부터 |
| 15613 | 검격과 분쇄 | 1.10 / 2.50초 | 7초 | 1.8초 | 1부터 |
| 15614 | 흑철 파쇄 연격 | 1.00 / 2.40초 | 8초 | 1.9초 | 2부터 |

기존 `PGRoleAIController`의 사거리·쿨다운·페이즈 조건, 직전 패턴 반복 억제, 공격 압박 예산을 사용한다. HP 50%에서 1.5초 전환 후 파쇄 연격을 개방한다. 2페이즈 시퀀스는 파쇄 연격 → 올려베기 → 검격과 분쇄 → 횡베기다. 조준 추적은 시작 0.25초 뒤 고정하며, 패턴 종료 뒤 최소 0.55초를 쉰다. 공격 후에는 피해 보너스 30%의 반격 기회를 준다.

공통 AI C++ 알고리즘을 새로 교체한 작업은 아니다. 보스 전용 패턴과 페이즈 데이터를 기존 실행기에 연결했다. 다른 몬스터의 Guard/Dodge/Counter 확장 순서는 [모션·AI 확장 기획](MotionExpansion_AI_Design.md)을 따른다.

## 실행과 재현

- `Tools/PlayDarkKnightBoss.ps1`: 격리 프로필로 RogueArena를 열고 보스 조우를 시작하는 런처.
- PIE 콘솔 `PGBossEncounter 15601`: 현재 세션의 6스테이지를 신규 보스로 바꾸어 실행한다. 보조 검증 런으로 표시하며 저장된 스테이지 테이블을 변경하지 않는다.
- UE 포함 Python으로 `Tools/Validation/RunDarkKnightBoss.py --step inspect|apply|validate|preview|runtime` 실행. 설정만 재적용할 때 `--reuse-motions`를 사용한다.
- 적용 시 실행 폴더에 테이블/생성 에셋 백업, 원본 해시, 적용 전 행 스냅샷을 남긴다. 실행 중인 에디터에서 백업을 복원하지 않는다.
- 자동 스폰 편성은 `--step roster`로 적용한다. `--step runtime --stage-roster`는 편성을 덮어쓰는 보스 조우 명령 없이 `PGStartStage 6`으로 실제 저장된 스테이지 스폰을 검증한다.

## 검증 근거와 범위

`Saved/DarkKnightBoss/auto-spawn-runtime01`: `saved_stage_roster=true`, 런타임 PASS 및 종료 코드 0. `PGBossEncounter`를 호출하지 않고 일반 `PGStartStage 6`으로 스폰·피해·페이즈·취소·사망 결과·재시작 및 자율 4패턴 선택을 검증했다. 던전 보스방 전체 이동 완주는 이 후속 변경에서 다시 실행하지 않았다.

후속 자동 스폰 연결: `Saved/DarkKnightBoss/auto-spawn02`에 스테이지 백업과 적용 전후 JSON을 저장했다. 6스테이지의 보스 ID 외 모든 필드가 동일하다. `Saved/QA/20261010T103101Z_4fa2cd5a`의 자동화·콘텐츠 검증은 PASS, 종합 PASS_WITH_WARNINGS다. 최초 `auto-spawn01`은 동시 던전 검증 프로세스의 파일 잠금으로 저장 실패했으며 해당 프로세스 종료 후 재적용했다. 아래 build05는 자동 스폰 연결 이전의 제작 검증 기록이다.

- `Saved/DarkKnightBoss/build05`: 최종 적용 및 별도 엔진 프로세스 재로드 PASS. 12개 대상 모션/몽타주, 4개 접점 프로필, 검 부착, 원본 해시, 기존 테이블 행 및 스테이지 편성 보존 확인.
- `Saved/DarkKnightBoss/preview04`: 실제 스폰 모델의 52개 샘플 프레임 캡처 PASS. 공격 4종의 자세 변화와 오른손 검 추종을 직접 확인했다. 앞선 검수에서 발견한 메시 회전 및 일체형 검 고정 문제는 수정했다. 캡처 자동 성공과 사람의 연속 플레이 품질 평가는 구분한다.
- `Saved/DarkKnightBoss/runtime01`: 실제 GAS 전투 PASS, 프로세스 종료 코드 0. 횡베기 160 / 올려베기 200 / 검격과 분쇄 296 / 파쇄 연격 288 피해를 확인했고, 조준 고정 후 측면 이탈은 0 피해였다. 일반 취소의 지연 타격 제거, 페이즈 전환 취소, 최소 재공격 대기, 사망 결과 UI 및 재시작을 통과했다. 36초 자율 AI 구간에서 15611~15614 모두 선택됐다. 체력을 보조 명령으로 높인 기능 검증이며 난이도 평가가 아니다.
- `Saved/QA/20261010T101300Z_c3c40346`: `quick --skip-build` 자동화와 전체 콘텐츠 검증 PASS, 종합 `PASS_WITH_WARNINGS`. GameplayCue 검색 경로 미지정, 테스트 월드의 장비/Actor 정리, 기존 이름표 직렬화 참조 및 스트리밍 설정 경고가 기록됐다. 이번 변경은 데이터/도구 작업이며 C++ 새 빌드와 패키징은 수행하지 않았다. 상호작용 런처 자체의 수동 실행은 별도 미검증이며 동일 조우 명령의 PIE 실행은 위 runtime에서 확인했다.

이번 검증은 보조 명령 기반이며 직접 조작 장시간 완주, 연속 영상의 모든 프레임, 오디오 청취 및 저사양 성능 검증을 포함하지 않는다. Humanoid 전체 팩의 모든 모델/모션이 검증되었다는 의미도 아니다. 이번에 선택한 Dark Knight와 BossyEnemy 조합에 한정한다.

## Art 사용 목록

`Art` 하위 전체 애니메이션 관련 17,376개는 [Art 사용 후보 목록](ArtAnimation_UsageCatalog.md)과 연결 CSV로 관리한다. BossyEnemy는 `ExternalAssets`이므로 Art 집계 밖에 있고, 위 표가 이번 보스의 실제 채택 목록이다.
