# HackSlash P1 구현 및 수용 상태

## 2026-10-05 액티브 스킬 속도 재조정

기본 3타의 속도/끊김 해결 후에도 액티브 스킬이 빠르다는 보고에 따라 저장 에셋을 확인했다. AttackSpeed는 1이지만 포즈 매핑이 원본 구간을 평균 1.43~1.67배, Hermite 보간 순간 최대 1.77~1.87배로 압축하고 있었다. 프로필 경로는 몽타주를 멈추고 위치를 직접 갱신하므로 기존 몽타주 RateScale을 바꾸는 방식으로는 해결되지 않는다.

액티브 5종의 논리 시간축을 1.5배로 늘렸다. 원본 포즈 위치를 보존하고 타격 창·이동·조준 확정·회피/공격 취소 시각도 함께 늘려 포즈와 판정을 맞춘다. 근접 판정 창은 0.06→0.09초이며 착지/발사 단발 판정은 그대로다. 기본 3타, 히트스톱, 피해, 거리, 쿨다운과 검기 비행 설정은 유지한다. 격분 가속은 기존대로 별도 적용된다.

| 스킬 | 전체 재생 시간 전→후 | 타격 시각 후 | 원본 대비 순간 배속 전→후 |
|---|---|---|---|
| 110 낙성참 | 1.55→2.325초 | 1.32초 | 1.821→1.214 |
| 111 질풍연참 | 1.35→2.025초 | 0.48 / 1.20초 | 1.865→1.243 |
| 112 원월참 | 1.45→2.175초 | 0.54 / 1.41초 | 1.858→1.239 |
| 113 파쇄연격 | 2.65→3.975초 | 0.30 / 1.08 / 1.98초 | 1.768→1.179 |
| 114 관통검기 | 1.40→2.10초 | 0.90초 | 1.803→1.202 |

`HackSlashP0.json` / `HackSlashP1.json`이 원본이다. `ConfigureAttackMotion.py`를 Unreal Python commandlet에서 `-PGActiveSkillTempo`와 함께 실행하면 액티브 프로필만 갱신한다. 절대 시각을 대입하므로 재실행해도 누적 감속되지 않는다. 전체 P0/P1 재생성 도구도 새 조준 시각과 판정 창을 읽는다. 검증은 키 간 평균 속도뿐 아니라 Hermite 도함수의 극값까지 검사하여 기본 논리 속도에서 1.35배를 넘는 압축을 차단한다. `RunHackSlashMotion.py`도 실행 중 논리 시각/몽타주 위치 차이로 이 상한을 검사한다.

변경 전 에셋은 `Saved/Backups/AttackMotion/20261005T013951260756Z`에 보존했다. `Saved/QA/SkillTempo/before.json`과 8개 `.copy`는 실제 변경 전 포즈/몽타주 기록이다. 최초 적용 자체는 성공했지만 보존 검사에서 proc_policy 구조체의 주소가 포함된 문자열을 비교해 실패했다(`apply.log`). 구조체 값 export_text 비교로 고친 재적용/동일성 검사는 통과했다(`apply2.log`, `preservation.json`). 기본 프로필 3개·몽타주 8개·테이블·카탈로그의 파일 해시를 보존했고, 재적용 중 피해/이동 거리/발동 정책/원본 포즈/투사체/표현 설정이 유지됨을 검사했다.

검증 결과:

- `Saved/QA/20261005T014055Z_6e1ca1_hack_slash_p1/report.json`: 새 프로세스 에셋 재로드, PG 자동 테스트 45개(성공 38·경고 포함 성공 7·실패 0), P0/P1 총 8개 공격의 실제 GAS 공간 검사 PASS. 피해량·이동 거리·장착/쿨다운 회귀 포함.
- `Saved/QA/20261005T014358Z_c75f8f_attack_motion_active_tempo/report.json`: P0 렌더 포즈 기록 완료, 111/112의 런타임 최대 포즈 배속 1.243/1.238. 기본 3타 포함 논리 시계 진행 중 포즈 정체 샘플 0개.
- `Saved/QA/20261005T014521Z_c0427e_attack_motion_active_tempo/report.json`: P1 렌더 포즈 기록 완료, 110/113/114의 런타임 최대 포즈 배속 1.214/1.178/1.202. 두 렌더 실행의 8개 공격·896개 포즈 샘플 중 논리 시계 진행 샘플 851개에서 포즈 정체 0개, 액티브 속도 상한 초과 0개. 보고서 상태 `RECORDED`는 기록 및 회귀 검사 완료이며 모션 품질의 직접 수용 판정은 아니다.
- 변경 Python 5개 구문 검사와 `git diff --check` 통과. 런타임 C++ 변경이 없는 데이터/도구 수정이므로 이번에는 재빌드·재패키징하지 않았다. 기존 패키지에는 새 에셋이 반영되지 않는다.

