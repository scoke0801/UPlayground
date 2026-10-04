# 핵 앤 슬래시 스킬 P0 구현 기록

2026-10-04 / Unreal Engine 5.8

## 범위와 판정

[설계 원본](../Design/HackSlashSkills/HackSlashSkills_Spec.md)의 첫 구현 MUST 범위인 기본 공격 100/101/102, 질풍연참 111, 원월참 112를 데이터와 런타임에 연결했다. 기존 회피와 GAS 피해 경로를 유지한다. **구현·자동 검증 단계이며 P0 직접 플레이 수용 완료는 아니다.** P1의 110/113/114 이관·자유 장착 UI, P2 장비 변형, 후속 모션 교체는 이번 범위에 포함하지 않는다.

기존 작업 트리와 Content의 변경은 보존했다. 원본 몽타주를 덮어쓰지 않고 프로필의 논리 시간 → 몽타주 위치 매핑으로 기존 애니메이션을 샘플링한다. 새 모션 후보의 최종 채택이나 연속 재생 품질 검수 완료를 의미하지 않는다.

## 구현

- `PGData/UPGPlayerSkillProfile`: 타격 창, 부채꼴·원형 범위, 피해 배율, 발동 자격, 이동 구간, 취소 시점, 포즈 매핑과 표현 참조. 유효성 검사에 실패한 연결 프로필은 시전을 거절한다. 프로필이 없는 스킬은 기존 경로를 사용한다.
- `PGActor/UPGPlayerAttackComponent`: 단일 논리 시계로 판정·이동·포즈를 진행한다. 120Hz 분할과 타격/이동 경계 분할로 긴 프레임의 창 누락을 줄인다. 몽타주 Notify·무기 충돌은 프로필 피해를 중복 실행하지 않는다. 비활성 상태의 Tick은 꺼진다.
- 공간 판정은 발 위치·적 캡슐 반경, 수직 허용 150cm, 벽 차폐를 사용한다. 대상 수 제한은 없다. 이동은 캡슐 sweep 및 지면 검사로 제한한다. 기존 Enemy 프로필이 Player와 Overlap이므로 별도의 살아 있는 적 캡슐 검사를 추가했다. 현재 111은 일반 적까지 통과하지 않는 보수적 정책이다.
- `PGShared/FPGSkillCastContext`와 GAS: CastId·원래 SkillID·공격력 스냅샷·약한 대상 참조, 타격별 중복 방지, 시전당 충격파 1회·격분 최대 3·원래 SkillID의 남은 쿨다운 35% 반환 1회. 출혈 강화의 마지막 타격 자격을 명시적으로 전달한다. 추가 피해는 기존 재귀 방지 경로를 유지한다.
- 입력 버퍼는 월드 게임 시간 기준 0.18초이며 회피 우선이다. 프로필 기본 콤보는 회피로 기존 만료 시간을 연장하지 않고 유지한다. 사망·UI·포커스 상실 시 입력과 콤보를 정리한다. 로컬 히트스톱은 논리 시계·이동·검흔 표시를 멈추며 쿨다운과 입력 버퍼는 계속 흐른다.
- 공격 종료·취소는 이동 속도, 루트 모션 모드, 히트스톱, 무기 충돌과 표시를 정리한다. 초기화 시 현재 로드아웃의 프로필·표현 리소스를 미리 로드한다.

| ID | 피해 배율 | 반경 / 전체 각도 | 타격 시점 | 지속 시간 | 이동 | 쿨다운 |
|---|---|---|---|---|---|---|
| 100 | 0.9 | 250 / 120° | 0.16 | 0.48 | 전진 20cm | 0 |
| 101 | 1.0 | 280 / 150° | 0.19 | 0.50 | 전진 25cm | 0 |
| 102 | 1.5 | 320 / 170° | 0.30 | 0.68 | 전진 60cm | 0 |
| 111 | 0.9 × 2 | 240 / 100° | 0.18, 0.40 | 0.72 | 전진 450cm | 3초 |
| 112 | 1.0 × 2 | 320 / 360° | 0.20, 0.48 | 0.76 | 이동속도 60% 보행 | 4초 |

