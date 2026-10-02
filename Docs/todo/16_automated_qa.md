# 자동 QA 실행

현재 RogueArena MVP의 기존 검사를 한 명령으로 실행한다. 실행기는 게임 데이터 제작 스크립트를 호출하지 않으며 결과를 `Saved/QA/<UTC시각_고유ID>/`에 보존한다.

## 실행 명령

프로젝트 루트의 PowerShell에서 실행한다. Unreal에 포함된 Python을 사용하므로 별도 Python 설치는 필요 없다.

```powershell
# 빌드 + 자동 테스트 + 에셋 검사 + 6구간 진행 + 사망/재시작 20회
.\Tools\Validation\RunQA.ps1 -Suite full

# 반복 개발용: 빌드 + 자동 테스트 + 에셋 검사
.\Tools\Validation\RunQA.ps1 -Suite quick

# 엔진 설치 위치를 직접 지정
.\Tools\Validation\RunQA.ps1 -Suite full -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'

# 빌드가 이미 최신일 때만 사용. 결과에 빌드 SKIPPED가 남는다.
.\Tools\Validation\RunQA.ps1 -Suite full -SkipBuild
```

엔진 경로 기본값은 실제 `.uproject`의 EngineAssociation과 Epic Games 기본 설치 경로에서 구한다. 프로젝트와 Content 저장소의 커밋 및 변경 목록, 엔진 Build.version, 단계별 명령·소요 시간·종료 코드·경고를 `report.json`에 남긴다. `report.md`는 사람이 읽는 요약이다. 서로 다른 QA 실행은 순차 실행한다.

UE 빌드는 UnrealBuildTool 사용자 캐시에도 쓰므로 해당 경로에 접근 가능한 개발 계정이 필요하다. Live Coding이나 DLL 잠금으로 빌드가 거절되면 편집 내용을 저장한 후 에디터를 종료하고 재실행한다. 실행기는 기존 에디터를 종료하지 않는다.

## 검사 범위와 판정

| 단계 | 검사 | 통과 조건 |
|---|---|---|
| build | Development Editor 증분 빌드 | 빌드 프로세스 성공. 실패 시 후속 검사 중단 |
| automation | `Automation RunTests PG.` | Source에서 발견한 모든 PG 테스트가 결과에 존재하고 성공. 미실행/진행 중/실패가 없어야 함 |
| assets | `ValidateRoguelikeMVP.py` | 현재 프로세스에서 에셋·참조·강화 그래프 검사 성공 로그 확인 |
| cycle | RogueArena의 기존 `PGCombatCycleSmoke` | 1~5구간 각 3웨이브와 보스 1웨이브가 순서대로 등장, 구간별 선택 1/2/1/2/1회, 각 구간 최종 보상 확정, 7회 선택 후 Finished |
| retry | 기존 `PGRetryProbe=20` | 20회 사망/맵 재시작 후 최초 상태 포함 21회 생존·충돌·입력·스테이지 상태 정상 |

프로세스 종료 코드만으로 성공 처리하지 않는다. 검사 결과 누락, 크래시/ensure, 시간 초과, 단계별 완료 증거 누락은 실패다. 자동 테스트의 의도된 오류는 Unreal의 테스트 판정을 따르고 경고는 별도 보존한다. 나머지 단계의 `Log*: Error:`는 실패로 처리한다. 단, retry의 정확한 `Stage 1 failed: Player defeated.` 20건은 주입한 사망이며, 이 횟수와 21회 정상 상태를 함께 검사한다. 다른 오류는 허용하지 않는다. 새 실행마다 고유 디렉터리를 사용하므로 과거 테스트 리포트가 현재 성공 근거가 되지 않는다.

cycle은 실제 시간과 60fps 상한으로 실행한다. 시간 가속 benchmark는 비동기 내비게이션/월드 준비보다 스폰 재시도 타이머를 빠르게 소진할 수 있으므로 사용하지 않는다. 완료/실패 로그로 정상 종료를 요청하며 240초 게임 시간 및 600초 프로세스 제한도 적용한다.

