# 모션 확장 AI 개선 기록

2026-10-10. [확장 기획](MotionExpansion_AI_Design.md)의 P0 행동 선택 관측부터 적용한다.

## 적용 범위

`PGRoleAIController`에 선택 진단을 추가했다. 기본값은 꺼짐이며 콘솔에서 다음 순서로 사용한다.

```text
pg.AI.DebugEnemyID 15601
pg.AI.DebugDecisions 1
```

`DebugEnemyID 0`은 모든 적, `DebugDecisions 0`은 출력 중지다. Output Log 또는 실행 로그에서 `PGCombatDecision`을 검색한다. 동일 개체·사유·SkillID는 1초에 한 번까지만 기록하므로 이벤트 총횟수 측정에는 사용하지 않는다. Pawn 이름으로 같은 EnemyID의 개체를 구분한다.

| 기록 | 의미 |
|---|---|
| missing_skill / invalid_pattern / weight_disabled | 데이터 누락·패턴 오류·선택 비활성 |
| phase_required / guard_attack_requirement / summon_limit | 페이즈·반격 전 공격 횟수·소환 상한 미충족 |
| cooldown / out_of_range / no_line_of_sight | 쿨다운·사거리·시야로 후보 탈락 |
| selected_guard_priority / selected_phase_sequence / selected_weighted | 보스 반격 우선·페이즈 순서·가중 선택 |
| selection_retained / pressure_wait | 기존 선택 유지·공격권 예약 거절 후 대기 |
| started / activation_failed | GAS 공격 시작 성공·활성화 실패 |
| pattern_locked / phase_transition / recovery_locked | 진행 중 공격·페이즈 전환·다음 행동 제한 |
| invalid_combatant / missing_combat_data | 유효한 전투 대상이나 적 데이터·핸들러 없음 |
| approach_request_failed / retreat_request_failed / position_request_failed | 추적·후퇴·재배치 요청의 즉시 실패 |
| flight_floor_missing | 저공 이동의 바닥 탐사 실패 |

후보 탈락은 기존 필터 순서에서 처음 만난 사유를 기록한다. `pressure_wait`는 Director의 예약 거절을 뜻하며 비용 상한·FIFO·시작 간격의 세부 원인을 분리하지 않는다. 이동 요청 승인 이후의 경로 중단은 이번 범위에 포함하지 않는다.

각 로그에 적/스킬 ID, 대기 스킬, 페이즈, 타겟까지 2D 거리, 공격 비용, 기본 몽타주·표현 몽타주·공격 프로필 경로를 함께 남긴다. 프로필 경로는 접점별 모션을 찾기 위한 연결 정보이며 실제 재생 성공 증거는 아니다. 진단 때문에 추가 에셋을 동기 로드하거나 난수를 소비하지 않는다. 선택 가중치·반격 우선순위·공격권 정책·전투 수치는 유지한다.

## 검증

`Tools/Validation/RunCombatBT.py --debug-decisions`는 기존 6역할 자율 공격·회복·일시정지/복귀·타겟 이동·보스 페이즈 검사에 진단 출력을 켠다. `report.json`에 관측한 사유 목록을 남기고 공격 시작 진단이 없으면 실패한다. 기본 실행은 진단을 켜지 않는다.

- UE 5.8 `UPlaygroundEditor Win64 Development` 빌드 성공. 최종 소스의 추가 빌드에서 최신 상태를 확인했다. 기존 엔진 API 사용 중단·컴파일러 권장 버전·모듈 순환 참조 경고는 남아 있다.
- `RunCombatBT.py --debug-decisions` **PASS**, 프로세스 종료 코드 0. 근거: `Saved/QA/20261010T103838Z_7f496e0f_combat_bt/report.json`, `probe.log`.
- 15101~15106 전 역할의 자율 공격·회복, 일시정지/복귀, 타겟 이동, 보스 페이즈 전환을 통과했다. 재배치 요청 수는 역할별 3/2/0/2/2/3회다.
- 실제 로그에서 쿨다운·시야·거리·공격 잠금·페이즈 제한/전환·공격권 대기·페이즈 순서/가중 선택·선택 유지·공격 시작의 11개 사유를 확인했다.
- 소환 상한·반격 전제·잘못된 데이터·이동 요청 실패 등 나머지 분기의 강제 재현, Dark Knight 전용 실행, 직접 조작과 성능 수용 검사는 이번 검증에 포함하지 않았다. 기존 6역할 자동 검증을 신규 모션 채택이나 밸런스 수용으로 해석하지 않는다.
- 변경 파일 `git diff --check` 통과.

## 후보와 채택의 구분 및 다음 범위

Art의 17,376개 목록과 Dark Knight 사용 확정 manifest는 수정하지 않는다. 새 모션 채택이나 신규 스킬 구현 완료를 의미하지 않는다. 다음 수직 구현은 섬광 찌르기·그리핀 Attack_2의 연속 모션 검수, 접점·그립·피해 검증 후 진행한다. 상황별 선택 가중치와 반격 독점 억제는 진단 근거를 수집한 뒤 별도 적용한다.
