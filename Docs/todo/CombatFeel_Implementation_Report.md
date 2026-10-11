# 타격감 1차 보완 — 적중 반동

## 2026-10-11 모션 피드백 보완

- `PGPlayerAttackComponent::Advance`가 확정 적중으로 히트스톱을 요청한 뒤에도 같은 프레임의 나머지 논리 시간을 진행하던 문제를 수정했다. 같은 시각의 대상/페이즈 처리를 마친 후 접촉 시각의 포즈를 샘플링한다. 정지 중 직접 Advance 호출도 차단하며 마지막 접점이 종료 시각에 있어도 정지부터 마친다.
- 적의 반동은 실제 남은 히트스톱 동안 최고 변위를 유지한 뒤 복귀한다. 공격자/피격자 정지 시간이 서로 달라도 피격자 타이머를 기준으로 한다. 추가 누적 정지나 AI 경직은 만들지 않는다.
- `FPGImpactFeedback.RecoilAngle`로 피격 방향을 따르는 짧은 메시 기울임을 추가했다. Normal 3도 / Heavy 6도 / Critical 4도가 기본값이며 기존 Intensity를 적용한 최종 각도는 10도 이내다. 외형 표시 메시와 자식 장비/모듈러 부위에 함께 적용하고 캡슐·AI 방향·공격 상태는 유지한다.
- `RecoilDistance=0`은 변위, `RecoilAngle=0`은 기울임을 각각 끈다. `RecoilDuration`은 정지 이후 복귀 시간이다. 반복 적중은 이전 변위/회전을 제거하고 새 반응으로 교체하며 Reset·사망·EndPlay에서 복구한다. 기존 에셋의 새 필드는 C++ 기본값을 사용하므로 원본 모션이나 에셋의 일괄 재저장은 필요하지 않다.
- 피해량·공격 속도 데이터·공격/스킬 간 캔슬 금지·대시 캔슬 정책은 그대로다. 이번 범위는 접점 정지와 피격 실루엣이며 후딜 리타이밍, 신규 피격 클립, 처형/카메라 연출은 포함하지 않는다.

### 검증 및 재현

- UE 5.8 Development 에디터 빌드 통과: `Saved/Logs/MotionFeedbackBuildFinal.log`. 기존 도구체인·엔진 API·모듈 순환 참조 경고는 남아 있다.
- 관련 자동 검사 17개 통과(경고 없는 성공 13개, 경고 포함 성공 4개, 실패/미실행 0): `Saved/QA/MotionFeedback/AutomationFinal/index.json`. 접점에서 긴 프레임 중단, 다음 타격 지연, 다중 대상 동일 페이즈 피해와 단일 정지, 취소 복원, 반동 유지/복구, 100회 반복, 부모 회전·스케일, 잘못된 각도, 사망 정리를 검사한다.
- 첫 실행은 추가 검사에서 소비한 체력을 기존 검사 전에 복구하지 않아 후속 기대값 3건이 실패했다. 테스트 준비를 `RestoreHealth`로 수정해 재검사했으며 실패 기록은 `Saved/QA/MotionFeedback/Automation`에 보존했다.
- `RunHackSlashMotion.py --label feedback --capture`: 기본 100/101/102와 111/112의 실제 GAS·공간 판정 실행, 본 포즈 368프레임 및 정지 이미지 5장 기록, 프로세스 종료 코드 0. `Saved/QA/20261011T070901Z_c57301_attack_motion_feedback/report.json`은 `RECORDED`다. 이 검사의 기본 대상은 비표시 캡슐이므로 적 외형 검수 근거로 사용하지 않는다.
- `RunHackSlashMotion.py --label feedback_p09 --capture --feedback-enemy 15701`로 실제 데이터 행의 인간형 적을 AI 없이 생성하고 적중 정지 자세를 촬영할 수 있도록 기존 도구를 확장했다. 별도 테스트 프로필만 사용한다.
- 인간형 표시 검수는 `Saved/QA/20261011T071345Z_924120_attack_motion_feedback_p09/report.json`에 `RECORDED`, 오류 없음/종료 코드 0으로 기록됐다. 5개 공격의 본 포즈 415프레임과 적중 화면 5장을 생성했고, 기본 1타/3타의 인간형 몸체·장비 표시를 확인했다. 정지 화면으로 연속 동작의 체감 품질을 확정하지 않는다. 검수 도구를 포함한 최종 빌드는 `Saved/Logs/MotionFeedbackProbeBuild.log`에 성공으로 기록됐다.
- 직접 조작의 재미 평가, 연속 영상·청취, 모든 몬스터 외형별 기울임 튜닝, 패키지 성능 인증은 별도다. 기존 열린 에디터는 자동 재시작하지 않으므로 새 Development 프로세스에서 확인해야 한다.

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
