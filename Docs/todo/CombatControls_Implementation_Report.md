# 스테이지 이동·커서 조준·기본 무기 장착

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