거리 단위는 cm, 시간은 실효 속도 1.0의 논리 초다. 속도 범위는 0.75~1.75이며 전진 총거리는 속도에 따라 변하지 않는다. 기존 빌드 프리셋에 명시된 쿨다운 조정은 유지하므로 표는 프로필 기본값 및 격리 로드아웃 기준이다.

## 에셋과 이관

- 제작 수치: `Tools/Validation/HackSlashP0.json`.
- 제작 도구: `ConfigureHackSlashP0.py`. `/Game/DataCenter/HackSlashP0/DA_PlayerSkill_100`, `101`, `102`, `111`, `112`와 `M_PlayerSlash`를 생성한다.
- `DT_Skill`의 해당 다섯 행에서 `PlayerProfile`, `SkillCoolTime`, `Desc`만 이관한다. 다른 행과 필드의 보존은 원본 JSON 비교로 검사한다.
- 이관 전 백업: `Saved/Backups/HackSlashP0/20261004T060946315194Z/`. `Saved/HackSlashP0_LastBackup.txt`가 검사 기준 백업을 가리킨다. 재이관은 현재 테이블을 다시 백업하므로 최초 기준선 보존에 유의한다.
- 현재 검흔은 재사용하는 평면 메시와 민트/라벤더 가산 머티리얼이다. 소리는 기존 `CombatCycle/S_Normal`을 연결했다. Niagara·사운드 훅은 제공하지만 최종 리본, 전진 잔상, 스킬별 제작 음향은 아직 아니다.
- 회피 점검 결과: ID 10000, 쿨다운 0, 몽타주 길이 약 1.167초, RateScale 1, 레벨 1 거리 곡선 100cm. 검사한 Ability/몽타주에는 무적 태그·Notify가 없었다. 새 무적 규칙을 추가하지 않았다. 세부 덤프는 `Saved/QA/HackSlashP0/preserved_settings.json`에 있다.

## 검증 근거

`RunHackSlashP0.py --render`는 Development 빌드 → 저장 에셋 재로드 검사 → 전체 `PG.` 자동 테스트 → 실제 GAS 공간 판정 → 오프스크린 렌더링을 실행한다. 각 단계의 종료 코드·오류·완료 표식을 검사한다. `assisted=true`, `direct_input=false`, `contact_injected=false`, `p0_acceptance_complete=false`를 보고서에 명시한다.

- 전체 자동 테스트: **39개 통과(33개 성공, 6개 경고 포함 성공), 실패 0**. 새 프로필 유효성, 피해/발동 제한, 공간/시계/취소 테스트 3개를 포함한다. 네이티브 테스트 픽스처의 시작 무기 미설정 경고 등이 남는다.
- 새 검사: 범위 경계·높이·벽 차폐, 타격 중복 방지, 공격력 스냅샷, 마지막 타격 출혈, 충격파 자격, 격분 상한, 원래 ID 쿨다운 반환 1회, 취소 후 정리, 0.75/1/1.75 속도의 450cm 전진, 벽·Overlap 적 몸체 정지.
- 실제 공간 프로브: 접촉 이벤트 주입 없이 공격력 100·방어 0인 격리 적을 배치하고 기존 Ability를 실행했다. 기본 공격의 전방 피해는 90/100/150·후방 0, 111은 서로 다른 두 위치의 적에게 각 90, 112는 전후방 각각 200이었다. 전진은 20/25/60/450cm, 112 무입력 이동은 0이었다.
- 저장 에셋 검사: 다섯 프로필 수치·참조와 비대상 행 보존 통과. `ValidatePlayerAttacks.py`는 프로필 행과 기존 행을 구분해 검사한다.
- 기존 공격 회귀: `Saved/QA/20261004T063955Z_3d876a_player_attacks/report.json` PASS. 해당 테스트 전용 프로필에서만 런타임 캐시의 새 프로필 참조를 비우고 기존 Notify·접촉 주입 경로를 검사한다. 에셋을 저장하지 않으며 신규 공간 검사의 대체 근거로 사용하지 않는다.
- 모듈 검사: `AuditModuleDependencies.py` 통과. PGData/PGShared에 PGAbilitySystem 의존성을 추가하지 않았다.
- 렌더링: D3D12 SM6, 1280×720 오프스크린 캡처. `Saved/QA/HackSlashP0/Skill_100.png` 등 다섯 장. 정지 화면은 애니메이션 접촉 동기화·입력감·프레임 성능의 통과 근거가 아니다.