프로세스 종료 코드는 PASS/PASS_WITH_WARNINGS가 0, FAIL이 1이다. 경고가 있는 통과는 무경고 통과와 구분한다. `-SkipBuild`나 `quick`으로 생략한 범위는 완료한 것으로 취급하지 않는다.

각 UE 실행에 고유한 `-PGTestProfile=QA_...`를 전달하여 일반 저장 슬롯과 분리한다. 자동 테스트 자체도 고유한 테스트 슬롯을 사용한다. 재현을 위해 QA 저장 파일과 로그는 남긴다. 시간 초과/중단 시 실행기가 시작한 프로세스 트리만 정리한다.

## 자동 QA가 증명하지 않는 것

- cycle은 체력 보정·자동 처치를 사용하는 **진행 회귀 검사**다. 일반 플레이어 입력, 공격 명중, 실제 생존 능력, 세 빌드의 균형을 증명하지 않는다.
- NullRHI이므로 HUD 가독성, 텔레그래프와 화면상 판정의 일치, VFX/SFX, 타격감, GPU 성능은 검사하지 않는다.
- 현재 저장 트랜잭션/직렬화 테스트와 맵 재시작은 새 OS 프로세스에서의 체크포인트 이어하기 검사를 대체하지 않는다.
- cooked 패키지 실행과 장시간 부하 검사는 이 실행기에 아직 포함하지 않는다.

후속 순서는 실제 입력을 사용하는 고정 전투 시나리오와 피해/피격/시간 기록 → 렌더링 UI 캡처 검토 → 현재 MVP 패키지에서 이어하기 → 고정 환경 성능/20분 부하다. DPS 숫자가 유사한 것만으로 빌드 재미나 난이도를 통과 판정하지 않는다.

