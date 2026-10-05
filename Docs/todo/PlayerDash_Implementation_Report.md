# 플레이어 대시

## 조작과 데이터

- 기존 회피 슬롯(스킬 10000, `GA_Skill_Roll`)을 대시로 사용한다. 저장·시작 어빌리티·격분의 잔상 강화 연결은 기존 ID/태그를 유지한다.
- `DA_InputConfig.DefaultMappingContext`가 참조하는 `IMC_Default`의 C를 LeftShift로 교체한다. HUD는 실제 Enhanced Input 매핑에서 키를 읽는다.
- 이동 입력 방향을 우선하며 정지 중에는 지면 조준 방향으로 전진한다. 기본 거리는 450cm, 시간은 0.36초다. `BP_LocalPlayer → PlayerDashComponent → PG|Dash`에서 거리·시간·잔상 간격·소멸 시간·색을 조정한다. 기존 스킬 재사용 시간은 유지한다.
- 지상에서만 시작하며 CharacterMovement의 RootMotionSource로 이동한다. 벽 충돌 및 지면 처리를 유지하고 대시 중 낭떠러지 보행을 제한한다. 종료·취소 시 원래 보행 설정을 복원한다. 대시 중 재진입과 공격은 차단하며 기존 공격의 회피 취소 창에서 진입한다.
- 대시 시작 시 플레이어 캡슐의 Enemy 채널(`ECC_GameTraceChannel1`)이 Block이면 Overlap으로 바꿔 적을 통과한다. 기존 Overlap/Ignore 설정은 유지하고 종료·취소·사망·EndPlay에서 변경한 응답을 복구한다. 벽·지면과 전투 Overlap 판정은 유지하며 무적을 부여하지 않는다.

## 애니메이션과 잔상

- 목록 확인: AnimeKatana `KC_Dash`(1.8초), FrankSlash `Sword2_F_Dash`(0.6초), 플레이어 스켈레톤의 `AS_ElfSelenaDodge_F_Anim`(1초).
- 기존 플레이어 스켈레톤과 호환되며 몸을 굴리지 않는 낮은 전진 동작인 `AS_ElfSelenaDodge_F_Anim`을 선택했다. 머리/골반 포즈 표본도 확인했다. 원본은 수정하지 않는다.
- `/Game/Art/PlayerCombatFX/AS_PGPlayerDash`는 루트 이동을 고정한 복제본, `AM_PGPlayerDash`는 플레이어 AnimBP의 `FullBody` 슬롯을 사용한다. 이동 거리는 애니메이션 변위와 중복 적용하지 않는다.
- 현재 표시 메시와 모듈러 부위의 포즈를 `UPoseableMeshComponent`로 복제한다. 숨겨진 원본 리그는 제외하며 각 스냅샷은 월드에 고정되어 청록색으로 사라진다. 최대 8개 스냅샷을 재사용하고 정지한 벽 앞에서는 새 잔상을 겹쳐 생성하지 않는다.
- 기본 생성 간격 0.045초, 소멸 0.24초. 포즈 메시 자체는 매 프레임 애니메이션을 평가하지 않으며 대시/소멸이 끝나면 잔상 관리 틱도 중지한다. 사망·취소·EndPlay에서 정리한다.

## 재현과 검증

- 생성/이관: UE Python `Tools/Validation/ConfigurePlayerDash.py`. 기존 테이블·입력·대상 에셋을 `Saved/Backups/PlayerDash/<timestamp>`에 백업한다. 다른 스킬 행과 회피의 기존 밸런스 값은 비교하여 보존한다.
- 재로드: 동일 스크립트에 `-PGDashValidate`를 전달한다.
- 통합 검사: 에디터 Development 빌드 후 `Tools/Validation/RunPlayerDash.py --all-characters`. `--apply`는 에셋 이관을 추가하고 `--render-only`는 화면/공간 검사만 실행한다.
- `PGDashProbe`는 `Dash_` 격리 프로필에서만 동작한다. 실제 GAS 시전·이동 거리·벽 충돌·강제 취소·잔상 수명과 정리를 검사하고 선택 캐릭터의 대시 화면을 저장한다. 사용자 저장 데이터는 사용하지 않는다.

## 검증 범위

- UE 5.8 `UPlaygroundEditor Win64 Development` 최종 빌드 PASS: `Saved/Logs/BuildPlayerDash.log`.
- 최종 통합 실행 `Saved/QA/20261005T085346Z_e8811f_player_dash/report.json`: **PASS**. 저장 재로드, PG 자동 테스트 45개(일반 성공 38, 경고 포함 성공 7, 실패 0), 7종 실제 GAS 검사 모두 통과했다.
- Bokusei / LianLian / Honoka / Hichi / Siuha / Lili / Nenmir에서 열린 바닥 이동 458.33cm, 220cm 앞 검증 벽에서 이동 167.48cm로 정지했다. 프레임 단위 RootMotionSource 적분으로 기본 설정 450cm와 작은 차이가 있다. 최대 동시 잔상 5개, 종료 후 0개·관리 틱 중지, 강제 취소 후 잔여 이동 없음과 보행 설정 복원을 확인했다.
- 7장의 SM6 1280×720 캡처를 직접 확인했다. 각 캐릭터의 낮은 전진 포즈와 외형에 맞는 청록색 잔상이 보인다. 경로는 실행 폴더의 `<캐릭터>/User/Saved/QA/PlayerDash/Dash.png`다. 캡처는 고정 60Hz와 일시정지 프레임을 사용하며, 첫 시전 리소스 사전 준비 후 별도 렌더 대기 없이 촬영했다.
- 최초 렌더에서 확인한 DefaultSlot/FullBody 불일치와 첫 시전 렌더 리소스 준비 문제를 수정했다. 초기 반복 검사 실패는 기존 로드아웃의 재사용 시간이 끝나기 전에 검사한 것이므로 쿨다운을 유지하고 검사 시점을 수정했다. 초기 실패 실행도 `Saved/QA`에 남겼다.
- 최초 원본 백업: `Saved/Backups/PlayerDash/20261005T083821687588Z`. 최종 에셋 적용 로그: `Saved/Logs/ConfigurePlayerDash.log`.

직접 키보드 조작의 체감, 장시간 패키지 성능 및 네트워크 플레이 검증은 별도다. 무적 시간·전용 대시 음향·무기 자체의 잔상은 이번 구현 범위에 포함하지 않았다.

## 2026-10-05 적 통과 수정 검증

- UE 5.8 Development Editor 빌드 PASS: `Saved/Logs/BuildDashEnemyPass.log`.
- `Saved/QA/20261005T090852Z_81a071_player_dash`에서 에셋 재로드 및 PG 자동 테스트 45개 PASS. 해당 실행의 렌더 프로세스는 제한 환경에서 엔진 초기화가 멈춰 종료했으므로 전체 보고서는 FAIL로 보존한다.
- 권한을 조정한 별도 렌더 실행 `Saved/QA/20261005T091159Z_6df27d_player_dash/report.json` PASS. Bokusei의 실제 GAS 대시가 EnemyCharacter 프로필 캡슐 3개를 458.33cm 이동하며 통과했다. 대시 전·후의 일반 이동 Sweep은 적에게 막히고 종료·강제 취소 후 원래 Enemy 응답으로 복구됨을 확인했다.
- 열린 바닥 458.33cm, 검증 벽 앞 167.48cm 정지, 재진입 차단·취소 후 잔여 이동 없음·잔상 정리도 PASS. 이번 후속 검증은 공통 캡슐 동작을 대상으로 하며 전체 외형 재검사 및 직접 키보드 조작은 수행하지 않았다.
