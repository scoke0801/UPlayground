# 3·4단계 구현 및 검증 보고서

작업일: 2026-09-19. Unreal Engine 5.8.2 / Win64 / 로컬 싱글 플레이.

## 범위

3단계는 06·07·09의 파밍/빌드/저장 경로, 4단계는 10·11의 검사 확장과 12의 필요한 모듈 경계 정리다. 기존 1·2단계 변경을 보존했다. 수용 기준은 아래 실행 근거와 한계를 함께 읽는다. 기존 1·2단계의 전용 연출 에셋·신규 정예·조작감 수용 기준까지 완료한 것으로 간주하지 않는다.

## 3단계

- `PGProgressionData`: 24칸 가방, 획득 거리, 드랍 확률, 아이템 정의/등급/슬롯/기본 옵션/가중치/추가 난수 옵션, 빌드별 슬롯·스킬·쿨다운·해금 클리어 수를 데이터로 관리한다. 초기 카탈로그에는 장비 4개와 빌드 4개(두 방향 × 두 단계)가 있다.
- 아이템은 정의 ID·GUID·확정된 옵션을 보관한다. 난수 시드가 같으면 정의와 옵션을 재현한다. GUID는 획득 개체마다 다르다. 옵션 순서를 정렬해 TMap 순회 순서가 난수 결과에 영향을 주지 않게 했다.
- 적의 최초 사망 전이에서만 드랍한다. 파괴/스테이지 정리 경로는 추가 드랍하지 않는다. 드랍은 획득 거리/생존/가방 용량/중복 GUID를 검증하고 **저장 성공 이후에만** 제거한다. 저장 실패나 가방 포화 시 바닥에 남는다.
- `I`: 가방과 비교·장착/해제·빌드 선택·버리기. `E`: 가장 가까운 드랍 획득. 가방을 열면 전투를 일시정지한다. 겹치는 바닥 라벨은 가방의 근처 드랍 목록에서 세로 정렬하며, 일반/마법 이상/희귀 필터를 제공한다. 버리기는 두 번 클릭하고, 장착 중인 아이템은 먼저 해제한다.
- 무기 1슬롯과 장신구 1슬롯의 소유 장비는 기존 무기 액터 보너스와 별도 Infinite GE로 적용한다. 무기 액터의 기존 BP/애니메이션 연결을 유지하며 새 소유 장비는 수치 보너스다. 아이템 교체에 따른 별도 무기 외형 교체는 포함하지 않는다.
- 시작 스킬의 C++ ID 목록을 카탈로그로 이관했다. 로드아웃은 슬롯 매핑을 교체하고 기존 슬롯의 사용 시각을 보존한다. 단계를 더하려면 카탈로그에 빌드를 추가한다. 현재 성장 효과는 스킬 구성과 쿨다운이며, 신규 피해 계수·자원 비용 체계는 추가하지 않았다.
- 기존 액티브 스킬 테이블의 쿨다운이 0이므로 카탈로그에 기본 초 단위 쿨다운도 정의했다. Rapid의 주력 쿨다운은 5.6→4.4초, Heavy는 8/12→6.4/9.6초이며 HUD는 실제 적용된 스킬 핸들러의 잔여 시간을 표시한다. `UpgradeStage34Loadouts.py`는 원본 에셋을 Saved/Backups에 보관하고 초기 생성 데이터만 이관한다.
- `PGProfileSubsystem`이 Pawn 밖에서 소유/장착/빌드/누적 클리어/체크포인트/최근 보상 토큰/런 보너스를 소유한다. 적·투사체·시전·현재 HP는 저장하지 않고 구간 시작 상태로 재구성한다. 재시작은 해당 구간부터, 마지막 구간 이후 새 런은 장비·빌드·누적 클리어를 유지하고 런 보너스를 초기화한다.
- 보상 스탯과 다음 체크포인트를 같은 스냅샷으로 저장한다. 이전 저장으로 돌아갈 때는 그 스냅샷에 포함된 지급도 함께 돌아간다. 지원하는 보상 카드 유형은 기존 Stat이며, 장비는 적 드랍, 빌드 성장은 누적 클리어 해금과 가방 선택으로 연결한다.
- 저장은 두 슬롯을 교대로 사용한다. 최신 파일이 구조적으로 유효하지 않으면 이전 유효 파일로 복원한다. 미지원 버전/유효 파일 없음은 원본을 덮어쓰지 않고 저장을 차단한다. 가방에서 복구를 두 번 확인하면 원본 바이트를 별도 Recovery 슬롯에 보관한 뒤 마지막 유효 프로필(없으면 빈 프로필)로 시작한다. 알 수 없는 아이템 ID는 옵션과 함께 보관하지만 장착할 수 없다. 알 수 없는 빌드 ID는 기본 빌드로 실행한다.

