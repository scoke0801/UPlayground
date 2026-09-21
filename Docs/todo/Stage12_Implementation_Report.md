# 1·2단계 구현 및 검증 보고서

후속 작업: [3·4단계 구현 및 검증 보고서](Stage34_Implementation_Report.md). 아래 내용은 1·2단계 완료 시점의 기록이며, 인벤토리·장비·저장 및 모듈 경계 변경은 후속 보고서를 따른다.

작업일: 2026-09-19. 대상: Unreal Engine 5.8.2, Win64, 싱글 플레이 키보드·마우스.

## 범위와 판정

1단계는 실행 기준선·스테이지 진행·보상 확정·필수 데이터 검증, 2단계는 쿼터뷰 조작·GAS 전투 수치·입력 반응성과 기본 피드백으로 진행했다. 코드와 기존 BP/테이블 연결을 함께 변경했다. 3단계의 인벤토리·장비 획득 UX·저장, 4단계의 모듈 재편은 이번 범위에 포함하지 않는다.

**핵심 구현은 반영했으며 상용 품질 수용 기준 전체를 완료한 상태는 아니다.** 렌더링 없는 자동 실행으로 확인할 수 없는 화면·조작감, 별도 연출 에셋과 신규 정예 콘텐츠는 아래 잔여 항목으로 구분한다.

## 1단계 변경

- 실제 설정을 기준으로 문서 엔진 버전을 5.8로 정정했다. 설치 엔진은 5.8.2이며 엔진 연관 설정을 변경하지 않았다.
- 스테이지 메시지 구독을 각각 보관·해제한다. 지연 스폰은 시작 후 경과 시간으로 판정하고, 간격 0도 타이머의 유한 배치로 처리한다. 스폰 성공 시에만 대기열을 소비하며 재시도 상한을 둔다.
- 적 인스턴스를 중복 집계하지 않는다. 사망 및 파괴 알림이 겹쳐도 남은 수를 한 번만 줄인다. 소환 적의 클리어 포함 여부를 데이터로 정한다. 대기열과 생존 적이 모두 없어야 보상 단계로 넘어간다.
- 보상 후보는 가중 추출하며 한 번 생성한 토큰·선택 인덱스·진행 상태·플레이어 생존·실제 지급 성공을 확인한다. 기존 `OnRewardSelected` 직접 호출은 지급을 우회하지 못한다.
- 현재 보상은 최대 체력 +100, 공격 +20, 방어 +30 중 선택하는 Stat 보상이다. 최대 체력 증가는 즉시 회복하지 않는다. Item/Skill 지급은 구현하지 않았으므로 해당 보상 데이터는 시작 전에 거절한다. 기존 미지원/빈 풀은 이번 검증용 스탯 풀로 이관했다.
- 빈 풀에는 명시적인 다음 구간 버튼을 제공한다. 실패·사망·마지막 구간 완료 화면과 현재 맵을 다시 여는 재시작을 제공한다. 저장 복원 기능은 아니다.
- SearchKey 중복과 동일 행 타입의 테이블 중복을 차단한다. GameInstance 초기화 전에 끝나지 않은 에셋 검색을 보완하고, 투사체 서브시스템의 데이터 의존성을 명시했다. `/Game/DataCenter`를 쿠킹 대상에 포함했다.

## 2단계 변경

