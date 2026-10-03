# 몬스터 AI·스킬 다양화

2026-10-03. 대상은 RogueArena의 일반 3종·정예 2종·보스다. 플레이어 스킬과 성장 데이터는 이번 변경 대상에 포함하지 않는다.

## 설계 근거

- [Blizzard, Diablo IV Quarterly Update — February 2020](https://news.blizzard.com/en-us/article/23308274/diablo-iv-quarterly-updatefebruary-2020): 몬스터 역할의 조합이 플레이어의 위치·대상·공격 선택을 바꾼다는 설명을 참고했다. 여기서는 사거리와 회피 방향이 다른 공격을 한 역할의 키트에 묶는다.
- [Santa Monica Studio, Evolving Combat in God of War, GDC 2019, 39–43쪽](https://media.gdcvault.com/gdc2019/presentations/Sheth_Mihir_EvolvingCombat.pdf): 적별 비용과 제한된 공격 토큰으로 동시 위협을 조절하는 방식을 참고했다. 이 프로젝트는 대상별 비용 상한과 경고 시작 간격을 사용한다. 대기 순서와 요청 만료를 추가해 먼저 생성된 적이 공격 기회를 독점하지 않도록 했다.
- [Blizzard, Diablo IV 2.5.0 PTR](https://news.blizzard.com/en-gb/article/24242857/the-2-5-0-ptr-what-you-need-to-know): 역할의 구별, 겹치는 공격 개성의 정리, 시각적 명확성이라는 방향을 참고했다. 해당 문서는 테스트 서버 제안이며 이 프로젝트의 수치 검증을 대신하지 않는다.

선택한 구현은 위 자료를 바탕으로 한 프로젝트 자체 설계다. 재미와 난이도는 직접 조작 검증이 필요하다.

## 추가한 공격

| 몬스터 | 추가 스킬 | 대응과 반격 기회 |
|---|---|---|
| 추격자 15101 | 15111 꿰뚫기 | 베기 사거리 밖에서 직선 예고. 조준 확정 후 옆으로 이동 |
| 사수 15102 | 15112 삼연 사격 | 세 발의 경로를 각각 예고. 경로 사이 이동, 같은 발사 묶음은 대상당 한 번만 적중 |
| 수호자 15103 | 15113 방패 찌르기 | 중거리 직선 공격. 측면 회피 후 방어가 풀리는 후딜에 반격 |
| 분쇄자 15104 | 15114 균열 고리, 15116 내려찍기 | 고리는 안쪽·바깥쪽이 안전. 근접에서는 내려찍기로 대응하고 멀면 기존 돌진 |
| 파수꾼 15105 | 15115 봉쇄 고리 | 기존 순차 장판과 달리 적 중심의 고리. 안쪽으로 진입하면 공격 기회 |
| 황혼의 기사 15106 | 15109 황혼의 일식 | 2페이즈 고리 공격. 1.4초 예고, 2초 후딜·받는 피해 보너스 60% |

공격은 기존 8개에서 15개가 된다. 전용 새 골격 애니메이션을 제작한 것은 아니며, 기존 몽타주·수호자 표현을 활용한다. 찌르기와 고리의 전용 자세·음향은 후속 폴리싱 대상이다.

## 실행 구조

- `PGSkillDataRow`에 최소 발동 거리, 선택 가중치, 공격 압박 비용, 고리 내부 안전 반경, 투사체 수·각도를 추가했다. 기존 패턴 enum 뒤에 `Thrust`, `RingBurst`를 추가해 기존 값의 순서를 보존했다.
- `PGRoleAIController`가 유효 거리·시야·쿨다운·페이즈로 후보를 추리고 가중치를 사용한다. 복수 후보가 있으면 직전 공격을 피한다. 공격 허가 대기 중에는 선택을 유지하되 범위가 무효가 되면 다시 선택한다.
- `PGCombatDirectorSubsystem`은 월드별·타겟별로 공격을 조절한다. 전체 액터 검색이나 매 프레임 틱을 쓰지 않는다. 기본 비용 상한 3, 일반 공격 1, 강한 공격 2, 보스 공격 3이다. 후딜까지 자리를 유지해 반격할 시간을 확보한다. 새 예고 시작 간격은 0.28초다. 대기 요청이 0.8초 갱신되지 않으면 만료된다.
- 기존 BT 실행 Task도 `RequestedSkillID`를 전달한다. 공통 공격·소환 Ability가 정확한 ID를 사용하며, 알 수 없는 ID나 쿨다운 중 요청을 다른 공격으로 대체하지 않는다. BT Task의 기존 즉시 완료 방식은 유지한다. 개별 Blueprint Ability의 독자적인 실행 그래프를 일괄 개편한 것은 아니다.
- 스킬 사용마다 우선순위를 계속 높이던 동작을 제거했다. 기존 BT는 데이터 가중치와 상황 가중치를 곱하며 소환 횟수 제한을 우회하지 않는다.
- 고리·찌르기·부채꼴 투사체의 예고와 판정에 같은 위치·길이·반경·각도를 사용한다. 다중 투사체는 발사 묶음의 적중 대상을 공유한다. 취소 시 묶음의 남은 투사체도 제거한다.

## 데이터와 재생성

- 원본: `Tools/Validation/CombatVariety.json`.
- 이관: `Tools/Validation/ConfigureCombatVariety.py`를 UE Python commandlet으로 실행한다. 대상 두 테이블과 전용 재질은 `Saved/Backups/CombatVariety/<실행 시각>`에 백업한다. 다른 적·스킬 행과 웨이브·드랍·성장 수치는 보존한다.
- 에셋: `/Game/DataCenter/CombatVariety/M_PGCombatVarietyBounds`, 기존 `DT_Enemy`, `DT_Skill`.
- 검증: `ValidateCombatVariety.py`가 기존 `ValidateContentMilestone.py`와 RunQA에 연결된다. 새 구성은 content schema 3이다. 콘텐츠 1·2단계 이관 뒤에 이 스크립트를 실행한다.
- 콘솔: `pg.AI.MaxAttackPressure 3`, `pg.AI.AttackStartSpacing 0.28`, `pg.Combat.EliteDebug 1`. 압박 상한을 낮춰도 무거운 공격이 영원히 대기하지 않도록 개별 비용을 상한 안으로 제한한다.

## 검증 기록

- UE 5.8 Development Editor 빌드 성공. `Saved/CombatVarietyBuild.log`, `Saved/CombatVarietyBuildFinal.log`.
- 실제 데이터 이관과 저장 후 재조회 성공. `Saved/QA/CombatVariety_Configure/migration.log`.
- 정확한 SkillID, 쿨다운 대체 금지, 동시 압박·시작 간격·대기 공정성, 고리 안전지대·찌르기 측면 회피, 다중 투사체 중복 피해·취소를 회귀 검사한다.
- 첫 자동 실행 실패 1건은 테스트용 미등록 SkillID를 슬롯에 채우지 않은 fixture 문제였다. 수정 후 30개 테스트가 통과했다. 최초 결과는 `Saved/QA/CombatVariety_Initial`에 보존한다.
- 전체 QA `Saved/QA/20261003T062536Z_db8e968d/report.json`: **PASS_WITH_WARNINGS**. 자동 테스트 30개, 에셋 검사, 16웨이브·7회 선택·6구간 진행, 사망·재시작 20회 통과. 바로 앞에서 에디터를 별도로 빌드한 뒤 `--skip-build`로 실행했다.
- 첫 렌더링 `Saved/QA/20261003T062944Z_f7b20c12_combat_variety/report.json`: 자동 관측 PASS. 신규 공격 7개의 예고·회복 14회, 혼합 전투 25초·1458프레임 관측, 다섯 역할 모두 공격·동시 공격 최대 2. 캡처에 에디터 UI가 포함돼 큰 고리 일부가 잘려 있어 게임 뷰포트 전용 캡처로 재검증한다.

- 최종 빌드 후 `Saved/QA/CombatVariety_Final/Automation/index.json`: **30개 통과, 실패 0**. 스킬 초기화 전 호출 방어를 검증했고, 투사체 테스트에 월드 컨텍스트를 등록해 DestroyActor 경고를 제거했다. 기존 테스트 환경의 시작 무기·GameplayCue 경고는 남아 있다.
- 최종 렌더링 `Saved/QA/20261003T063719Z_ff1f9116_combat_variety/report.json`: **PASS**. 고유 파일명으로 저장한 1280×720 게임 뷰포트 PNG 14장을 모두 시각 검토했다. 직선·세 갈래 사격 경로, 고리 내부 안전 영역, 청록색 회복 표시를 확인했다. 혼합 전투 25초·1457프레임에서 다섯 역할 모두 공격했고 동시 공격은 최대 2였다. 고리 외곽은 상대 위치에 따라 화면 밖으로 나가지만 관측 장면에서 안쪽 회피 경계는 보였다.

자동 처치 기반 진행 검사는 직접 조작·난이도·재미를 검증하지 않는다. 렌더링 검사는 격리 패턴 및 자동 AI 관측이며 실제 입력, 패키지 성능, 장시간 부하, 사운드 믹싱 판정을 포함하지 않는다. 전용 모션과 카메라 가장자리 가독성의 직접 플레이 폴리싱은 후속 범위다.