## 4단계

- PGData의 StartupData가 커스텀 ASC/Ability 대신 엔진 `UAbilitySystemComponent`/`UGameplayAbility`를 사용한다. **PGData→PGAbilitySystem 순환 예외를 제거**했다. 저장된 파생 BP 어빌리티 클래스는 허용 타입의 하위 클래스이므로 그대로 로드한다.
- PGShared의 불필요한 PGAbilitySystem include 경로를 제거했다. Build.cs의 중복 PGData/PGMessage/AIModule 항목을 정리했다.
- StageManager가 구체 보상 위젯을 생성하지 않는다. `StagePresentation` 메시지로 선택지/토큰/상태/약한 UObject 델리게이트를 전달하고, PGUIManager가 UI와 입력 모드를 소유한다. 선택 콜백은 StageManager의 검증/지급으로 돌아간다. UI가 없는 자동 테스트에서도 진행 로직을 검사한다.
- Actor/UI/ASC의 메시지와 UI 접근, 플레이어 스킬 메시지에 World/GameInstance 문맥을 명시했다. 매니저 종료 시 다른 인스턴스의 static weak 포인터를 지우지 않는다. 기존 비문맥 API는 호환용으로 남아 있으며 전체 멀티 PIE 격리를 주장하지 않는다.
- 파밍 카탈로그의 중복 ID/옵션/가중치/스킬/슬롯/쿨다운 범위를 시작 전에 검사한다. 시작 로드아웃 몽타주를 준비/보유한다. `ValidateStage34Assets.py`는 읽기 전용 에셋 교차 검사, `AuditModuleDependencies.py`는 실제 include와 선언 의존성 목록을 산출한다.
- `PGProfileStatus`, `PGDropItem <ID> <Seed>`, `PGSaveFailure <bool>`, `PGFarmingSmoke`, `PGStress <적 수> <드랍 수>`를 추가했다. 실제 동작은 Shipping에서 비활성이다. PGFarmingSmoke는 `-PGTestProfile=이름`이 없으면 실행하지 않는다.
- 부하 검사에서 소환 우선순위가 횟수 상한을 우회하고, 한 번의 시전이 여러 소환 태스크를 생성하는 기존 결함을 발견했다. 상한을 우선 검사하고 적합한 스킬이 없으면 대기하며, 한 태스크로 정의된 수만 소환하도록 수정했다. 반복 Notify/취소 뒤 비동기 완료는 추가 소환하지 않는다.
- 데미지 플로터의 실제 활성 목록을 추적하고 전체 128개/대상별 8개를 기본 상한으로 두었다. 초과 요청은 생략하며, 중복 반환을 막고 WorldCleanup에서 활성/대기 위젯과 대상 참조를 정리한다. 플로터 매니저 접근도 월드 문맥을 사용한다.
- 일반 C++ 스킬 핸들러에 잘못 사용하던 `TObjectPtr`를 명시적으로 초기화한 네이티브 포인터로 바꿨다. 기존 캐릭터 EndPlay의 소유 객체 해제 경로를 유지한다.

## 확인된 실행 결과

