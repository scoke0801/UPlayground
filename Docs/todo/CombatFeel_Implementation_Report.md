# 타격감 1차 보완 — 적중 반동

## 확인한 문제

- `PGCharacterBase::PlayCombatFeedback`에는 히트스톱·VFX·SFX·카메라 흔들림이 이미 있다. 기능의 부재와 실제 체감 품질은 구분한다.
- `PGAbilityHitReact`는 다른 몽타주가 재생 중이면 피격 모션과 재질 처리 전에 종료한다. 몽타주가 없는 분기에서는 재질 스위치를 켠 직후 종료하면서 다시 끈다.
- 기존 재질 스위치는 원본 메시만 변경한다. `PGCharacterAppearanceComponent`가 만드는 표시 메시·모듈러 부위와 새 툰 재질에는 별도 연결이 필요하다. 이번 변경에서 피격 플래시는 수정하지 않았다.

## 변경

- 확정 피해의 기존 피드백 경로에서 `PGEnemyPresentationComponent`의 짧은 방향성 반동을 실행한다. 공격 몽타주나 애니메이션 히트스톱 중에도 표시된다.
- 교체 외형이 있으면 표시 메시, 없으면 기존 캐릭터 메시를 움직인다. 충돌 캡슐·피해량·AI 공격 상태는 변경하지 않는다. 실제 밀쳐내기나 경직 기능은 아니다.
- `DA_PGCombatFeedback`의 Normal / Heavy / Critical에 `RecoilDistance`와 `RecoilDuration`을 추가했다. 기본 6cm / 0.14초이며 거리는 기존 Intensity와 FeedbackIntensity를 곱한다. 거리 0으로 끈다. 기존 저장 에셋은 새 필드의 C++ 기본값을 사용한다.
- 같은 대상의 기존 피드백 간격 제한을 따른다. 이전 반동을 제거한 뒤 새 반동을 적용하므로 위치가 누적되지 않는다. 부모 스케일을 보정하고, 타이머는 반동 중에만 작동한다.
- 사망·Reset·EndPlay에서 오프셋과 타이머를 정리한다. 전용 방어 표현 에셋이 없는 적도 사망 후 반동을 재개하지 않는다.

## 검증

- `PG.CombatCycle.EnemyHitRecoil`: 공격 중·히트스톱 중 반동, 100회 중복 요청, 캡슐/공격 상태 보존, 시간 경과 복원, 비활성 수치, 부모 스케일 보정, 방어 데이터 없는 적의 사망 및 늦은 호출을 검사한다.
- UE 5.8 `UPlaygroundEditor Win64 Development` 빌드 통과. 로그: `Saved/Logs/HitRecoilBuild.log`. 기존 엔진 API deprecation·도구체인·모듈 순환 참조 경고가 있다.
- `PG.Combat+PG.Content+PG.HackSlash+PG.Animation` 28개 통과(21개 성공, 7개 경고 포함 성공, 실패/미실행 0). 보고서: `Saved/QA/HitRecoil/FocusedAutomation/index.json`. 새 `EnemyHitRecoil` 테스트는 경고 없이 통과했다. 경고는 테스트용 무기 초기화, GameplayCue 검색 경로, 월드 문맥 제거 후 액터 정리에 관한 것이다.
- 전체 `PG.` 실행은 작업 트리에 있던 `PG.Consumables.HealingAndLifecycle`의 `PGConsumableTests.cpp:51`에서 잘못된 월드 문맥 assertion으로 중단됐다. 로그: `Saved/Logs/HitRecoilAutomation.log`. 해당 테스트 코드는 수정하지 않았으며 전체 회귀 통과로 보지 않는다.
- 현재 열린 DebugGame 에디터는 기존 바이너리를 사용한다. 변경 동작은 빌드한 Development 구성으로 새로 실행하거나, 에디터 종료 후 DebugGame을 빌드해 확인해야 한다. 기존 세션은 종료하지 않았다.
- 렌더링·직접 입력·청취 검수와 P09 외형별 시각 비교는 미완료다. 자동 테스트는 상태·타이머·변위의 기능 검증이며 타격감의 주관적 수용 검사를 대신하지 않는다.

## 남은 폴리싱

- 실제 쿼터뷰에서 P09·기존 적·보스의 반동 강도와 연속 공격 체감 비교.
- 표시 외형 전체에 적용되는 짧은 피격 플래시. 기존 피격 몽타주 수명에 종속시키지 않는 경로가 필요하다.
- 타격음 청취 및 군중 적중 시 중첩 확인, 일반타·마무리타·치명타의 음색과 피드백 강약 조정.
- 타격 시점·검기·사운드의 연속 모션 검수. 이번 변경만으로 타격감 폴리싱 전체 완료를 주장하지 않는다.