최종 실행: `Saved/QA/20261004T064121Z_c7ff01_hack_slash_p0/report.json` — build/assets/automation/spatial/render 모두 PASS. 첫 기본공격의 검흔도 히트스톱 이후 캡처에서 확인했다. 이전 실행 `20261004T063610Z_a1ccc6_hack_slash_p0`에는 표시 시간이 월드 시계로 먼저 끝나는 첫 타격 캡처 문제가 있었으며, 최종 실행은 논리 시계로 보정한 결과다.

## 재현

엔진 내장 Python으로 `Tools/Validation/RunHackSlashP0.py --render`를 실행한다. 기존 경로 회귀는 `RunPlayerAttacks.py`, 읽기 전용 보존값 확인은 UE Python에서 `InspectHackSlashP0.py`다.

직접 비교 준비는 RogueArena를 `-PGTestProfile=HackSlash_Manual`로 실행한 뒤 콘솔 `PGHackSlashLoadout`을 사용한다. 기존 액티브 슬롯에 111+112를 런타임 장착하며 저장하지 않는다. 일반 플레이 저장에는 적용되지 않도록 테스트 프로필 이름을 검사한다. `pg.Skill.DebugShapes 1`은 범위를, `pg.Skill.DebugCast 1`은 시전·타격 로그를 표시한다. `PGHackSlashProbe`는 격리 자동 검사 후 프로세스를 종료하므로 직접 플레이용으로 실행하지 않는다.

## 남은 수용 기준

1. 설계의 P0-M10/M15/RING/E1 각 5시드 전후 비교, 실제 입력·무보조 영상·실패 기록과 피해/처치 시간 측정.
2. 타격 프레임과 칼날 자세의 연속 재생 검수, 111의 두 검흔 각도·잔상, 112의 입력 보행, 회피 후 콤보 유지와 UI/포커스 복귀를 직접 조작으로 확인.
3. 모서리·다른 층·경사·낭떠러지·밀집 적에서 이동과 차폐 검수. 현재 일반 적까지 진로를 막는 정책의 전투감 판단.
4. 최종 Niagara/SFX·카메라 피드백과 밀집 전투 가독성 조정. 자동 렌더 성공을 연출 완성 판정으로 사용하지 않는다.
5. 패키지 1080p 20분 성능 및 전체 런 밸런스. 이번 검사는 전체 RunQA full의 장시간 진행·재시작 검사를 다시 수행한 것이 아니다.

이 항목을 통과하기 전에는 설계 수치를 최종 밸런스나 P0 출시 준비 완료로 표시하지 않는다.

## 2026-10-04 후속 — P0 전투 비교 도구

설계 14장의 **단일 정예와 혼합 무리 결과를 통과한 뒤 P1로 확장** 조건에 따라, 이번 후속은 실제 비교를 재현하고 실패까지 기록하는 실행 도구를 추가했다. **P0 직접 플레이 수용 완료나 P1 구현 완료가 아니다.** 스킬 밸런스·몽타주·Content 에셋은 이번 후속에서 변경하지 않았다.

### 구현과 비교 조건