| 검사 | 근거 |
|---|---|
| 3단계 Editor 빌드 | `Saved/Logs/Stage34_Stage3Build.log` — Succeeded |
| 4단계 Editor 빌드 | `Saved/Logs/Stage34_Stage4Build.log` — Succeeded |
| 최종 Editor 빌드 | `Saved/Logs/Stage34_FinalBuild.log` — Succeeded |
| 자동 테스트 | `Saved/Automation/Stage34Final/index.json` — 8개 통과, 실패 0 |
| 카탈로그/BP 검사 | `Saved/Logs/Stage34_AssetValidation.log` — 아이템 4 / 빌드 4 / 스킬 37 |
| 실제 파밍 흐름 | `Stage34_MapSmoke.log` — pickup/equip/failedSaveRejected/reward 모두 1, revision 0→3, 체크포인트 1→2, 공격 320→422 |
| 프로세스 재실행 | `Stage34_RenderedSmoke.log` — revision 3, 장비/아이템/체크포인트 2, 공격 422 복원. 후속 보상으로 revision 6, 체크포인트 3 |
| 최종 렌더링/성장 검사 | `Stage34_FinalRenderedSmoke.log` — pickup/equip/failedSaveRejected/reward/growth 모두 1, 실제 쿨다운 5.6→4.4초. `Saved/Screenshots/WindowsEditor/ScreenShot00002.png`에서 1280×720 패널 가독성 확인 |
| Win64 Development 패키지 | `Saved/Logs/Stage34_Package.log` — BUILD SUCCESSFUL, ExitCode=0. `Saved/Stage34Package/Windows/UPlayground.exe` |
| 패키지 파밍/재실행 | `Stage34_PackagedSmoke.log`, `Stage34_PackagedRestore.log` — 두 실행 모두 5개 흐름 플래그가 1. 재실행 시 revision 5 / 아이템 1 / 장착 1 / 체크포인트 2 / Rapid2 / 공격 422 / 최대 체력 1600 복원, 다음 보상 후 체크포인트 3 |
| 모듈 의존 검사 | `Saved/Automation/Stage34/ModuleDependencies.json` |
| 사망→실제 맵 재시작 20회 | `Saved/Logs/Stage34_Retry20.log` — COMPLETE failures=0. 매회 생존/캡슐 충돌/이동 입력 차단 해제/사망 태그 제거/InProgress 확인 |

자동 테스트는 기존 Stage/Combat 5개와 `PG.Progression.TransactionsAndSerialization`, `PG.Lifecycle.MessageIsolation`, `PG.AI.SummonBudget`이다. 파밍 검사에는 시드 재현, 중복/포화, 저장 실패 원복, GUID/옵션 직렬화, 디스크 저장·로드 20회, 이전 슬롯 유지, 구조적 손상/미지원 버전, 장비 효과 적용·해제 100회가 포함된다. 메시지 검사는 두 매니저 간 격리와 20회 구독 해제를 확인한다. 소환 검사는 높은 동적 우선순위도 상한을 우회하지 못하는지 확인한다. 전체 다중 PIE E2E 검사는 아니다.

## 부하 측정

i5-12400F / RAM 64GB / RTX 3060 Ti, Editor의 `-game -RenderOffscreen`, 1280×720, StageDevMap, 추가 적 100/드랍 100, 3,600프레임을 측정했다. 최초 120프레임을 제외한 3,480프레임의 결과다. 소환 적과 전투 효과를 허용하므로 실제 활성 적 수가 100으로 고정된 시나리오는 아니다. 동시 별도 빌드 작업 없이 측정했으며 프레임별 적 배치/AI 난수까지 동일한 통제 실험은 아니다.