720p/60fps 상한의 프로그램 입력·오프스크린 렌더 검증이며, 직접 조작 체감·청취·장시간 성능 수용은 별도다. 특히 길어진 파쇄연격의 회복 시간과 회피/연계 창은 실제 전투에서 추가 체감 조정할 수 있다.

아래는 이전 P1 구현 및 검증 기록이다.

작성: 2026-10-04. 범위는 P2 직전의 P1-A/P1-B 구현이다. P2 장비 변형은 추가하지 않았다.

## 2026-10-04 스킬 속도·콤보 전환 후속 수정

기본 3타만 느려진 뒤에도 액티브 스킬의 압축과 콤보 경계의 끊김이 보고되어 다음을 수정했다. 아래 수치를 원본 JSON과 실제 에셋에 반영하고 Development·DebugGame 구성을 빌드했다.

| 스킬 | 기존 지속 시간 | 변경 지속 시간 | 변경 타격 시점 |
|---|---:|---:|---|
| 낙성참 110 | 0.90 | 1.55 | 0.88 |
| 질풍연참 111 | 0.72 | 1.35 | 0.32 / 0.80 |
| 원월참 112 | 0.76 | 1.45 | 0.36 / 0.94 |
| 파쇄연격 113 | 0.92 | 2.65 | 0.20 / 0.72 / 1.32 |
| 관통검기 114 | 0.58 | 1.40 | 0.60 |

- 각 타격의 원본 자세를 유지하면서 준비·회수 구간을 재배분했다. 113의 최대 약 9배속 구간을 포함해 모든 프로필 구간의 평균 포즈 진행률을 2.1 이하로 검사한다. 피해·쿨다운·전진 총거리·광분 규칙은 유지한다. 단위는 속도 1.0의 논리 초다.
- 프로필 몽타주는 처음부터 원본 시작 포즈·재생률 0으로 시작한다. 종료·취소 시 원본 배속으로 다시 재생하지 않고 마지막 샘플 포즈로 블렌드 아웃한다. 인스턴스의 자동 블렌드 아웃을 꺼 논리 시계와 자연 종료의 경쟁도 제거한다.
- `ConfigureAttackMotion.py`는 기존 8개 프로필과 플레이어 몽타주를 백업한 뒤 시간 및 양방향 0.16초 Cubic 블렌딩만 적용한다. 기존 P0/P1 생성 도구도 같은 블렌딩 함수를 사용한다. 원본 시퀀스와 스킬 테이블은 변경하지 않는다.
- `PGHackSlashComboProbe` 실행 플래그와 `RunHackSlashMotion.py --combo`는 실제 유지 입력 100→101→102→100, 교차 블렌딩 가중치, 이전 포즈 고정, 해제 후 종료를 검사한다. 기존 단발 공격 검사는 콤보 경계의 검증 근거가 아니었다.
- Development 빌드 성공: `Saved/Logs/AttackMotionBuild.log`. C++ 자동 테스트 45개 통과(38 성공·7 경고 포함 성공): `Saved/Automation/AttackMotion/index.json`.
- 수정 코드의 NullRHI 실제 GAS 유지 입력 검사도 통과했다: `Saved/Logs/AttackComboCode.log`의 `PASS combo=100,101,102,100 sampled_crossfade=1 release=1`. 이 실행은 아직 기존 0.25초 Linear 몽타주 에셋을 사용하므로 새 0.16초 Cubic 에셋과 화면 품질의 최종 검증으로 집계하지 않는다.
- 에디터 종료 후 8개 프로필과 몽타주 저장 완료: `Saved/Logs/AttackMotionApply.log`. 원본 백업은 `Saved/Backups/AttackMotion/20261004T135817857951Z`다. DebugGame 빌드는 `Saved/Logs/AttackMotionDebugBuild.log`에서 성공했다. 실행 도구의 `--configuration DebugGame`으로 실제 사용 구성도 검증할 수 있다.
- 최종 DebugGame 저장 재로드·자동 테스트 45개(38 성공·7 경고 포함 성공)·P0/P1 실제 공간 판정 PASS: `Saved/QA/20261004T135939Z_e0a107_hack_slash_p1/report.json`. 피해·이동 거리·투사체 판정은 유지됐다.
- 최종 DebugGame 렌더 콤보 PASS: `Saved/QA/20261004T140221Z_034ff4_attack_motion_combo_final/report.json`. 196개 포즈 기록·27개 교차 블렌딩 샘플, 전환 간격 0.500/0.517/0.733초를 기록했다. 100→101→102→100 순서, 이전 포즈 고정, 블렌딩 가중치 합 0.95 이상, 해제 후 종료를 검사했다. 하드웨어 직접 입력·연속 영상의 체감 수용·패키지 재생성은 이번 검증에 포함하지 않는다.
- P1 세 스킬의 DebugGame 렌더 기록 완료: `Saved/QA/20261004T140355Z_bdf385_attack_motion_skill_final/report.json`. 논리 시계 진행 샘플 317개에서 포즈 정체 0개다. 포즈 계측 성공을 직접 플레이의 자연스러움이나 성능 수용으로 간주하지 않는다.
- P0 다섯 공격의 DebugGame 렌더 기록 완료: `Saved/QA/20261004T140523Z_bb712f_attack_motion_p0_skill_final/report.json`. 논리 시계 진행 샘플 293개에서 포즈 정체 0개다. 8개 공격 합계 610개 진행 샘플을 기록했다. 일부 IK 본의 큰 프레임 회전은 기록에 남아 있으므로 정체 0개가 모든 관절의 시각적 연속성을 보증하지는 않는다.

