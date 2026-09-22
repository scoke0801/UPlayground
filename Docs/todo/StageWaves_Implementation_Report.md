# 웨이브 스테이지와 빌드 시간

## 진행 규칙

`웨이브 1 → 전멸 → 웨이브 대기 → 웨이브 2 … → 마지막 웨이브 전멸 → 빌드 시간 → 다음 스테이지`

- 현재 웨이브의 스폰 대기열과 추적 중인 적이 모두 비어야 웨이브를 완료한다. 기본 설정에서는 소환된 적도 포함한다.
- `OnStageCompleted`와 `OnAllMonstersKilled`는 마지막 웨이브 완료 시 한 번 발생한다. 보상 선택은 스테이지 클리어 판정과 분리한다.
- 마지막 웨이브 완료 시 빌드 타이머와 보상 선택을 시작한다. 보상을 일찍 골라도 타이머를 초기화하거나 단축하지 않는다.
- 빌드 시간 기본값은 30초다. 종료 시 미선택 보상은 첫 번째를 자동 선택한다. 보상이 없는 경우에도 체크포인트 지급 경로를 거친다.
- 저장/보상 확정 실패 시 다음 스테이지로 넘어가지 않고 실패 상태로 전환한다. 중복/이전 토큰은 거부한다.
- 빌드 중 장비 창은 게임을 일시정지하지 않는다. 시간이 끝나면 보상/장비 창을 닫고 다음 전투를 시작한다. 전투 중 장비 창의 기존 일시정지는 유지한다.
- 마지막 스테이지도 보상과 빌드 시간을 마친 뒤 `Finished`로 전환한다. 사망/재시작/월드 종료 시 타이머를 정리한다.

## 에디터 데이터

`/Game/DataCenter/DataTables/Stage/DT_StageData`에서 설정한다.

| 필드 | 의미 |
|---|---|
| `Waves` | 순서대로 진행할 웨이브 목록 |
| `Waves[].MonsterSpawnInfos` | 해당 웨이브의 몬스터 ID·수량·우선순위·스폰 지연 |
| `Waves[].StartDelay` | 이전 웨이브 전멸부터 시작까지 대기 시간. 첫 웨이브는 스테이지 시작 기준 |
| `BuildDuration` | 마지막 웨이브 전멸부터 다음 스테이지까지 빌드 시간 |
| `SpawnInterval`, `SpawnBatchSize` | 각 웨이브 안에서의 배치 스폰 간격과 상한 |
| `bCountSummonedEnemies` | 소환 적을 전멸 조건에 포함. 기본값 true |

`SpawnDelayTime`은 각 웨이브가 실제 시작한 시점 기준이다. `RemainingMonsters`는 현재 웨이브의 미스폰+생존 적 수, `SpawnedMonsters`는 스테이지 누적 기본 스폰 수다.

`Waves`가 비어 있는 기존 데이터는 `MonsterSpawnInfos`를 단일 웨이브로 읽는다. `NextStageDelay`와 예전 enum 값은 직렬화 호환을 위해 보존하며 새 진행에는 사용하지 않는다.

`Tools/Validation/ConfigureStageWaves.py`는 기존 몬스터 총량과 목록 순서를 유지하여 최대 3개 웨이브로 균등 분할한다. 나머지 수량은 뒤쪽 웨이브에 배정한다. 첫 웨이브는 즉시, 이후는 2초 대기한다. 웨이브 내부 상대 스폰 지연은 보존한다. 이미 작성된 Waves는 덮어쓰지 않는다(`-PGRebuildStageWaves`를 명시하면 기존 목록에서 재생성). 원본 에셋/JSON과 이관 요약은 `Saved/Backups/StageWaves`에 저장한다.

## 표시와 디버깅

- 메인 HUD: 현재/전체 웨이브, 현재 웨이브 남은 적, 다음 웨이브 대기, 빌드 카운트다운.
- 보상 창: 빌드 카운트다운과 미선택 자동 처리 안내.
- `PGStageStatus`: 스테이지, 상태, 웨이브, 남은 적, 누적 스폰, 빌드 잔여 시간.
- `PGStartStage <ID>`: 해당 스테이지 첫 웨이브부터 재시작.

## 검증

`PG.Stage.DataValidation`과 `PG.Stage.Lifecycle`은 빈 웨이브, 잘못된 시간, 총량 overflow, 미스폰/소환 적 잔존, 웨이브 전환, 중복 처치, 빌드 타이머 유지, 이전 보상 토큰, 빌드 중 사망/오래된 콜백을 검사한다.

### 적용한 실제 데이터

| 스테이지 | 웨이브별 기본 몬스터 수 | 빌드 시간 |
|---|---|---|
| 1 | 1 / 1 / 1 | 30초 |
| 2 | 1 / 1 / 2 | 30초 |
| 3 | 1 / 1 / 1 | 30초 |

기존 총량을 보존한 초기 구성이다. 실제 전투 밀도와 난이도는 별도 플레이 밸런싱 대상이다. 기존 전투 표현 프리셋 생성기 `ConfigureCombatCycleAssets.py`도 `Waves`를 쓰도록 갱신했지만, 이번 작업에서 해당 프리셋을 다시 적용하지는 않았다.

### 실행 근거

- UE 5.8 Development Editor 빌드 성공: `Saved/Logs/WaveBuild.log`.
- 스테이지 자동 테스트 2개 성공, 실패 0: `Saved/Automation/StageWavesFinal/index.json`.
- 데이터 이관 및 JSON 왕복 검증 성공. 저장 후 재실행에서 `changed=False`: `Saved/Logs/WaveMigrationRepeat.log`.
- 실제 StageDevMap + 격리 테스트 프로필에서 1/2스테이지의 각 3개 웨이브, 30초 빌드, 다음 스테이지 시작 확인: `Saved/Logs/WaveRuntimeFinal.log`. 보상을 클리어 약 3초 후 골라도 다음 스테이지는 클리어 약 30초 후 시작했다.
- 미선택 자동 보상 후 진행은 첫 실행의 1/2스테이지에서 확인: `Saved/Logs/WaveRuntime.log`. 이 실행은 기존 테스트 도구의 100초 제한에 걸렸으며, 도구 제한을 600초로 늘렸다.
- 3스테이지 사망 VFX에서 NullRHI가 반환한 null Niagara 컴포넌트 접근 크래시를 발견하여 `PlayDeathDissolveVFX`에 생성 성공 검사를 추가했다. 사망 디졸브/클리어 로직은 계속 진행한다.
- 수정 후 저장된 3스테이지 체크포인트에서 3개 웨이브, 보상 1회, 30초 빌드, 최종 `Finished`까지 성공: `Saved/Logs/WaveRuntimeStage3.log`의 `PGCombatCycle COMPLETE rewards=1`. 앞선 1/2스테이지 실행과 합쳐 총 9개 웨이브의 진행을 확인했다.

NullRHI 검증은 실제 HUD 가독성이나 전투 난이도 검증을 대신하지 않는다. 보상 창/장비 창의 실제 조작·시각 확인, 웨이브별 전투 밀도 조정은 에디터 플레이에서 확인해야 한다.