- `Source/UPlayground/Cheat/PGSkillScenario.cpp`: `PGSkillScenario <id> <seed> <baseline|p0>`. `HackSlash_` 테스트 프로필과 Development에서만 실행하며, 한 월드에서 한 번만 실행한다. 각 시험은 새 프로세스를 사용한다.
- 기존 스테이지의 스폰·보상 타이머와 적을 정리한 뒤 RogueArena의 실제 내비게이션 위에 적을 배치한다. 웨이브 시작을 우회할 때 비어 있던 동적 내비게이션을 기존 `PrepareNavigationForWave` 경로로 초기화한다. 투영·충돌·AI 연결에 실패하면 시험을 실패로 기록한다.
- M10은 15101 × 10 + 15102 × 2, M15는 15 + 3, RING은 사방 근접 12 + 사수 2, E1은 정예 15104 × 1이다. AI·GAS·공간 판정은 기존 경로를 사용하며 접촉 주입·무적·전투 중 회복은 없다. 적의 드랍은 꺼 둔다.
- 첫 비교는 강화 없이 공격력 100, 플레이어 HP 1000·방어/치명타율 0, 근접 HP 250·사수 HP 200·정예 HP 5000·적 방어 0으로 고정한다. 이 값은 **QA 비교 조건**이며 라이브 밸런스의 확정값이 아니다. 100+111+112 및 기존 회피를 장착한다. 기존 적 공격력·AI 수치·메시·카메라는 유지한다.
- 시드 173001~173005는 초기 배치의 작은 편차를 고정한다. 실시간 AI의 모든 난수·경로·공격 순서를 완전히 같은 궤적으로 재생한다는 보장은 아니다.
- `baseline`은 최초 이관 백업 `Saved/Backups/HackSlashP0/20261004T060946315194Z/skills.json`에서 다섯 행 전체를 메모리에 복원한다. 빈 `PlayerProfile`과 다섯 ID를 검사하며, 파일 SHA-256과 행 내용을 각 manifest에 보존한다. 디스크의 DT_Skill을 저장하거나 되돌리지 않는다. 이것은 **현 런타임에서 이관 전 스킬 데이터 경로 비교**이며 과거 실행 파일 전체 복원은 아니다.
- 누적 유효 피해·피격량은 Health 변경 이벤트로 계산한다. 적별 HP 감소 시각·사망, 사수 첫 피해 시각, 스킬 사용 ID/시각을 남긴다. 일반전은 최대 60초, E1은 30초까지 관찰하고 조기 처치·사망·시간 초과를 각각 기록한다. 프로세스 중단과 불완전 로그도 FAIL로 보존한다.
- 종료 시 입력 버퍼·활성 Ability와 적 행동을 중지하고 관측 델리게이트를 해제한다. 런처는 결과를 저장하고 게임을 종료한다. 테스트 프로필의 Assisted 표식과 `combat_assistance=0`을 구분한다. 실제 입력 영상 확인 전까지 `direct_input_verified=false`, `p0_acceptance_complete=false`다.

### 실행

저장소 루트의 PowerShell에서 실행한다. `.ps1`은 `.uproject`의 버전에 맞는 엔진 내장 Python을 찾아 사용한다. C++ 변경 후에는 먼저 `UPlaygroundEditor Win64 Development`를 빌드한다.

```powershell
# 40회 직접 비교 목록 생성: 게임을 실행하지 않음
./Tools/Validation/RunHackSlashComparison.ps1 --prepare

# 같은 시드의 전후 시험을 각각 직접 조작. READY 이후 기존 입력 사용.
./Tools/Validation/RunHackSlashComparison.ps1 --scenario P0-M10 --seed 173001 --variant baseline
./Tools/Validation/RunHackSlashComparison.ps1 --scenario P0-M10 --seed 173001 --variant p0

# 시나리오 4종의 전후 배치/실행 점검: 직접 플레이 결과에서 제외
./Tools/Validation/RunHackSlashComparison.ps1 --smoke --all

# 포위 장면의 1080p 오프스크린 캡처
./Tools/Validation/RunHackSlashComparison.ps1 --smoke --render-smoke --scenario P0-RING

# 성공·실패를 모두 집계. 자동 점검은 직접 비교 40칸에서 제외.
./Tools/Validation/RunHackSlashComparison.ps1 --summarize Saved/QA
```