## 실행기 판정 회귀 검사

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' -B Tools/Validation/TestQARunner.py
```

테스트 누락·0개 실행·미완료, 보상 누락·중복 웨이브, 재시작 중 비정상 상태가 거짓 통과하지 않는지 확인한다.

## 2026-09-30 실행 결과

UE 5.8.2, 현재 소스 및 작업 중인 Content 에셋으로 실행했다. 결과는 **FAIL**이며 통과한 검사와 진행 불능을 구분한다.

| 검사 | 결과 |
|---|---|
| 실행기 판정 테스트 | 6개 성공 |
| Development Editor | 성공 |
| PG 자동 테스트 | 12개 성공: 일반 성공 10개, 경고 포함 2개, 실패/미실행 0개 |
| Rogue MVP 에셋 검사 | 성공 |
| RogueArena 독립 게임 실행 | 1구간 1웨이브에서 `Spawn retries exhausted: 15101`로 실패 |
| 사망/재시작 | 20회 및 정상 상태 21회 확인, 실패 0 |

실행 증거: `Saved/QA/20260930T124707Z_ca738233/report.json`, `report.md`, `cycle.log`, `retry.log`, `Automation/index.json`. 증분 빌드 1.81초, 자동 테스트 18.41초, 에셋 검사 11.31초, 진행 검사 17.56초였다. 첫 전체 빌드 로그는 `Saved/QA/20260930T124103Z_8ed79872/build.log`에 있다.

첫 benchmark 실행과 시간 가속을 제거한 60fps 상한 실행에서 같은 스폰 실패를 확인했다. 시간 가속만으로 설명할 수 없다. 스폰 경로는 내비게이션 투영·플레이어까지의 경로·지면 경사·캡슐 충돌을 모두 요구하지만 현재 로그는 어느 조건이 실패했는지 구분하지 않는다. **NavMesh가 원인이라고 확정하지 않았으며**, 다음 수정은 독립 실행에서 이 조건별 실패를 관찰하고 PIE와 비교하는 것이다. 해당 경로가 통과하기 전 6구간 완주를 QA 완료로 표시하지 않는다.

기존 MVP의 렌더링 PIE 완주 기록은 다른 실행 조건이다. 이번 자동 QA는 독립 `-game -NullRHI` 실행을 추가한 것으로, 패키지 빌드나 화면이 있는 플레이에서도 동일하게 재현된다고 단정하지 않는다.

에셋 검사에서는 `PGWidgetComponentBase` 직렬화 참조가 `PGUIEnemyNamePlate` 타입과 맞지 않아 null 처리된다는 기존 경고도 관찰했다. 시각 QA에서 적 네임플레이트 BP 연결을 확인할 후속 항목이다. 테스트의 저장 실패/복원 시나리오 경고와 GameplayCue 검색 경로 경고도 리포트에 보존했다.

## 2026-09-30 스폰 수정 후 결과

앞의 FAIL은 수정 전 이력이다. 현재 코드의 전체 실행은 `Saved/QA/20260930T132517Z_1a11655e/report.md`와 `report.json`에 있으며 **PASS_WITH_WARNINGS**다.

| 검사 | 결과 |
|---|---|
| 실행기 판정 테스트 | 7개 성공 |
| Development Editor | 성공, 21.8초 |
| PG 자동 테스트 | 13개 성공: 일반 12개, 경고 포함 1개 |
| Rogue MVP 에셋 검사 | 성공, 기존 강화 18개 |
| 독립 게임 진행 | 16웨이브·7선택·6구간 완료, 156.4초 |
| 사망/재시작 | 20회 및 정상 상태 21회, 44.5초 |

원인은 StageManager의 다음 틱 내비게이션 초기화가 구간 시작의 타이머 정리에 취소되는 것이었다. 타일이 없는 Dynamic NavMesh의 준비를 웨이브 스폰 타이머 등록 앞으로 옮겼다. 실패 조건별 로그와 디버그 표시, 저장 가능한 런 시드와 Assisted 표시, 구간별 피해/시간/보상 기록도 연결했다. 변경과 저장 예외 처리는 [콘텐츠 구현 계획의 실행 기록](17_content_implementation_plan.md#2026-09-30-0단계-실행-기록)에 있다.

cycle은 고정 시드 `173001`을 사용하고 현재 실행의 기록 파일을 `cycle.telemetry.json`으로 복사한다. 6개 구간의 Cleared, 동일 시드, Assisted, 구간별 확정 보상 수 `1/2/1/2/1/0`, 유한한 비음수 수치와 양수 전투 시간을 검사한다. 자동 처치는 전투 피해로 계산하지 않으므로 이 실행의 직접 피해 0을 DPS 자료로 사용하지 않는다. 실제 GAS 직접/추가 피해 집계는 별도 `PG.Run.TelemetryAndSeed` 테스트로 검증한다.

## 렌더링 내비게이션 비교

최신 빌드에서 전체 QA와 겹치지 않게 실행한다. 에디터를 새로 실행하며 대상 맵과 제작 에셋은 저장하지 않는다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' -B Tools/Validation/RunNavigationPresentation.py

# 현재 설치 엔진의 한국어 smoke test 오류를 구분하는 진단 실행
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' -B Tools/Validation/RunNavigationPresentation.py --mode pie --culture en
```

`--mode standalone`, `--mode pie`로 범위를 선택한다. 이 검사는 NullRHI 대신 offscreen 렌더링과 `Shot SHOWUI`를 사용한다. 2·8·16초에 준비된 내비게이션과 플레이어 투영, 완전한 경로를 따라 움직이는 적, 현재 프로세스의 새 PNG를 요구한다. 보고서는 `assisted=true`, `direct_input=false`와 언어·명령을 기록한다. 오류를 무시하지 않으며 화면 캡처의 가독성 판정은 별도로 수행한다.