## 수용 상태

**구현·자동 검증과 실제 플레이 수용을 구분한다. P0/P1 전체 수용 완료 및 P2 진입 가능 상태가 아니다.**

- P0: 기존 비교 도구와 40회 시험 구성을 유지한다. 직접 플레이 40회는 미실행이다.
- P1-A: 110/113/114 프로필, 5종 중 서로 다른 2종 장착, 저장 트랜잭션, 스킬 ID 쿨다운을 구현했다. 신규 저장의 기본 장착은 데이터의 111+112이며 기존 저장의 빈 선택은 이전 프리셋을 유지한다.
- P1-B: 기존 강화 자격과 CastId 제한을 사용하며 투사체의 원래 스킬 귀속, 환급 슬롯 표시, 현재 입력 키와 설명을 연결했다. 세 강화 빌드의 무보조 6구간 플레이는 미실행이다.
- 110/114 자세 매핑은 기존 몽타주의 후보 구간이다. 연속 모션·사운드·판정 정렬 검수 완료로 표시하지 않는다.
- 20분 패키지 전투 성능 및 6구간/사망/이어하기 결합 수용 검사는 미완료다.

이전 Windows Computer Use 재연결 확인은 `Computer Use native pipe is unavailable ... (os error 2)`로 실패했다. 이번 재개 세션은 네이티브 앱 제어 API가 비활성화되어 직접 입력 검수를 수행하지 않았다. 프로그램에서 입력 콜백을 호출한 결과는 직접 플레이 횟수에 포함하지 않는다.

## 구현 계약

| 기능 | 적용 내용 |
|---|---|
| 낙성참 110 | 최대 600cm 지상 경로·캡슐·지면 선검사, 벽/몸체 앞 축소, 동적 차단 중단, 0.52초 원판 300cm 단일 질의·220%, 0.12초 이전/0.62초 이후 회피 |
| 파쇄연격 113 | 전방 270cm/60도, 0.16/0.34/0.62초 70%+70%+145%, 타격별 20cm 이동, 마지막 타격만 강타·출혈 폭발 |
| 관통검기 114 | 0.22초 단일 발사, 전폭 160cm, 1800cm/s, 1000cm/0.70초 한계, 발사점 포함 Sweep, 대상당 160% 1회, 벽 차단·높이 제한 |
| 투사체 수명 | CastContext를 독립 보관, 회피/다음 시전/Ability 종료와 분리, 시전자 사망·스테이지 변경·런 종료 시 제거, 늦은 적중도 원래 CastId로 기록 |
| 장착·저장 | 기존 v1 저장의 빈 선택은 이전 프리셋 유지. 새 선택 2개를 저장 성공 후 적용. 중복/고정 공격 ID 거절. 첫 준비·웨이브 정비에서만 변경 |
| 쿨다운 | 해제한 스킬의 종료 시각을 ID별 보관. A→B→A 및 두 슬롯 맞교환 보존. 해제된 원래 스킬도 반환 가능 |
| HUD·설명 | Enhanced Input의 현재 매핑 키, 쿨다운/동작/준비/사망 이유, 프로필에서 생성한 범위·타격별 배율·이동·취소 설명, 환급 ID와 회피 잔상 준비 강조 |

