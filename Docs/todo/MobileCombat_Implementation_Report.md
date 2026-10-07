# 이동 공격 · 대시 연결 — 2026-10-06

## 적용 범위

기본 100/101/102, 원월참 112, 관통검기 114에서 WASD 이동을 유지한다. 피해량·실제 모션 접점·포즈 시간·쿨다운·공격/회피 취소 시각은 유지했다.

| 스킬 | 이동 | 모션 슬롯 |
|---|---|---|
| 기본 3타 | 시작부터 종료까지 기존 이동 속도 100%, 강제 전진 제거 | UpperBody |
| 원월참 | 시작부터 종료까지 85% | 기존 FullBody 회전 |
| 관통검기 | 시작부터 종료까지 90% | UpperBody |

- `PGPlayerAttackComponent`는 시작 프레임부터 현재 Walk 구간의 속도를 적용한다. 이동 공격의 시작·콤보 전환·종료/취소에서 속도 벡터를 지우지 않는다. 히트스톱은 공격 논리 시계·모션을 정지시키며 Walk 입력 이동은 유지한다. 스킬이 강제 이동하는 구간은 기존 경로를 사용한다.
- 기존 `ABP_LocalPlayer`의 `UpperBody` 슬롯과 `Spine_01`부터의 레이어를 사용한다. 기본 3타·관통검기 몽타주의 슬롯을 변경하며 원본 애니메이션 시퀀스와 AnimBP 그래프는 수정하지 않는다. 정지 시에는 기존 이동 그래프의 하체 대기 포즈를 사용한다.
- `PGPlayerAnimInstance`의 이동 방향은 카메라 기준 WASD 축 대신 캐릭터를 기준으로 한 실제 속도로 계산한다. 공격 방향과 이동 방향이 다를 때 하체 이동·방향 보정에 사용할 값을 제공한다.
- 대시 중 입력 예약은 기본 0.45초이며 일반 공격 중의 0.18초 버퍼는 유지한다. 대시 종료의 RootMotionSource 속도 처리는 보행 최대 속도 제한으로 변경했다. 이동 입력이 없거나 취소되면 즉시 정지한다. 적 통과·벽 충돌·사망/취소 정리는 기존 경로를 사용한다.

## 데이터·재현

- 원본: `Tools/Validation/HackSlashP0.json`, `HackSlashP1.json`. `movement_mode`, `walk_speed_ratio`, `upper_body` 값과 관통검기의 Walk 구간을 생성 도구에도 반영했다.
- `ConfigureMobileCombat.py`는 대상 프로필 5개와 몽타주를 먼저 백업하고 이동 구간 및 대상 슬롯만 저장한다. 저장 전후 타격·포즈·시계·취소·투사체 수치의 일치를 검사한다. `-PGMobileCombatValidate`는 저장된 에셋을 읽어 확인한다.
- 백업: `Saved/Backups/MobileCombat/20261006T140716326744Z`. 실제 변경 에셋은 프로필 5개와 몽타주 4개다. 대상 테이블 및 비대상 스킬 에셋을 저장하지 않는다.
- `RunMobileCombat.py`는 새 프로세스 재로드 → PG 자동 테스트 → 기존 8종 공간 판정 → 유지 입력 콤보 → 외형별 이동 공격 순서로 검사한다. `PGMobileCombatProbe`는 `MobileCombat_` 격리 프로필에서만 실행한다.
- `Tools/PlayMobileCombat.ps1`은 Development 빌드로 일반 RogueArena 플레이 창을 연다. `-Configuration DebugGame`도 지원하며 테스트 플래그·테스트 프로필을 사용하지 않는다. 이전 바이너리로 실행 중인 에디터에는 재시작 후 네이티브 변경이 반영된다.

## 검증 결과

- UE 5.8 `UPlaygroundEditor Win64 DebugGame` 및 `Development` 빌드 PASS. 로그: `Saved/Logs/BuildMobileCombatComplete.log`, `Saved/Logs/BuildMobileCombatDevelopment.log`. 기존 도구체인·엔진 API·모듈 순환 참조 경고는 유지된다.
- 최종 이동 공격 QA: `Saved/QA/20261006T142014Z_dbd0d5_mobile_combat/report.json`, **PASS**. PG 50개(성공 41, 경고 포함 성공 9, 실패/미실행 0), 8종 공격 공간 판정, 기본 100→101→102→100 콤보가 통과했다.
- 일반 플레이용 Development에서도 Bokusei의 5종 이동 공격·히트스톱·대시 연결·벽 충돌 검증 PASS. 별도 실행 근거: `Saved/QA/20261006T143344Z_a157f8_mobile_combat/report.json`.
- Bokusei·Hwarin·Yura(저장 ID `Hichi`)의 5종 이동 공격, 총 15회에서 기본 3타 최소 속도 600cm/s, 원월참 510cm/s, 관통검기 540cm/s를 확인했다. 각 공격 중 2프레임 히트스톱 분기와 로컬 허벅지 포즈 변화, 종료 속도 유지도 검사했다. 이동은 Enhanced Input의 실제 Move 액션을 주입한다.
- 각 외형에서 대시 시작 직후 예약한 관통검기가 0.367초 뒤 보행 속도를 유지하며 시작했다. 원월참 이동 중 벽 앞에서 135.26cm 진행 후 막히고 취소 시 속도 제한을 복원했다.
- 별도 최종 대시 QA: `Saved/QA/20261006T142027Z_1bdb6d_player_dash/report.json`, **PASS**. 정지 입력의 개방 공간 458.33cm, 벽 앞 167.48cm, 취소 후 이동 누출 없음, 적 통과·충돌 복구·잔상 정리를 확인했다.
- Python의 기존 HackSlash 기록 검사 23개와 변경 Python 문법 검사, 플레이 실행 PowerShell 문법 검사 통과. 이동 QA에서 1280×720 캡처 15장을 생성했고 세 외형의 대표 기본 공격·관통검기·원월참 화면을 확인했다.

자동 입력·포즈 및 정지 화면 검증이다. 직접 키보드/마우스로 느끼는 손맛, 연속 모션·타격음, 세 이동 속도 비율의 최종 밸런스와 장시간 밀집 전투 성능은 별도 플레이 검수 대상이다. 현재 조준 고정과 스킬 취소 시각은 기존 정책이며, 다른 액티브의 이동 공격 확장은 이번 범위에 포함하지 않는다.
