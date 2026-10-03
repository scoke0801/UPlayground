# 스테이지 이동·커서 조준·기본 무기 장착

## 2026-10-03 기본 전투 조작 개선

- 기본 공격은 Enhanced Input의 Started로 첫 타를 요청하고 Triggered로 누르는 동안 다음 공격 가능 시점을 확인한다. `PGAbilitySystemComponent.bRepeatNormalAttackWhileHeld`로 켜고 끈다. 반복 입력은 버퍼를 만들거나 갱신하지 않으며, 이미 예약된 스킬·회피를 우선한다. 버튼을 놓으면 진행 중인 공격은 유지하고 추가 반복을 멈춘다. 짧게 눌러 예약한 1회 입력은 기존 버퍼 규칙을 따른다.
- 입력을 지우는 기존 UI·포커스·사망·프로필 변경 경로는 누름 상태도 지운다. UI 위에서 공격을 시작하거나 커서를 UI로 옮기면 다시 눌러야 연속 공격이 재개된다.
- 입력을 누르는 것만으로 캐릭터를 회전시키지 않는다. 공격은 실제 발동 시 현재 커서 조준을 갱신한다. 회피는 발동 시 현재 이동 입력 방향을 사용하고, 이동 입력이 없으면 커서 방향을 사용한다. `PGQuarterViewData.bDodgeFollowsMovement=false`로 커서 전용 회피를 선택할 수 있다. 이동 Completed/Canceled는 방향을 초기화한다.
- 콤보는 같은 슬롯·같은 기본 스킬에만 이어진다. 현재 몽타주의 남은 재생 시간(재생 속도 반영)에 `DT_Skill.ComboResetSeconds`를 더해 유지하며 기본 추가 시간은 0.4초다. 그 시간이 지나거나 다른 기술을 확정하면 첫 타로 돌아간다. 월드 시간을 사용하므로 일시정지 중에는 만료되지 않는다. 스킬 사용 메시지는 실제 사용한 연계 타격 ID를 전달한다.
- 자동 테스트: `PG.Combat.InputBuffer`에 반복·해제·회피 우선·반복 비활성화를 추가하고 `PG.Combat.ComboSequence`, `PG.Combat.DodgeDirection`을 추가했다.
- `IA_Skill_Normal`의 Pressed/Released 트리거 제거와 저장을 완료했다. `ConfigureCombatFeel.py`는 원본을 `Saved/Backups/CombatFeel`에 보존한 뒤 이 액션만 변경한다. `ValidateCombatFeel.py`는 액션·매핑 트리거와 네이티브 회피 연결을 검사하며 기존 RunQA 에셋 검사에도 포함된다. 회피 모션 워핑은 캐릭터의 `GetDodgeDirection()`을 사용해 자세와 도착점을 같은 방향으로 계산한다.
- 이번 범위는 조작 응답성이다. 공격별 피해 차이, 적의 경직/공격 중단 규칙, 처치 시간, 타격음과 이펙트의 체감은 직접 플레이 비교가 필요하다. 자동 테스트 통과를 전투 재미 검증으로 간주하지 않는다.

### 초기 검증 및 잠금 이력

- 최초 일반 빌드 성공: `Saved/Logs/CombatFeelBuild.log`. 1차 RunQA는 자동 테스트 28개와 기존 에셋 검사 통과: `Saved/QA/20261003T042204Z_5be57bc6/report.md`. 이후 회피 워핑 연결을 추가했다.
- 당시 최종 소스의 일반 빌드는 컴파일 후 실행 중 에디터의 DLL 잠금으로 LNK1104가 발생했다: `Saved/Logs/CombatFeelBuildFinalLocal.log`. 에디터를 종료하지 않고 `ModuleWithSuffix=...,10349`로 관련 5개 모듈을 별도 연결해 성공했다: `Saved/Logs/CombatFeelBuildHotReload.log`. 당시 `Binaries/Win64/UnrealEditor.modules`는 해당 DLL들을 가리켰다.
- 최종 별도 프로세스 자동 테스트 28개 통과(24 성공, 4 경고 포함 성공, 실패/미실행 0): `Saved/Automation/CombatFeelFinal_10349/index.json`, `Saved/Logs/CombatFeelAutomationFinal.log`. 경고는 기존 GameplayCue 경로 및 격리 테스트 월드의 시작 무기 설정이다.
- 최초 입력 에셋 저장 실패 근거: `Saved/Logs/CombatFeelConfigure.log`. 아래 후속 적용에서 잠금 해제 후 저장과 재검증을 완료했다.

### 2026-10-03 후속: 입력 에셋 적용 및 일반 빌드 복구