- `PGQuarterViewData`: 고정 회전, 카메라 거리·줌 범위·추적 지연·조준 Trace를 노출했다. BP_LocalPlayer에 프리셋을 연결했다. WASD와 지면 조준을 분리하고 UI·일시정지·사망·포커스 상실 시 입력을 차단한다.
- `UPGAtrributeSet`: 현재/최대 체력, 공격·방어·치명타·회복·이동속도를 관리한다. 기존 StatComponent는 GAS 조회/초기화 어댑터로 사용한다. 실제 클래스의 기존 철자는 BP 호환을 위해 유지했다.
- 피해는 `공격 × 방어상수 / (방어상수 + max(방어, 0))`에 치명타 배율을 적용하고 최소 피해를 보장한다. 치명 확률은 기존 데이터의 10000 기준, 추가 치명 피해는 100 기준이다. 수치는 CombatTuning DataAsset으로 노출했다.
- 피격 리액션 어빌리티 활성 여부와 피해 확정을 분리했다. 회복은 최대 체력까지 제한하고 사망 후 회복으로 되살아나지 않는다. 장비 효과는 기존 GE를 제거하고 새 Infinite GE로 교체한다. 피해 계산에서 무기 공격력을 다시 더하지 않는다.
- 최근 입력 한 개를 기본 120ms 동안 보관한다. 성공하면 소비하며 만료·홀드 해제·사망·입력 차단·장비 교체 시 버린다. 토글 입력은 예약하지 않는다. 몽타주/사용 조건 검증 후에 비용·쿨다운·콤보 사용을 확정한다.
- 스킬 행의 공격/회피 캔슬 비율과 기존 ComboWindow Notify를 연결했다. 히트스톱은 확정 피해에만 적용하고 재적용 간격·시간 상한·강도를 제한한다. Niagara/SFX/CameraShake 슬롯을 제공한다.
- 근접 중복 타격, AOE 다중 대상과 이탈 처리, 반환된 투사체의 잔여 충돌 및 월드 전환 후 풀 참조를 보완했다. AI 체력 비율은 부동소수 계산과 최대 체력 0 방어를 사용한다.
- 실제 실행에서 보고된 구조체 기본값 누락을 정정하고 CommonUI용 GameViewportClient를 지정했다.

## 검증 결과

| 검증 | 결과 및 근거 |
|---|---|
| 변경 전 Editor 기준선 빌드 | 통과 — `Saved/Logs/Stage12_BaselineBuild.log` |
| 1단계 Editor 빌드/데이터 테스트 | 통과 — `Stage12_Stage1Build.log`, `Saved/Automation/Stage1/index.json` |
| 2단계 Editor 빌드 | 통과 — `Saved/Logs/Stage12_Stage2Build.log` |
| 최종 자동 테스트 | 5개 통과, 실패 0 — `Saved/Automation/Stage2/index.json` |
| 에셋 교차 참조 | 스테이지 3 / 적 9 / 스킬 37 / 스탯 보상 3 — `Stage12_AssetValidation.log` |
| StageDevMap 실행 | NullRHI 25초, 정상 종료. 초기 HP 1500/1500, 공격 320, 방어 1000, 치명 1000. Stage 1, InProgress, 남은 적 3 — `Stage12_MapSmoke.log` |
| Game Development 빌드 | 통과 — `Saved/Logs/Stage12_GameBuild.log`; 최종 코드는 `Stage12_Package.log`의 Game/Editor 동시 빌드로 재검증 |
| 최종 Development 패키지 및 실행 | 통과 — BuildCookRun ExitCode 0. 패키지 NullRHI 25초 정상 종료, Error/Fatal/Ensure 로그 0. 실제 스탯·Stage 1·적 3종·투사체 풀 확인 — `Stage12_Package.log`, `Stage12_PackagedSmoke.log` |

자동 테스트는 `PG.Stage.DataValidation`, `PG.Stage.Lifecycle`, `PG.Combat.DamageBoundaries`, `PG.Combat.AttributesEquipmentAndHealing`, `PG.Combat.InputBuffer`다. 지연 스폰, 중복 사망, 오래된 토큰, 지급 우회, 피해/회복 경계, 장비 100회 적용·해제, 입력 예약 성공·만료·해제·사망을 검증한다. 월드 전체의 전투/보상 플레이를 자동으로 끝내는 E2E 테스트는 아니다.

검증 중 AttributeSet을 ASC의 자식으로 생성하면 피해 처리 시 Actor 캐스트에서 충돌하는 문제를 발견했다. GAS `InitStats`로 Actor 소유의 AttributeSet을 생성하도록 수정한 뒤 테스트를 통과했다. 실제 맵에서는 에셋 레지스트리 초기 검색 시점 때문에 수치가 1로 남고 스테이지가 실패하는 문제를 발견해 동기 검색과 서브시스템 의존성으로 수정했다.

## 실행 및 재현

에디터에서 `/Game/Maps/StageDevMap`을 열어 플레이한다. 프로젝트의 기본 시작 맵은 기존 EmptyDevMap을 유지한다. 패키지는 `Saved/Stage12Package/Windows/UPlayground.exe /Game/Maps/StageDevMap`으로 실행한다. 카메라·피드백·전투 수치는 `/Game/DataCenter/Stage12`의 세 DataAsset에서 조정한다. 스킬 캔슬 비율은 DT_Skill에서 조정한다.