- 독립 실행: `Saved/QA/20260930T133020Z_0a39b102_navigation`의 standalone PASS. 적 스폰·접근과 HUD를 캡처에서 확인했다.
- 기본 한국어 PIE: `Saved/QA/20260930T133437Z_7fa14702_navigation` FAIL. 스폰·이동·정상 종료는 확인했지만 시작 시 UE 엔진의 `FUnifiedErrorTest_CreateErrorMessage`가 현지화 메시지와 영어 기대값을 비교해 오류 15건을 낸다. 엔진 소스 `Runtime/Core/Tests/Experimental/UnifiedError/UnifiedErrorTests.cpp:479` 이하에서 확인했다.
- 영어 지정 PIE: `Saved/QA/20260930T133545Z_7426a250_navigation` PASS, 오류 0건. `-culture=en`은 해당 실행에만 적용하며 사용자 설정은 유지한다.

네임플레이트 미표시와 밀집 전투의 피해 숫자 겹침은 남은 시각 확인/정비 항목이다. Computer Use 연결이 `native pipe is unavailable (os error 2)`로 실패해 실제 입력 플레이는 수행하지 못했다. 렌더링 진단 통과를 직접 플레이나 첫 마일스톤 완료로 대체하지 않는다.

## 콘텐츠 1단계 회귀 검사

`RunQA`의 기존 에셋 검사에 `ValidateContentMilestone.py`를 연결했다. 이관된 적의 역할·AI 컨트롤러·공격/표현 참조와 일반 구간의 고정 웨이브 인원 수를 검사한다. 자동 테스트에는 `PG.Content.AttackGeometry`, `PG.Content.PatternLifecycle`이 포함된다.

전체 실행 `Saved/QA/20260930T143137Z_9bb9f911/report.json`은 **PASS_WITH_WARNINGS**다. Development Editor 빌드, 자동 테스트 15개(일반 13, 경고 포함 2), 저장된 에셋, 16웨이브·7선택·6구간, 사망/재시작 20회와 정상 상태 21회를 통과했다. 실행기 판정 테스트 7개도 성공했다. 기존 엔진 API/참조 경고와 격리 테스트의 시작 장비 없는 플레이어 경고는 리포트에 남긴다.

다섯 적 역할의 예고/회복 장면은 전체 QA가 끝난 뒤 별도로 실행한다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' -B Tools/Validation/RunContentPresentation.py
```

이 검사는 저장하지 않은 임시 적을 PIE에 만들고 실제 GAS 스킬을 발동하여 상태 10개, 새 캡처 10장, 정상 종료를 요구한다. 종료 코드/엔진 오류/누락은 실패로 남긴다. 실행별 테스트 프로필, Assisted 표시와 `-culture=en`을 사용한다. 직접 입력, 자유 이동 AI의 전술, 빌드 밸런스와 패키지 검증을 대신하지 않는다. 결과와 남은 수용 기준은 [콘텐츠 계획의 1단계 실행 기록](17_content_implementation_plan.md#2026-09-30-1단계-구현-진행)을 따른다.

## 2026-10-01 콘텐츠 2단계 회귀 검사

보스 데이터 schema 2를 기존 에셋 검사에 추가했다. `PG.Content.BossPhaseLifecycle`과 `PG.Content.BossAttackSelection`이 큰 피해·재진입·공격 취소·치명 피해·2페이즈 조합을 검사하며, `PG.Stage.Lifecycle`은 격파 후 결과 표시 지연과 새 런 준비 시 타이머 취소도 확인한다.

최종 전체 실행 `Saved/QA/20261001T133944Z_4e35d90f/report.json`은 **PASS_WITH_WARNINGS**다. 빌드, PG 자동 테스트 17개(일반 14·경고 포함 3), 저장된 에셋, 16웨이브·7선택·6구간, 사망/재시작 20회와 정상 상태 21회를 통과했다. GameplayCue 검색 경로, 격리 테스트의 시작 장비, 기존 네임플레이트 직렬화 경고는 보고서에 보존한다.

최신 빌드에서 다른 QA와 겹치지 않게 보스 화면 검사를 실행한다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' -B Tools/Validation/RunBossPresentation.py
```