- 실행 중인 에디터가 없는 상태에서 `Tools/Validation/ConfigureCombatFeel.py`를 실행했다. 입력 액션만 저장했으며 액션·매핑 트리거와 네이티브 회피 연결 검사에 통과했다: `Saved/Logs/CombatFeelApply.log` (`PGCombatFeel VALIDATION PASS mappings=1`).
- 적용 직전 원본: `Saved/Backups/CombatFeel/20261003T052527132981Z/Blueprints/Input/Actions/IA_Skill_Normal.uasset`.
- `RunQA.ps1 -Suite quick`의 일반 Editor Win64 Development 빌드가 성공했다. 관련 5개 모듈을 정상 DLL 이름으로 연결했고 `Binaries/Win64/UnrealEditor.modules`도 일반 DLL을 참조한다. 최초 샌드박스 시도는 UnrealBuildTool 사용자 설정 폴더 접근 거부로 실패했으며, 권한 확보 후 재실행했다.
- 일반 DLL로 실행한 자동 테스트 28개 통과(24 성공, 4 경고 포함 성공, 실패/미실행 0). 근거: `Saved/QA/20261003T052616Z_127d6b99/Automation/index.json`. 테스트 경고는 기존 GameplayCue 경로 및 격리 테스트 월드의 시작 무기 설정이다.
- 새 프로세스에서 저장된 에셋을 다시 로드한 전체 에셋 검사도 통과했다. 최종 결과는 `PASS_WITH_WARNINGS`: `Saved/QA/20261003T052616Z_127d6b99/report.md`. 빌드의 비권장 MSVC 버전, 에셋 검사의 기존 스트리밍 풀 설정 우선순위·적 이름표 직렬화 경고는 해당 보고서에 남겼다.

### 남은 직접 플레이 검증

- 공격 버튼을 누른 채 콤보 반복, 해제 후 추가 반복 중지, 예약한 스킬·회피 우선 발동을 확인한다.
- 공격 중 UI 진입·커서 이동·포커스 상실 후 새로 누르기 전까지 반복이 재개되지 않는지 확인한다.
- 이동 중 회피와 정지 중 커서 회피의 자세·도착점, 콤보 유지/초기화 타이밍을 화면에서 비교한다. 타격음·이펙트·처치 시간과 전투 재미 평가는 별도 플레이 검증으로 남는다.

2026-09-21 / Unreal Engine 5.8

## 적용

- `RogueArena`, `StageDevMap`의 Recast를 Dynamic / Force Rebuild On Load로 설정했다. 복제된 맵의 Recast에 타일이 없으면 StageManager가 월드 초기화 다음 틱에 최초 빌드를 요청한다.
- 스폰 좌표는 적 캡슐 높이를 반영한다. 실제 캡슐 충돌과 플레이어까지의 완전한 경로를 검사하며, 실패 시 큐에서 재시도한다. 검증 실패 후 임의 위치에 강제 생성하던 처리를 제거했다.
- 공격 어빌리티가 활성화된 동안에도 60Hz 커서 조준을 유지한다. 입력 및 버퍼 공격 실행 시에도 조준을 갱신하며, 지면 트레이스 실패 시 캐릭터 높이의 수평면을 사용한다. 회피·피격 몽타주의 회전은 유지한다.
- PossessedBy에서 시작 어빌리티 등록 후 기존 장착 어빌리티를 즉시 실행한다. 해당 에셋의 AbilityTags가 비어 있어 StartUpData가 부여한 `InputTag.Equip.Weapon`으로 spec을 찾는다. 손 소켓·무기·스킬 구성은 기존 데이터를 사용한다.
- 플레이어 장착은 몽타주 알림을 기다리지 않는다. 이미 장착한 무기는 중복 처리하지 않으며, 동일 클래스의 무기 어빌리티 중복 부여도 방지한다.
- DA_InputConfig, IMC_Default, IMC_Weapon의 장착·해제 입력을 제거하고 BP_PlayerWeapon_Sword의 해제 어빌리티 부여를 제거했다. 플레이어 해제 어빌리티 활성화도 C++에서 차단한다.

## 검증

- Editor Win64 Development 빌드 성공: `Saved/Logs/CombatControlsBuild6.log`.
- 기존 `PG.*` 자동화 테스트 12개 모두 성공: `Saved/Logs/CombatControlsAutomation.log`, `Saved/Automation/CombatControls`.
- 실제 `-game -NullRHI` RogueArena 실행: `Saved/Logs/CombatControlsFinal.log`. 별도 RebuildNavigation 명령 없이 정상 동작했다.
  - 수정 전 적 이동 속도 0, 플레이어까지 경로 실패.
  - 수정 후 5마리 스폰, 모두 완전한 경로 확인. 초기 거리 931~994에서 8초 시점 약 66~81까지 접근했고 이후 측면 이동도 관측했다.
  - 2/8/16초 모두 기본 검이 `RightWeaponSocket`에 장착되어 있었고 해제 요청은 활성화되지 않았다.
- 실제 마우스를 움직이며 공격·회피를 비교하는 화면 검증은 수행하지 않았다. 조준 체감과 몽타주별 회전 연출은 PIE에서 확인할 수 있다.

## 재현 도구

- `Tools/Validation/ConfigureCombatControls.py`: 입력/무기 에셋 설정.
- `Tools/Validation/ConfigureStageNavigation.py`: 맵 내비게이션 설정.
- `Tools/Validation/InspectCombatControls.py`: 에셋·BT·내비게이션 읽기 전용 덤프.
- `PGCombatControlsProbe`: `-PGTestProfile=이름` 실행에서만 동작하는 진단 치트. 스테이지 1 시작, 테스트용 체력 설정, 2/8/16초 상태 기록. `-PGControlsExit` 사용 시 종료한다. 일반 플레이 저장 슬롯과 분리된다.