콘솔 명령: `PGStageStatus`, `PGStartStage 1`, `PGCombatStats`. Shipping에서는 동작하지 않는다.

PowerShell에서 저장소 루트를 기준으로 실행한다.

```powershell
$engine = 'C:/Program Files/Epic Games/UE_5.8'
$project = Join-Path (Get-Location) 'UPlayground.uproject'
& "$engine/Engine/Build/BatchFiles/Build.bat" UPlaygroundEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
& "$engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $project -unattended -NullRHI '-ExecCmds=Automation RunTests PG.' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD/Saved/Automation/Stage2"
& "$engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $project -unattended -NullRHI -EnablePlugins=PythonScriptPlugin -run=pythonscript "-script=$PWD/Tools/Validation/ValidateStage12Assets.py"
& "$engine/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun "-project=$project" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive "-archivedirectory=$PWD/Saved/Stage12Package" '-map=/Game/Maps/StageDevMap' -CookMapsOnly -unattended
```

`InspectStage12Assets.py`는 읽기 전용 조사, `ValidateStage12Assets.py`는 읽기 전용 검사다. `ConfigureStage12Assets.py`는 에셋 변경 도구이며 정상 프로젝트 실행의 선행 조건이 아니다. 이미 이관을 실행했고 원본은 `Saved/Backups/Stage12`에 1회 보관했다. 기존 파일을 확인하지 않고 백업으로 덮어쓰지 않는다.

## 에셋 및 버전 관리

루트 저장소는 Content를 제외하고 Content는 별도 Git 저장소다. 이번 소스와 함께 아래 Content 변경을 별도로 관리해야 한다.

- `Blueprints/Actor/LocalPlayer/BP_LocalPlayer.uasset`
- `DataCenter/DataTables/Stage/DT_StageData.uasset`
- `DataCenter/DataTables/Reward/DT_StatReward.uasset`
- `DataCenter/Stage12/DA_PGQuarterView.uasset`
- `DataCenter/Stage12/DA_PGCombatFeedback.uasset`
- `DataCenter/Stage12/DA_PGCombatTuning.uasset`

기존 솔루션 파일과 Content/Temp/IKRetarget의 삭제 상태는 이번 작업에서 변경하지 않았다. Git 커밋/푸시는 수행하지 않았다. 루트 문서와 `Docs/todo/*.md`가 무시되지 않도록 `.gitignore` 예외를 추가했다.

## 남은 수용 기준과 제한

- 16:9/21:9, 최소·최대 줌, UI 클릭 차단, 포커스 복귀, 회피/루트모션 회전, 30/60/120fps 콤보·캔슬을 화면에서 확인해야 한다. NullRHI 성공을 조작감/렌더링 검증으로 간주하지 않는다.
- 현재 피드백 프리셋은 히트스톱 수치를 연결했다. 전용 VFX/SFX/CameraShake 에셋은 미지정이다. 강도 값은 BP에서 조정할 수 있지만 사용자용 설정 메뉴는 없다.
- 화면 가장자리 보정·장애물 가림 표시, 신규 정예의 예고→공격→회복 설계, 인디케이터와 실제 판정 일치 검증은 남아 있다. 기존 9종 적 데이터 검사를 신규 적 콘텐츠 완성으로 보지 않는다.
- 사망→재시작, 보상 버튼 연타, 마지막 구간 완료의 실제 UI 반복 플레이가 남아 있다. 현재 스탯 보상은 런 내 보너스이며 저장하지 않는다.
- 동적 GE와 전역 서브시스템 접근은 현재 싱글 플레이 범위다. 네트워크 복제와 다중 PIE 월드 격리, 전체 캐시/비동기 로딩 수명 개선은 후속 작업이다.
- 다수 적·투사체 부하, AI 전체 탐색 비용, 프레임 시간은 아직 측정하지 않았다. 기존 모듈 순환 의존과 엔진 API deprecated 경고도 후속 정리 대상이다.
- 외부 `SM_Cube`가 이전 경로의 `MI_PrototypeGrid_Gray`를 참조하는 기존 로드 경고와 BP_Enemy_Base의 EnemyNamePlate 직렬화 타입 불일치 경고가 있다. 에셋 이관/리다이렉트 확인 대상으로 남겼다.