결과는 고유 `Saved/QA/<시각>_<ID>_hack_slash_comparison/` 아래에 쌓인다. `manifest.json`은 실행 명령·기준선·P0 명세 해시, `trial.log`는 이벤트, `report.json`은 해당 시도의 기록이다. `matrix.json`의 시나리오/시드/변형별 명령을 따라 실행한다. 영상은 별도로 녹화하고 해당 trial에 연결해야 하며 도구가 입력이나 영상을 자동 검증했다고 표시하지 않는다. `comparison-summary.json`은 재시도를 포함한 모든 직접 시험과 제외한 smoke 경로를 보존하며 자동으로 수용 완료를 선언하지 않는다.

### 검증 근거와 잔여 항목

- UE 5.8 Development Editor 빌드 통과: `Saved/QA/HackSlashComparison_Build.log`. 기존 엔진 API 폐기 예정·선호 도구 체인 경고는 남는다.
- 기존 P0 build/assets/automation/spatial 모두 PASS: `Saved/QA/20261004T065155Z_c001e4_hack_slash_p0/report.json`. 자동 테스트 39개, 실패 0. 이 실행은 비교 도구의 초기 추가 시점 회귀이며 후속 진단 수정은 최종 빌드와 시나리오 점검으로 검증했다.
- 시드 173001의 4종 × 전후 2종 **8회 무입력 실행 정상 종료**, 적 종류·HP·좌표 전후 일치: `Saved/QA/20261004T065651Z_e71f78_hack_slash_comparison/report.json`. 적 공격에 의한 실제 HP 감소와 요약 대조를 확인했다. 공격 입력을 하지 않았으므로 플레이어 피해량·스킬 사용은 0이며 전투 밸런스 근거가 아니다.
- `TestHackSlashComparison.py`의 11개 검사 통과: 사망/시간 초과 보존, 중단·합계 불일치·다른 시드·잘못된 런타임 변형·중복 경계·가짜 클리어·짧은 관찰 거절, 기준선 확인, 집계 시 실패 보존 및 smoke 제외. 모듈 의존성 검사 통과.
- 최종 파서로 기존 8개 실제 로그를 다시 읽어 모두 통과: 같은 실행 폴더의 `final_parser_check.json`. 집계에서는 8개 smoke를 제외하고 직접 시험 40칸 모두 PENDING임을 확인했다.
- 최종 포위 장면 캡처는 **1920×1080 PNG**이며 실제 적 배치·기존 캐릭터·HUD를 열어 확인했다: `Saved/QA/20261004T070244Z_03ef97_hack_slash_comparison/P0-RING_173001_p0_f7d609fb/scene.png`. 해당 실행의 결과는 RECORDED, 오류 0이다. 첫 렌더 `20261004T070017Z_f0e646_hack_slash_comparison`은 저장된 창 크기 때문에 888×500이었으므로 1080p 근거에서 제외한다. `-ForceRes`와 PNG 헤더의 실제 해상도 검사로 수정했다. 장면의 기존 반사 캡처 경고는 남아 있고 연출·성능 완료를 뜻하지 않는다.
- 첫 실행 `20261004T065415Z_dda515_hack_slash_comparison`은 내비게이션 초기화 누락으로 실패했고 그대로 보존했다. 이후 정상 실행으로 수정 확인했다.
- 직접 시험 40회는 아직 PENDING이다. 스킬별 유효 적중/시전 비율, 첫 입력→타격 지연, 111 이동거리·2타 성공률, 112 중 피격·이탈, 강화 3계열·발동 상한의 통합 기록은 후속 검증이 필요하다. 현재 Health 로그만으로 피해를 특정 시전이나 강화에 귀속시키지 않는다.
- 연속 모션·VFX/SFX 품질, 경사/층/낭떠러지 이동, 실제 입력·포커스 복귀, 1080p 패키지 20분 성능과 전체 런 밸런스는 미완료다. 이번 도구 실행과 정지 캡처로 대체하지 않는다.