준비 중 상시 활성 Ability를 전투로 오인하던 오류를 실제 장착 검사에서 발견해 수정했다. 웨이브 대기의 `RemainingMonsters`는 다음 웨이브 예정 수량을 포함하므로 장착 판정은 Stage 상태와 `SpawnedEnemies`를 사용한다. 스킬/회피/장비 전환 Ability, 피해 처리, 저장 재진입은 별도로 차단한다.

## 콘텐츠와 보존

- 신규: `Content/DataCenter/HackSlashP1/DA_PlayerSkill_110`, `_113`, `_114`.
- 변경: `DT_Skill`의 위 3개 행에서 `PlayerProfile`, `SkillCoolTime`, `Desc`만 변경. `DA_PGProgression.SelectableActiveSkills`에 110~114, `DefaultActiveSkills`에 111+112 등록.
- 원본 몽타주와 적 행은 이 작업에서 수정하지 않았다.
- 적용 전 백업: `Saved/Backups/HackSlashP1/20261004T083417730875Z`.
- 재현 가능한 비대상 행 기준선: `Tools/Validation/Baselines/HackSlashP1_Skills.json`. P0 기준선 → P1 적용 전 → 적용 후를 연결하여 검사한다.
- 기존 Content 저장소의 다른 미커밋 변경은 그대로 보존했다.

## 실행