검사는 실제 6구간 보스의 세 공격 예고/회복, 전환, 격파, 결과 창, 재생성을 관찰하고 1280×720 현재 실행 캡처 10장을 요구한다. 체력 보정과 GAS 피해 치트를 쓰므로 `assisted=true`, `direct_input=false`다. 전환 전 파동 차단, 전환 중 공격 차단, AI의 돌진→횡베기→파동 조합, 격파 뒤 추가 판정 없음, 재생성 시 1페이즈 복귀를 함께 검사한다. 자동 통과 뒤에도 캡처의 화면 내용은 직접 검토한다. 첫 시각 검토 실패와 수정, 최종 근거는 [콘텐츠 계획의 2단계 기록](17_content_implementation_plan.md#2026-10-01-2단계-보스전-구현-진행)을 따른다.

이 검사는 직접 조작 공략, 세 빌드 밸런스, 사운드 청취, 패키지·장시간 성능 검증을 대체하지 않는다. 화면 테스트는 `-nosound`로 실행하며 음향 에셋 참조 유효성만 별도 검사한다.

## 2026-10-01 콘텐츠 3단계 회귀 검사

기존 MVP 에셋 검사에 `ValidateBuildKeystones.py`를 연결했다. 강화 21개, 신규 세 핵심의 선행 조건·선택 제한·아이콘 참조, 4·5구간의 핵심 후보 예약과 기존 7회 선택, 유효한 전투 튜닝을 검사한다. 기존 적/보스의 content schema 2와 강화의 build schema 3은 별도로 기록한다.

`PG.Content.BuildRewardRules`, `PG.Content.BuildKeystoneCombat`과 확장한 프로필 검사는 조건·시드·예약 추첨·중복 차단·튜닝 설명·추가 피해 원인·반환 횟수·실제 적 충돌 채널·약화 만료·격분 소비·장비 원복·신규 강화 직렬화를 다룬다. 전체 QA `Saved/QA/20261001T143712Z_bee5a527/report.json`은 **PASS_WITH_WARNINGS**이며, 자동 테스트 19개(일반 15·경고 포함 4), 21개 강화 에셋, 16웨이브·7선택·6구간, 사망/재시작 20회와 정상 상태 21회를 통과했다. 보강한 출혈 경계 검사를 포함한 quick QA `20261001T144624Z_0fabbc12`도 동일하게 통과했다.

최신 빌드에서 다른 QA와 겹치지 않게 세 계열의 화면 비교를 실행한다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' -B Tools/Validation/RunBuildPresentation.py
```

검사는 테스트 프로필 전용 `PGBuildScenario`로 같은 위치에 정지 표적 3개를 배치한다. 핵심 카드 3장면, 세 계열의 강화 전후 6장면, 출혈 중첩 1장면의 새 1280×720 캡처와 상태 로그·정상 종료를 요구한다. 출혈 처치 반환과 방어 약화는 합성 직접 적중 이벤트를 사용하며, 격분의 잔상은 실제 ASC 회피 스킬 활성화 경로로 확인한다. `assisted=true`, `direct_input=false`, `synthetic_hits=true`다. 시나리오 전환 때 보상 창·기존 적·스킬 쿨다운과 플레이어 위치를 정리한다.

직접 입력, 세 빌드의 보스 공략 시간·조작감·난이도, 사운드 청취를 대신하지 않는다. 캡처는 자동 통과 뒤에도 시각 검토한다. 초기 FAIL 이력과 최종 화면 검증 근거는 [콘텐츠 계획 3단계 기록](17_content_implementation_plan.md#2026-10-01-3단계-빌드-완성-강화-구현)에 남긴다.
