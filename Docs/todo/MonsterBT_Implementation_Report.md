# 몬스터 BT 개선 — 전체 역할 이관 및 위치 선정 (2026-10-03)

## 범위와 결정

Unreal Engine 5.8.2의 실제 엔진 소스와 프로젝트 코드를 기준으로 구현했다. 비동기 태스크 안정화와 사격형 시범 이관 후, 추격자·사수·수호자·분쇄자·파수꾼·보스(15101~15106) 전체를 공통 BT 실행으로 확장했다. Legacy 몬스터의 기존 BT도 비동기 태스크 수정과 주변 아군 검색 개선을 사용한다.

공격의 예고·조준 확정·판정·회복은 기존 GAS에 유지한다. BT는 공격 종료까지 기다리며, `PGCombatDirectorSubsystem`은 대상별 공격 비용·시작 간격·FIFO 대기 순서를 계속 관리한다. 피해 수치, 스킬 목록, 보스 2페이즈 순서, 웨이브 및 드랍 데이터는 이번 작업에서 변경하지 않았다.

## 조사 근거

- [Epic BT 개요](https://dev.epicgames.com/documentation/en-us/unreal-engine/behavior-tree-in-unreal-engine---overview): 이벤트 기반 실행과 서비스·관찰자 중단의 역할. 이번 구현은 Ability 종료를 이벤트로 받고 공간 판단은 낮은 주기의 서비스로 유지한다.
- [Epic 노드 인스턴스 정책](https://dev.epicgames.com/documentation/en-us/unreal-engine/behavior-tree-node-reference-in-unreal-engine): 기본 노드는 여러 AI가 공유하므로 비동기 요청의 개체별 상태를 노드 템플릿에 저장하면 안 된다.
- [Tom Looman Action Roguelike](https://github.com/tomlooman/ActionRoguelike), [공격 실행 설명](https://tomlooman.com/unreal-engine-sample-game-action-roguelike/): BT의 행동 선택과 Action의 실제 공격 실행을 분리하는 공개 Unreal 구현. 프로젝트의 GAS 경계를 유지하는 근거로 참고했다.
- [Guerrilla의 Horizon Zero Dawn AI 발표](https://www.guerrilla-games.com/read/the-ai-of-horizon-zero-dawn): HTN·Utility 및 집단 조율을 다루는 상용 사례. Unreal BT 구현 예제가 아니라 개별 판단과 집단 조율의 책임 분리를 참고했다.

## 구현

### EQS 요청 수명

`PGBTTask_FindSkillUseLocation`을 개체별 인스턴스로 실행한다. Query ID, 시작 시 타겟·SkillID를 기록하고 다른 요청의 결과, 취소 후 결과, 변경된 타겟·스킬의 결과를 수락하지 않는다. Abort, 태스크 종료, 노드 인스턴스 파괴에서 요청을 먼저 무효화한 뒤 `AbortQuery`를 호출한다. 이 순서는 동기 취소 콜백의 재진입을 막는다. 시작 실패와 빈 결과도 실패로 처리한다.

새 요청 전에 이전 목적지를 지우고 잘못된 스킬 데이터를 거부한다. EQS에 전달하는 최대 거리는 `GetPatternActivationRange()`로 통일하고 `MinDistance`도 전달한다. 기존 EQS 에셋이 새 파라미터를 참조하도록 일괄 이관하지는 않았다. 최소 거리는 거리 데코레이터 및 최종 실행 검사에서도 검증한다.

### 공격 태스크와 GAS

`PGBTTask_ExecuteSkill`은 선택한 `FGameplayAbilitySpecHandle`을 추적하고 `OnAbilityEnded`를 구독한다. 활성화 후 `InProgress`를 반환하고 완료 이벤트에서 성공·취소를 구분한다. 활성화 함수 안에서 즉시 끝나는 경우에는 반환 전 완료 통지를 유예한다. 중단 시 델리게이트를 먼저 해제하고 해당 핸들만 취소한다. 트리 종료·노드 파괴에서도 동일한 정리를 수행한다.

역할형 태스크는 기존 `TryExecuteSkill`을 통해 공격권을 요청한다. 공격권을 받지 못한 시도는 대기열 위치를 취소하지 않는다. 시작한 공격이 끝나거나 중단되면 예약을 해제한다. 공격 직전 현재 타겟의 거리·시야·생존 상태를 다시 확인한다.

기존 공통 몽타주 콜백은 정상 완료까지 `bWasCancelled=true`로 보고하고 있었다. 정상 완료/블렌드아웃은 성공, 중단/취소는 실패로 구분했다. 소환은 실제 비어 있지 않은 스폰 결과만 성공으로 끝낸다. 소환 이벤트 없이 몽타주가 끝난 경우 성공 횟수를 올리지 않는다.

### 전체 역할 BT와 편집 자산

`DT_Enemy.CombatBehaviorTree`가 역할별 트리를 지정한다. 현재 6종은 `/Game/DataCenter/AI/Combat/BT_PGCombatRole`과 `BB_PGCombatRole`을 공유하며 개체별 Blackboard와 태스크 인스턴스는 분리된다. 콘텐츠 브라우저에서 트리를 복제한 후 특정 행에 지정해 확장할 수 있다. 참조가 없으면 `PGCombatDirectorSubsystem`의 월드별·주기별 런타임 트리로 복구한다.

```text
Sequence [CombatContext Service]
  Selector
    Hold: 타겟 없음 / 전환 / 외부 공격 진행 / 제한된 후퇴 진행
    Retreat: 데이터 기반 거리·시간·쿨다운으로 제한된 후퇴
    ExecuteSkill: 공격권 요청 → Ability 시작 → 종료·취소 대기
    Approach: 주변 빈 위치 선정 → 추적 또는 공격권 대기
  Wait: DecisionInterval ± 0.02초
```

`bUseCombatBehaviorTree` 기본값은 true다. `pg.AI.UseBehaviorTree 0`은 새 적을 동일 판단 함수의 타이머 실행으로 전환하는 비교용 설정이다. 이미 실행 중인 트리에는 영향을 주지 않는다. 저장된 트리의 서비스 주기와 Wait는 BT 에디터에서 조정한다. 컨트롤러 `DecisionInterval`은 런타임 fallback/타이머 주기이며 기본 0.2초다. `MoveRetryInterval`은 0.5초다. 같은 타겟·수락 반경에 대한 정상 추적 재요청을 생략하고, 별도 동기 경로 조회 없이 partial path를 금지한 MoveTo 결과를 사용한다.

`SetCombatThinkingEnabled(false/true)`는 두 실행 방식을 중단·재개하고 실행한 공격과 공격권도 정리한다. `IsUsingCombatBehaviorTree()`로 현재 실행 방식을 확인할 수 있다.

`ConfigureCombatBT.py`는 BT/BB 생성 및 6종 연결을 수행한다. 기존 그래프·튜닝을 보존하며, 명시적 `-PGRebuildCombatBT`에만 기본값으로 재생성한다. 저장 전 원본을 `Saved/Backups/CombatBT/<시각>`에 복사한다. 커맨드렛에 화면 위젯이 없으므로 엔진 `AutoArrange` 대신 고정 구조의 노드 좌표를 지정한다. `ValidateCombatBT.py`는 새 에디터에서 실행 구조·키·그래프·6종 참조를 검사한다.

### 전투 위치와 공격 대기

`DT_Enemy.Positioning`에서 활성화와 수치를 조정한다. 현재 Separation은 140~280cm, MaxMoveDistance는 250~350cm, ReconsiderSeconds는 0.9~1.5초, MinimumImprovement는 45다. 현재 위치가 혼잡하거나 사거리/시야에 불리할 때 주변 8개 후보를 점수화한다. 사거리 오차·이동 거리·주변 적과의 간격을 비교하고 충분한 개선이 없으면 제자리를 유지한다. NavMesh 투영·시야·선택한 공격의 최소/최대 거리를 통과한 후보 중 최대 2개만 경로를 요청한다.

공격권 대기 중에도 기존 스킬과 FIFO 자리를 유지하며 빈 자리로 이동한다. 공격 시작 시 이동을 중지한다. 새 애니메이션 에셋 없이 기존 이동·대기 및 수호자의 방어 표현을 사용한다. `pg.AI.DebugPositions 1`은 채택한 목적지와 이동 선을 표시한다. `GetPositionMoveCount()`는 실제 수락된 이동 요청 수다.

지원 스킬의 아군 탐색을 `PGCombatSpatial`의 주변 Pawn 충돌 검색으로 통일했다. 반경 밖·죽은 적·중복 충돌 결과를 제외하고 HP 비율이 가장 낮은 대상을 선택한다. 선택 서비스와 실행 태스크의 월드 전체 액터 순회를 제거했다.

## 1단계 검증 기록

- 1차 Quick QA: `Saved/QA/20261003T104738Z_a01ea1ab` — 빌드, PG 자동 테스트 33개, 기존 에셋 검사 통과. 엔진 deprecated API, GameplayCue 경로, 기존 UI 직렬화 등 경고가 있어 전체 판정은 `PASS_WITH_WARNINGS`다.
- 새 자동 테스트 3개: `PG.AI.BT.AsyncQueryIsolation`, `AbilityOwnership`, `RoleTreeLifecycle`. 요청 간 격리/늦은 결과, 다른 Ability 종료 무시/정확한 취소, 엔진 BT 시작/중단/재개/UnPossess를 검사한다. 최종 테스트에는 월드 템플릿 공유와 Blackboard 분리 검사도 포함한다.
- 1차 렌더링 PIE: `Saved/QA/20261003T104900Z_d970630f_combat_bt/report.json` — 사격형 3마리의 자율 공격·회복 관찰, 공격 중 중단, 일시정지 유지, 재개, 플레이어 위치 변경 후 재공격 통과. Ability를 강제 발동하지 않고 BT가 스스로 선택하도록 했다. 플레이어 HP는 격리 프로필에서 보조했다.
- 최종 전체 QA: `Saved/QA/20261003T105109Z_b1986a95/report.md` — 빌드, 자동 테스트 33개, 에셋, 6구간·16웨이브·7회 보상 선택, 사망·재시작 20회 모두 통과(`PASS_WITH_WARNINGS`). 실제 입력 없이 보조 처치로 진행한 검사다.
- 최종 빌드의 PIE 재검사 `Saved/QA/20261003T105405Z_23d4f9e7_combat_bt/report.json`에서는 자율 전투·중단·재개·이동 시나리오는 통과했으나 에디터가 정상 종료 로그를 남긴 뒤 종료 코드 `3221225477`을 반환해 전체 결과를 **FAIL**로 보존했다. 로그에 원인을 확정할 호출 스택은 없었다. 검증 스크립트에서 에디터/PIE 액터 참조 해제·GC와 종료 요청을 분리했다.
- 수정 후 최종 PIE: `Saved/QA/20261003T105617Z_76ffdac7_combat_bt/report.json` — 같은 자율 전투·중단·재개·이동 검사 및 정상 프로세스 종료까지 **PASS**. 앞선 종료 오류의 근본 원인을 확정한 것은 아니므로 실패 기록을 함께 남긴다.

재실행:

```powershell
# 최초 연결 또는 에셋 복구(기존 편집 내용은 보존)
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'C:/UsingProject/UnrealProject/UPlayground/UPlayground.uproject' -unattended -NullRHI -EnablePlugins=PythonScriptPlugin -run=pythonscript -script='C:/UsingProject/UnrealProject/UPlayground/Tools/Validation/ConfigureCombatBT.py'
./Tools/Validation/RunQA.ps1 -Suite full
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunCombatBT.py
```

처음 빌드 시 샌드박스의 AppData 쓰기 제한으로 UBT 초기화가 실패했다. 권한을 갖춘 실행으로 해결했다. 다음 링크 시 다른 카툰 프리뷰 에디터의 DLL 잠금으로 실패했으며, 그 프로세스가 자연 종료된 후 빌드가 통과했다. 해당 실패 로그도 원래 QA 실행 폴더에 보존한다.

## 전체 역할 확장 검증

- 에디터 빌드: `Saved/QA/20261003T111846Z_50736354/build.log` 통과. 동일 실행의 자동 테스트 35개 통과. 자산 검사는 저장 전에 실행되어 실패했으며, 저장 후 아래 전체 QA에서 재검증했다.
- 전체 QA: `Saved/QA/20261003T112043Z_ab124972/report.md` — 자동 테스트 35개, 저장된 BT/BB·6종 참조, 6구간·16웨이브·7회 보상, 20회 사망/재시작 통과. 기존 경고로 `PASS_WITH_WARNINGS`. 에디터 빌드 성공 직후 동일 바이너리를 `--skip-build`로 검사했다.
- Development 게임 타깃: `Saved/QA/combat_bt_game_build.log` 통과(81.39초). 에디터 자산 생성 의존성을 게임 타깃에 포함하지 않는다. Cook/패키징 결과는 아니다.
- 전체 역할 자율 전투: `Saved/QA/20261003T112112Z_497995d7_combat_bt/report.json` — 6종 공격·회복, 보스 전환 중 취소/이후 공격, 일시정지·재개, 타겟 이동 후 공격 통과. 위치 이동은 0회여서 해당 동작의 증거로 사용하지 않는다.
- 밀집 검사 `20261003T112737Z_b423c6a3_combat_bt`에서는 위치 이동 6·2·2·3·1·3회와 전투 시나리오가 완료되었지만, Python의 정적 내비게이션 투영 호출에서 NavigationSystem CDO ensure가 발생하고 종료 코드도 비정상이어서 **FAIL**로 보존했다. 진단용 정적 호출을 제거하고 실제 수락된 MoveTo와 렌더링 증거로 검증하도록 수정했다. PIE 전 에디터의 내비게이션 빌드 완료를 기다린다.
- 추가 자동 테스트 `PG.AI.BT.PositionStability`는 불필요한 선회 억제·밀집 시 빈 측면 선호를 검사한다. `EditableGraph`는 화면 없는 환경의 그래프 생성·재생성 및 잘못된 주기 거부를 검사한다.
- **최종 렌더링 검사**: `Saved/QA/20261003T113726Z_8b68b0f1_combat_bt/report.json` — **PASS**, 오류/ensure 없음, 정상 프로세스 종료. 6종의 공격 시작은 6·5·5·5·4·4회, 회복은 3·4·5·5·3·3회, 위치 이동은 5·2·0·2·3·1회였다. 혼잡하지 않은 역할은 제자리를 유지할 수 있다. 보스 2페이즈·중단/재개·타겟 이동 모두 통과했고 `autonomous.png`, `boss_phase_two.png`를 보존했다. 불필요한 RiderLink와 Zen 의존성을 검증 실행에서 제외했으며 앞선 비정상 종료의 단일 원인을 확정한 것은 아니다.

### 50마리 성능 비교

`RunGuardianBenchmark.py --seconds 120 --fps-cap 0`을 두 번 순차 실행했다. 1280×720, 같은 시드와 5→50→5마리 구성으로 비교했고, 측정 시작/도중 확인에서 별도 Unreal Editor는 없었다. 양쪽 모두 오류 0, 공격·회복과 종료 정리 통과(`PASS_WITH_WARNINGS`, 20분보다 짧은 진단이라는 경고)다.

| 50마리 구간, ms | 타이머 | BT |
|---|---:|---:|
| GameThread 평균 | 7.041 | 6.671 |
| GameThread p95 | 9.385 | 9.098 |
| Frame 평균 | 9.334 | 10.018 |
| Frame p95 | 11.013 | 11.898 |
| GPU 평균 | 8.211 | 9.044 |

타이머 기준은 `Saved/QA/20261003T113049Z_f74438ae_guardian_native/report.json`, BT는 `Saved/QA/20261003T113432Z_91670442_guardian_native/report.json`이다. 각각 `--engine-setting pg.AI.UseBehaviorTree=0/1`을 사용했다. 로그의 BT 시작 수는 0/60으로 실제 실행 방식 차이도 확인했다. CPU 평균은 약 5.3% 낮지만 전체/GPU 프레임은 높아, **단회 샘플로 성능 향상이나 인과관계를 단정하지 않는다**. 고정 플레이어와 스크립트 배치를 사용한 실제 AI/GAS 부하 검사이며, 자유 내비게이션·실제 입력·패키지·20분 안정성 검사가 아니다.

## 검증 한계

- Windows 직접 입력은 computer-use 초기화·재시도·세션 재초기화 후에도 native pipe 연결 실패(os error 2)로 실행하지 못했다. 렌더링 PIE와 자동 진행은 실제 키보드·마우스 조작감/난이도 수용 검사와 구분한다.
- 기존 EQS 에셋의 MinDistance 파라미터 연결은 별도다. 태스크와 최종 공격의 범위 검사가 잘못된 실행을 차단한다. 새 역할 BT의 위치 선정은 별도 공간 후보 평가를 사용한다.
- 새 대기 애니메이션이나 아트는 추가하지 않았다. 기존 이동·방어 표현의 역할별 폴리싱은 추가 개선 영역이다.