UE 5.8 Editor Development 빌드 후 엔진 Python으로 실행한다.

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunHackSlashP1.py
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunHackSlashPackage.py --p1
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' -m unittest discover -s Tools/Validation -p 'TestHackSlash*.py'
```

콘텐츠 재적용은 Unreal Python commandlet에서 `ConfigureHackSlashP1.py`를 실행한다. 매번 원본을 별도 백업하며 기존 플레이어 저장은 사용하지 않는다. 실제 검증 명령과 로그 경로는 각 `report.json`에 기록한다.

## 검증 근거

- UE 5.8 빌드: `Saved/QA/P1_build7.log` 성공.
- 1차 자동 검사: `Saved/QA/20261004T083652Z_8069dd_hack_slash_p1/report.json` — 에셋, 43개 자동 테스트, P0/P1 실제 공간 검사 통과.
- 준비 장착 검사 실패 보존: `Saved/QA/20261004T084148Z_fda5a8_hack_slash_p1/report.json`.
- 수정 후 실제 장착+공간 재검사: `Saved/QA/P1_loadout_fix/spatial.log` — 안전 단계 선택, 쿨다운 맞교환, 저장 실패 원복, 중복 거절 통과. 110 대상 2개 각각 220/이동 600/질의 1, 113 전방 285·후방 0/이동 60, 114 근거리·원거리 각각 160.
- Python 기록 검사 23개 통과. 지연 투사체와 다음 시전의 기록이 섞이면 실패한다.
- 모듈 의존성 검사 및 `git diff --check` 확인.

아래 최종 검증 기록이 중단 전 1차 결과보다 우선한다. 이전 패키지나 1차 성공 결과로 수정 후 버전의 검증을 대신하지 않는다.

## 2026-10-04 중단 작업 재개 — 최종 검증

중단 시점의 마지막 소스 변경은 `PGUIInventory.cpp`의 실제 장착/적용 예정 표시였다. `P1_build8.log`의 Editor 빌드는 성공했지만, 당시 `20261004T085430Z_998af5f6_hack_slash_package`와 `20261004T085439Z_a2992a_hack_slash_p1`은 이 변경 전 결과다. 최신 소스를 포함한 패키지를 새 경로에 생성하고 자동 검사·화면 캡처를 갱신했다. 이번 재개에서는 게임 코드와 Content를 추가 변경하지 않았다.

| 완료 항목 | 최종 근거와 검증 범위 |
|---|---|
| UE 5.8 Development 빌드·쿠킹·스테이징 | `Saved/QA/20261004T091111Z_242a870c_hack_slash_package/package.log`: `BUILD SUCCESSFUL`, 종료 코드 0 |
| 에셋·PG 자동 테스트·Editor P0/P1 공간 검사 | `Saved/QA/20261004T091306Z_a42e03_hack_slash_p1/report.json`: 4개 게이트 PASS. `Automation/index.json`: **43개 = 성공 36 + 경고 포함 성공 7, 실패 0·미실행 0** |
| 신규 기본 장착·구버전 저장 보존 | 같은 자동 보고서의 `PG.HackSlash.LoadoutTransactions`: 신규 111+112, 기존 v1 빈 선택의 디스크 재로드 보존, 두 선택 저장/재로드, 저장 실패 원복·재진입·중복 거절 PASS |
| Development 패키지 P0/P1 실제 공간 검사 | 새 패키지의 `report.json`, `spatial_observations.json`, `P1/spatial_observations.json`: 모두 PASS. 접촉 주입 없음, 입력 콜백은 프로그램에서 호출 |
| 안전 단계 장착·쿨다운 맞교환 | 새 패키지의 `P1/spatial.log`: `safe_stage=1 swap_cooldown=1 failed_save_atomic=1 duplicate_rejected=1` |
| 패키지 1080p 캡처 생성·공간 검사 | 새 패키지의 `FinalReview/report.json`: skills/inventory PASS, PNG 4장 모두 1920×1080. 이 PASS는 실행·해상도·로그 검사이며 연출 품질 자동 판정이 아님 |
| Python 기록 검사·모듈 경계 | 재개 후 `TestHackSlash*.py` **23개 PASS**, `AuditModuleDependencies.py` 종료 코드 0, `git diff --check` 통과 |

최종 실행 파일은 `Saved/QA/20261004T091111Z_242a870c_hack_slash_package/Package/Windows/UPlayground/Binaries/Win64/UPlayground.exe`다. SHA-256은 `aca670896b851a9a1eb12ea2f678f4386a4448d87fccd3cb869f428cff71f99c`이며 화면 검증에서 다시 대조했다. 캡처의 전체 명령·격리 UserDir·이미지 경로는 `FinalReview/skills/report.json`과 `FinalReview/inventory/report.json`에 보존했다.

패키지의 공격력 100 격리 측정은 110이 두 대상 각각 220·이동 599.997cm·공간 질의 1회, 113이 전방 70+70+145=285·후방 0·이동 60cm, 114가 근거리/원거리 각각 160이다. 이는 공간 계약 회귀 근거이며 실전 처치 시간이나 밸런스 수용 결과가 아니다.

최초 재패키징 `Saved/QA/20261004T091043Z_f198a3b0_hack_slash_package`는 AutomationTool 사용자 로그 경로 접근 거절로 실패했다. 실패 보고서를 보존하고 빌드에 필요한 권한으로 새 폴더에서 성공했다. 엔진 API 폐기 예정·선호 툴체인·기존 순환 참조 경고, 쿠킹 중 기존 적 이름표 속성 참조 경고는 남는다.

### 화면 검토와 해석 범위

- 중단 전 패키지의 `P1Render/User/Saved/QA/HackSlashP0/Skill_110.png`, `Skill_113.png`, `Skill_114.png`를 실제로 열었다. 110의 민트색 원형 표현·주황색 적중 효과, 113의 전방 부채꼴 검흔, 114의 전방 곡선 검기·보라색 입자를 확인했다. 이 캡처는 마지막 인벤토리 표시 수정 전 근거로 보존한다.
- 최신 패키지의 `FinalReview/skills/User/Saved/QA/HackSlashP0/` 3장도 열었다. 113의 전방 검흔과 114의 검기·입자는 식별된다. **110의 캡처에서는 캐릭터만 보이고 짧은 타격 표현은 보이지 않았다.** 같은 실행 파일을 단독 실행한 `IsolatedSkills/skills/User/Saved/QA/HackSlashP0/Skill_110.png`에서도 동일했다. 양쪽 피해·이동 로그는 PASS지만, 캡처 시점/렌더 준비/실제 표현 중 원인을 확정하지 못했으므로 **110 화면 품질은 미확정**이다. 이전 패키지의 정상 캡처로 최신 결과를 대체하지 않는다.
- 최신 `FinalReview/inventory/User/Saved/Screenshots/Windows/PGInventory00000.png`에서 한글 검술/자유 장착 안내, 낙성참·질풍연참의 프로필 설명, 슬롯 선택 버튼, 질풍연참의 `슬롯 1 · 장착 중`, 저장 완료 표시를 확인했다. 하단 스킬과 적용 버튼은 스크롤 아래에 있으므로 이 한 장으로 5종 전체·스크롤·키보드 선택·적용 예정→저장 완료 상호작용을 검수했다고 보지 않는다.
- 스킬 캡처는 HUD 없는 격리 평면 장면이며 적 메시가 보이지 않는다. 전투 중 적/위험 예고 가독성, 15/50마리, 720p, VFX 저/고, HUD 쿨다운/반환/잔상 강조는 이 화면의 검증 범위 밖이다. `-nosound`로 실행해 청취 QA도 수행하지 않았다.

### 확정한 잔여 수용 항목

| 잔여 항목 | 필요한 실행·근거 | 현재 판정 |
|---|---|---|
| 110 화면 검증 | 타격 전후 연속 프레임과 표현 수명·렌더 준비 상태 대조 | 최신 정지 캡처 2회에서 타격 표현 미확인. 원인 확인·필요한 수정 후 재검증 필요 |
| P0 실제 전투 비교 | M10/M15/RING/E1 × 5시드 × 전후 2종, 실제 입력·대표 영상·실패 기록·시전 계측 | **40칸 미완료**. 자동 프로브와 smoke 제외 |
| P1-A 장착과 입력 UX | 안전 단계에서 5종 중 2종 선택/적용/맞교환, 전투 중 거절, UI 열기·닫기/포커스 복귀, 회피·다음 시전 후 검기 | 자동 계약 PASS, 실제 조작 수용 미완료 |
| 이동·연속 모션 | 벽·보스·동적 장애물·모서리·경사·층·낭떠러지, 110/114 자세와 타격 정렬, 111 두 검흔·112 보행·콤보/회피 | QA-09·18의 실전 조건 및 연속 검수 미완료 |
| P1-B 빌드별 운용 | 출혈/충격파/격분 각각 무보조 6구간, 정예/보스 단독 비교, 처치 없는 보스전 운용과 실패 | QA-21 미완료. 자동 발동 제한과 구분 |
| 화면·청취 품질 | 720p/1080p, 15/50마리, VFX 저/고, 위험 예고/캐릭터/타격 방향, SFX·카메라 피드백 | QA-22 미완료. 정지 캡처 일부만 확인 |
| 장시간 패키지·저장 통합 | 목표 PC·품질을 기록한 1080p 20분 전투, Frame/Game/Render/GPU p50·p95·p99·최대, 메모리·잔존 투사체/타이머/충돌, 6구간/사망/이어하기 | QA-23 미완료. 이번 짧은 프로브는 full RunQA 재실행이 아님 |

**확정 상태: P1 구현·자동 회귀 및 일부 정지 화면 확인 완료. 110 화면 품질과 P0/P1 실제 플레이 수용은 미완료, P2는 미착수다.** 110 표현의 원인 확인과 네이티브 입력·영상·청취·장시간 전투 근거가 확보된 뒤 위 항목을 갱신한다.

## 2026-10-04 공격 모션 끊김 수정

사용자가 실제 공격 모션의 끊김을 보고하여 재생 경로를 수정했다. 위 공간 검사 PASS와 정지 이미지 확인만으로 모션의 연속성을 보장하지 않는다.

- `PGCharacterAnimInstance`: 히트스톱은 애니메이션에 0초 Delta를 전달한다. 기존 `Displacement / DeltaSeconds`가 정지 시 NaN, 이동 시 Inf를 만들 수 있어 0으로 처리하고 위치 기준은 계속 갱신한다. 시작 위치도 소유 캐릭터 위치로 초기화한다.
- `PGPlayerAttackComponent`: PostPhysics에서 포즈를 옮기던 코드를 `캐릭터 이동 → 공격 시계/포즈 → 메시 평가` 순서로 고정했다. 히트스톱 직후 논리 시계만 진행하고 표시 포즈는 직전 프레임에 남는 지연을 줄인다. 루트 모션 무시와 프로필 판정 권한, 기존 Notify 피해 차단은 유지한다.
- `PGPlayerSkillProfile`: 구간별 선형 포즈 매핑의 불연속적인 재생 속도를 양의 공통 접선을 사용하는 단조 Hermite 보간으로 바꿨다. 모든 원본 포즈 키를 정확히 통과하고 이웃 구간을 벗어나거나 역행하지 않는다. 피해·이동·쿨다운·취소 시각·포즈 키 데이터와 Content는 변경하지 않았다. 원본을 짧은 시전 시간에 압축하는 전체 속도는 그대로이므로 전용 모션의 연출 튜닝과 구분한다.
- 장착 준비 시 기본 공격의 후속 콤보 몽타주와 검흔 기본 메시도 보관하여 첫 사용 중 동기 로딩을 줄인다. 렌더 PSO/셰이더 준비까지 보장하는 작업은 아니다.
- `PG.Animation.HitStopDelta`는 정지/이동 중 0 Delta와 재개 시 속도를, `PG.HackSlash.PoseContinuity`는 타격 키 보존·속도 연속성·역행/범위 초과 방지를 검사한다.

`RunHackSlashMotion.py`는 실제 렌더에서 스킬별 논리 시각·몽타주 위치·히트스톱·평가된 뼈 포즈 변화를 기록한다. `--p1`로 110/113/114, 기본값으로 100/101/102/111/112를 검사하며 `--packaged-exe`로 패키지를 선택한다. 정지 캡처는 `--capture`에서만 수행하고 읽기 지연을 명시한다. 기록 성공은 `RECORDED`이며 직접 조작·모션 품질 수용을 자동 PASS로 선언하지 않는다.

1차 계측 `20261004T092828Z_aea1b2_attack_motion_before`와 `20261004T093016Z_493334_attack_motion_before`는 정지 캡처가 포함되어 프레임 시간 비교 근거에서 제외한다. 포즈 순서·0 Delta 수정 직후 `20261004T093427Z_077c6d_attack_motion_after`에서는 P0 다섯 스킬 모두 논리 시계 진행 중 전체 포즈가 그대로인 샘플이 0개였다. 이 기록은 Hermite 보간 적용 전 중간 결과이며 아래 최종 결과와 구분한다.

첫 전체 회귀 `20261004T093743Z_61b8b1_hack_slash_p1`는 새 히트스톱 테스트가 AnimInstance를 잘못된 Outer에 생성하여 실패했다. 테스트 객체를 실제 SkeletalMeshComponent 소유로 고쳤으며 실패 보고서는 보존한다.

최종 Editor 빌드는 `Saved/QA/AttackMotion_FinalBuild.log`와 픽스처 수정 후 `AttackMotion_TestBuild.log`에서 성공했다. `Saved/QA/20261004T094036Z_64ea95_hack_slash_p1/report.json`은 에셋·자동 테스트·P0/P1 공간 검사 모두 PASS이며, 자동 테스트 **45개 = 성공 38 + 경고 포함 성공 7, 실패 0**이다. 두 신규 회귀도 Success다. 모듈 의존성 검사, Python 도구 구문 검사와 `git diff --check`를 통과했다.

수정 패키지는 `Saved/QA/20261004T093857Z_2f373cde_hack_slash_package/Package/Windows/UPlayground/Binaries/Win64/UPlayground.exe`다. `report.json`의 패키지·P0/P1 공간 검사 모두 PASS이며 실행 파일 SHA-256은 `6054c344063bd92566720a1d9aeec4139d504509f25c9ae5b7bb66eadc292cba`다. 이전 `091111Z` 패키지에는 이 모션 수정이 없다.

최종 패키지 렌더 계측은 `Saved/QA/20261004T094306Z_81fdb6_attack_motion_final_package/report.json`(P0)과 `Saved/QA/20261004T094427Z_e5b211_attack_motion_final_package/report.json`(P1)에 있다. 총 8개 공격·345개 포즈 샘플에서 히트스톱 외 논리 시계가 진행한 298개 샘플 모두 전체 뼈 포즈가 갱신됐으며, 시간만 진행하는 정지 샘플은 **0개**다. 두 실행은 정지 캡처 없이 720p/60fps 상한으로 수행했고 공간 검사도 통과했다. 이는 평가 갱신 정체 회귀 근거이며 목표 프레임 성능·연속 영상의 자연스러움·직접 콤보 조작을 인증하지 않는다. 110 타격 표현의 이전 정지 캡처 문제도 이번 무캡처 계측만으로 완료 처리하지 않았다.