| 지표 | 수정 전 평균 / p95 | 수정 후 평균 / p95 |
|---|---:|---:|
| 프레임 | 85.57 / 135.78 ms | 50.59 / 68.68 ms |
| Game thread | 85.53 / 136.56 ms | 50.57 / 68.50 ms |
| Render thread | 10.69 / 15.31 ms | 9.17 / 11.44 ms |
| GPU | 18.85 / 28.22 ms | 14.30 / 17.38 ms |
| UI(Exclusive Game thread) | 23.62 / 49.96 ms | 1.22 / 1.65 ms |

근거: `Saved/Profiling/Stage34/Stress100.csv`, `Stress100After.csv` 및 각 `.summary.json`, `Saved/Logs/Stage34_Stress*.log`. `SummarizeStage34Csv.py`가 CSV 메타데이터 행을 제외하고 계산한다. **60fps / p95 16.7ms 제안 기준은 미달**이다. UI 비용은 크게 감소했지만 나머지 Game thread의 애니메이션/물리/전투 비용을 추가 프로파일해야 한다. 이번 실행은 수정 전 약 5분, 수정 후 약 3분이며 20분 soak나 고정 200투사체/초당 100플로터의 통제 시나리오를 대신하지 않는다.

## 실행

`/Game/Maps/StageDevMap`에서 플레이한다. 튜닝은 `/Game/DataCenter/Progression/DA_PGProgression`에서 한다. 기존 에셋을 덮어쓰지 않도록 초기 생성 도구는 카탈로그가 없을 때만 생성한다.

```powershell
$engine = 'C:/Program Files/Epic Games/UE_5.8'
$project = "$PWD/UPlayground.uproject"
& "$engine/Engine/Build/BatchFiles/Build.bat" UPlaygroundEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
& "$engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $project -unattended -NullRHI '-ExecCmds=Automation RunTests PG.' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD/Saved/Automation/Stage34Final"
& "$engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $project -unattended -NullRHI -EnablePlugins=PythonScriptPlugin -run=pythonscript "-script=$PWD/Tools/Validation/ValidateStage34Assets.py"
& "$engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $project /Game/Maps/StageDevMap -game -unattended -NullRHI -benchmark -seconds=15 -fps=60 -PGTestProfile=MyFarmingCheck '-ExecCmds=PGFarmingSmoke'
& "$engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $project /Game/Maps/StageDevMap -game -unattended -NullRHI -PGTestProfile=MyRetryCheck -PGRetryProbe=20
```

테스트 슬롯 이름은 `PGTest_<이름>_0/1`, 일반 플레이는 `PGProfile_0/1`, PIE는 인스턴스별 이름을 사용한다. 자동 테스트는 고유한 임시 슬롯만 만들고 지운다. Content는 별도 저장소이며 새 `DataCenter/Progression/DA_PGProgression.uasset`도 소스와 함께 관리해야 한다. 커밋/푸시는 수행하지 않았다.

## 수용 기준의 한계

- 두 빌드의 동일 조건 실제 DPS/생존 비교, 10~15분 자연 플레이, 실제 여러 PIE 월드 E2E는 별도 수용 검증이다. 빌드 선택/쿨다운 데이터 연결만으로 밸런스 완료를 주장하지 않는다. 재시작 20회는 NullRHI 실행이므로 화면과 조작감 검증과 구분한다.
- 아이콘/장비 외형과 획득 VFX/SFX, 바닥 라벨 자체의 투영·겹침 해소, 패드/재바인딩 설정은 아직 폴리싱 대상이다. 인벤토리 목록이 겹침 없는 획득 경로를 제공한다.
- PGAbilitySystem↔PGActor/PGAI, PGActor↔PGUI의 기존 순환 예외는 실제 구체 참조가 남아 있어 유지한다. 전체 프레임워크 재편이나 에셋 클래스 이동을 수행하지 않았다.
- 전체 비동기 로딩·캐시 소유권 개편, 모든 몽타주/연계 에셋의 사전 로드, 원자적 파일 교체/전원 차단 내구성은 완료했다고 주장하지 않는다. 저장은 작은 로컬 프로필에 대한 동기식 이중 스냅샷이다.
