# 플레이어 공격 폴리싱·콤보 복구

2026-10-03 / Unreal Engine 5.8

## 콤보 버그

저장된 `GA_Skill_NormalAttack`과 스킬 슬롯 Ability 6개의 `BlockAbilitiesWithTag`에 자신의 `Player.Ability.Attack` 태그가 있었다. GAS는 재발동 처리 전에 태그 요구사항을 검사하므로, `bRetriggerInstancedAbility=true`여도 공격 중 다음 타격을 활성화할 수 없었다. 기존 `ComboSequence` 테스트는 순번 계산만 검사했고 실제 에셋의 태그 차단은 검사하지 않았다.

- 공격 차단 태그만 제거하고 장착·해제 차단은 보존한다. 기존 `CanStartSkill`이 몽타주 진행률과 취소 창을 검사한다.
- 기본 콤보는 `100 → 101 → 102 → 100`이다. 101행에 남아 있던 자기 자신을 포함하는 체인 목록도 비웠다.
- 콤보 유지 시간에 몽타주의 `RateScale`을 반영한다. 현재 기본 공격 몽타주의 에셋 배율은 1.5다.
- 공격 종료·취소 때 무기 충돌과 타격 대상 목록을 명시적으로 정리한다.
- 공격 중 게임 종료 시 컴포넌트 `EndPlay`에서 무기를 제거한 뒤 늦은 `NotifyEnd`가 호출되어 발생하던 `WeaponToToggle` assertion도 수정했다. 무기가 이미 없으면 타격 목록을 비우고 종료한다.

## 공격 구분

`DT_Skill`에 `PlayerAttackPlayRate`, `PlayerMeleeDamageMultiplier`, `bPlayerHeavyImpact`를 추가했다. 기본값은 기존 동작을 유지한다. 피해 배율은 확정 근접 이벤트 동안만 적용하므로, 나중에 적중하는 검기나 다른 스킬에 남지 않는다. 방어·치명타·강화·흡혈·실제 HP 변경은 기존 GAS 경로를 사용한다. 강타 표현은 기존 Heavy 피드백 프리셋을 사용하며, 기본 3타가 스킬 전용 빌드 효과를 발동시키지는 않는다.

| 공격 | 근접 타격 수 | 타격당 공격력 배율 | 추가 재생 배율 | 쿨다운 |
|---|---:|---:|---:|---:|
| 기본 1타 | 1 | 0.90 | 1.10 | 0초 |
| 기본 2타 | 1 | 1.00 | 1.18 | 0초 |
| 기본 3타 | 1 | 1.50·강타 표현 | 1.00 | 0초 |
| 도약 베기 110 | 1 | 1.80·강타 표현 | 1.10 | 4초 |
| 전진 연격 111 | 2 | 0.85 | 1.25 | 3초 |
| 회전 베기 112 | 2 | 0.80 | 1.20 | 5초 |
| 집중 연격 113 | 3 | 0.70 | 1.35 | 4초 |
| 검기 114 | 근접 1 + 기존 투사체 | 근접 1.00 | 1.10 | 3초 |

111~113의 몽타주에는 긴 단일 충돌 창을 분리한 타격 창을 저장했다. 각 창 안에서는 같은 적에게 한 번만, 다음 창에서는 다시 적중한다. 기존 모션·검기 투사체·VFX를 재사용한다. 비어 있던 궁극기 115의 새 모션 제작은 이번 범위에 포함하지 않았다.

## 재현·검증

- 수치 원본: `Tools/Validation/PlayerAttacks.json`.
- 읽기 전용 진단: `InspectPlayerAttacks.py`.
- 에셋 이관: `ConfigurePlayerAttacks.py`. 원본 백업은 `Saved/Backups/PlayerAttacks/20261003T121807894769Z`. 반복 적용 시 알림 클래스 기준으로 기존 창을 교체하며, 다른 스킬·적 데이터와 표현 알림을 보존한다.
- 저장 검사: `ValidatePlayerAttacks.py`는 기존 `ValidateRoguelikeMVP.py`에도 연결했다. 재로드한 8개 공격 프로필·7개 Ability·3개 다단히트 몽타주를 검사한다.
- 실제 실행 검사: `RunPlayerAttacks.py`. `PGPlayerAttackProbe`는 `PlayerAttacks_`로 시작하는 격리 테스트 프로필에서만 동작한다. 실제 플레이어·무기·Ability·몽타주를 사용해 유지 입력, 1→2→3→1, 해제, 대기 후 초기화, 스킬별 타격 창을 검사한다. 고정 표적에 접촉을 반복 주입해 창 안의 중복 적중 방지와 타격당 피해도 검사하며, 활성 충돌 창에서 GAS 취소 후 충돌 종료를 검사한다.
- 수정 전 재현: `Saved/Logs/PlayerAttackBefore.log`의 `FAIL active attack blocks the next combo ability`.
- 초기 수정 후 실행: `Saved/Logs/PlayerAttackAfter.log`. 콤보 시작 시점은 약 0.02·0.73·1.43·2.40초이며, 5개 스킬의 타격 창 검사가 통과했다.
- 일반 Editor Development 빌드 성공: `Saved/Logs/PlayerAttackBuildFinal.log`. 후속 진단 도구 빌드: `Saved/Logs/PlayerAttackProbeBuildFinal2.log`.
- PG 자동 테스트 36개 통과(32 성공, 4 경고 포함 성공): `Saved/Automation/PlayerAttacks/index.json`. 새 `PG.Combat.PlayerMeleeProfiles`는 방어를 거친 배율, 강타 상태 복원, 다음 일반 피해로의 배율 유출 방지를 확인한다.
- 추가 검증 과정에서는 고정 표적의 낙하, 시작 장비 공격력 가산치를 테스트 환경에서 분리했다. 다단 피해·취소 검사 후 종료 과정에서 위의 늦은 알림 assertion을 재현하여 런타임 코드를 수정했다. 초기 실패 기록은 `Saved/QA/*_player_attacks`에 보존한다.
- 최종 빌드: `Saved/Logs/PlayerAttackCleanupBuild.log` 성공. 최종 전체 자동 테스트도 36개 통과(32 성공, 4 경고 포함 성공, 실패·미실행 0): `Saved/Automation/PlayerAttacksFinal/index.json`.
- 최종 에셋·실행 검사는 `Saved/QA/20261003T122656Z_cbd9d3_player_attacks/report.json`의 **PASS**다. 공격력 100·방어 0·치명타/장비/강화 없는 격리 표적에서 110~114의 근접 피해는 각각 180·170·160·210·100으로 확인했다. 콤보 반복·해제·초기화·다단 판정·중복 방지·활성 타격 창의 취소·프로세스 정상 종료를 모두 통과했다.
- 작업 시작 전에 있던 AI 변경의 `PGBTService_SelectSkill.cpp`는 해당 헤더를 먼저 포함하도록 순서만 수정했다. 다른 기존 AI·아트 작업은 유지했다.

## 직접 플레이로 남은 항목

자동 실행은 NullRHI이며 키보드·마우스 하드웨어 입력 및 화면·소리를 평가하지 않는다. 실제 무기 궤적의 명중 범위, 다단히트 창과 칼날 접촉 순간의 세부 정렬, 밀집전 히트스톱·사운드, 새 쿨다운의 빌드별 처치 시간은 직접 플레이로 조정해야 한다. 접촉 주입 검사는 공간 판정 품질 검사를 대신하지 않는다.
