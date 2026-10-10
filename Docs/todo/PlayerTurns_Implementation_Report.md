# 플레이어 방향 전환 모션

## 2026-10-09 Idle 교체 · 턴 응답성 수정

이 절이 아래 최초 구현의 35° 출발·이동 중 125° 턴·65% 이동 재개 설정보다 우선한다.

### 원인과 수정

- 기존 기본 Idle은 `Sword2_Equip_Idle`이었다. 같은 팩의 `Sword2_Unequip_Idle`을 공통 플레이어 스켈레톤으로 리타겟하여 `BS_PlayerSword`의 정지 표본 9개에 연결했다. 1.667초 루프이며 적의 Idle과 방향별 걷기·달리기는 보존했다. 소스의 Unequip이라는 이름은 자세 이름이고 장비 해제 로직을 호출하지 않는다.
- 0.5초 턴을 1.3배속으로 재생하면서 65%까지 이동 입력을 0으로 반환해 약 250ms 출발 대기가 발생했다. 재개를 20%(약 77ms)로 앞당기고 45%까지 부드럽게 입력을 복귀시킨다. 모션 길이가 바뀌어도 `MaxTurnSeconds`(0.4초) 이내가 되도록 재생률을 계산한다.
- 이동 중 급반전에도 제자리 발 디딤 모션을 사용했고, 가속을 재개해도 턴 포즈의 가중치가 남았다. 기본 `bAllowMovingTurns=false`로 달리기 반전은 8방향 이동과 회전 보간을 사용한다. 정지 출발 턴도 이동 재개 구간에 맞춰 지상 이동 포즈로 블렌딩한다.
- 45° 작은 변경까지 턴이 시작되던 기준을 60°로 높였다. 정지 턴은 이동 입력의 첫 프레임에만 선택해 급반전 감속·벽 충돌 중 재진입하지 않는다. 입력을 해제하면 다시 허용한다.
- 진행 중인 턴의 목표와 새 입력이 65°보다 작게 달라지면 이전 방향을 계속 따르던 취소 기준을 20°로 낮췄다. 취소 뒤 이동 입력은 즉시 정상 전달되며 공격·회피 취소와 전투 후 방향 유지 정책은 유지한다.

### 재현 · 적용 근거

```text
python Tools/Validation/BuildPlayerLocomotionPolish.py
python Tools/Validation/RunPlayerLocomotionPolish.py
python Tools/Validation/RunPlayerTurns.py --step preview --character Bokusei
```

- 플레이어 전용 적용은 Idle·플레이어 리타겟 설정·BlendSpace·턴 DataAsset만 백업한다. `Saved/PlayerLocomotionPolish/20261009T111707945168`의 적용과 새 프로세스 재로드가 PASS이며 `transaction.json`은 VERIFIED다.
- 최초 스크립트의 에디터 전용 BlendSample 프로퍼티 접근 실패는 수정했으며 해당 실행 `20261009T111607240394`는 RESTORED다.
- 전체 이동 에셋 재생성도 `player_idle_prefix`를 따라 새 Idle을 유지한다. 기존 Idle 패키지는 참조 교체 후에도 복구용으로 남겨둔다.
- 입력 프리뷰는 작은 각도·달리기 반전의 턴 미사용, 180ms 이내 이동 재개, 45° 입력 수정, 공격·회피 연결, 전투 중 16방향/속도 조합을 포함한 28조건으로 확장했다. 연속 캡처 224장 외에 Idle 정지 화면을 저장한다.

### 검증 결과

- UE 5.8 Development 에디터 빌드 PASS: `Saved/PlayerLocomotionPolish/build_20261009T111810/build.log`.
- PG 자동 테스트 53개 PASS(일반 성공 44, 경고 포함 성공 9, 실패 0)와 기존 에셋 검사 PASS: `Saved/QA/20261009T112055Z_3b809ae7/report.md`. 이번 빌드를 사용해 `--skip-build`로 실행했다. 기존 테스트 환경의 장비·GameplayCue·월드 정리 경고와 에셋 경고는 보고서에 보존했다.
- Bokusei 입력·렌더 28조건/224장 + Idle 캡처 PASS: `Saved/PlayerTurns/20261009T112055036866/report.json`. 90°/180° 정지 출발에서 속도 5cm/s 초과 시점은 117ms, 45° 출발은 17ms였다(60Hz 고정 스텝). 달리기 반전에는 턴 포즈가 재진입하지 않았고, 취소·공격·회피·16방향 전투 이동이 통과했다.
- Hwarin에서도 같은 28조건/224장 + Idle 캡처 PASS: `Saved/PlayerTurns/20261009T112342794157/report.json`. Idle과 90°/180° 전환 화면을 확인했다. 두 외형 합계 56조건·448장과 Idle 2장이다.
- `User/Saved/QA/PlayerTurns/Idle.png`와 90°/180°/달리기 반전의 연속 캡처를 직접 확인했다. 새 Idle은 검을 아래로 든 중립 자세이며 걷기·달리기 포즈로 이어진다.

장시간 직접 키보드 조작, 경사면 접지와 모든 의상의 관통 검수는 자동 검사 범위 밖이다.

## 연결과 튜닝

- 후속 측면·후방 이동: 공격 중과 종료 후 1.5초는 공격 방향을 유지하며 8방향 이동을 사용한다. `CombatStrafeSeconds`를 0으로 설정하면 종료 후 방향 유지가 해제된다. 공격을 이어가면 유지 시간도 갱신된다. 평상시 Turn 이동과 구분되며, 만료 후 기존 회전 로직으로 돌아간다.

- 보유 Sword2의 좌·우 45°/90°/180° 턴 6개를 플레이어 공통 전투 스켈레톤으로 리타겟한다. 실제 적용본은 `/Game/DataCenter/PlayerTurns`다.
- `DA_PlayerLocomotion`은 각 모션과 원본 루트 회전의 61개 표본, 선택 각도, 재생 속도, 이동 재개 지점, 블렌드 시간을 소유한다. 원본 설정은 `Tools/Validation/Data/PlayerTurns.json`이다.
- 정지 출발은 35°부터 턴을 선택하고, 이동 중에는 125° 이상 급반전에 턴을 사용한다. 작은 이동 방향 변경은 기존 8방향 이동과 회전 보간을 사용한다. 기본 턴은 0.5초 클립을 1.3배로 재생한다.
- 턴 초반에는 이동 가속을 잠시 쉬고 기존 속도를 정상 감속한다. 65% 이후 이동 입력을 부드럽게 복귀시키며, 턴 끝 0.12초 구간에서 지상 이동 포즈와 섞는다. 순간적인 위치 이동이나 속도 삭제는 하지 않는다.
- 원본의 회전 곡선으로 캡슐을 회전한다. 리타겟이 골반에 흡수한 같은 회전은 생성본에서 제거하여 중복 회전을 방지한다. 원본 클립은 수정하지 않는다.
- `ABP_LocalPlayer`의 `PGPlayerTurnEvaluator`와 `PGPlayerTurnBlend`를 기존 지상 이동과 공중 분기 사이에 연결했다. 기존 발 IK·상체/전신 공격 슬롯은 이후에 평가된다. 턴은 몽타주를 사용하지 않아 스킬 시작을 막지 않는다.
- 키 해제, 큰 입력 방향 변경, 공중 상태, 조작 잠금, 공격·회피 시작에서 턴을 취소한다. 회피 컴포넌트 시작 프레임에 즉시 취소하며, 마지막 턴 포즈를 보존한 채 블렌드 아웃한다. 재시작 보호 시간은 월드 시각을 사용해 정지·공격 중에도 만료된다. 정확히 뒤쪽인 입력은 직전 턴 방향을 사용해 좌우 선택 떨림을 방지한다.

## 재현과 백업

UE 5.8 에디터를 빌드한 뒤 UE 번들 Python으로 실행한다.

```text
python Tools/Validation/RunPlayerTurns.py --step apply
python Tools/Validation/RunPlayerTurns.py --step validate
python Tools/Validation/RunPlayerTurns.py --step preview --character Bokusei
```

- `apply`는 대상 AnimBP와 생성 패키지만 백업하고, 적용 및 새 프로세스 재로드가 실패하면 해당 실행의 백업을 복구한다. 기록은 `Saved/PlayerTurns/<실행 시각>`에 남긴다.
- `validate`는 6개 방향, 모션·스켈레톤 참조, 루트 잠금, 회전 표본, 시작/끝 골반 방향, 그래프 노드와 컴파일을 확인한다.
- `preview`는 격리 프로필과 바닥에서 실제 Enhanced Input 이동을 주입한다. 기존 턴 11조건과 전투 방향 유지 중 걷기/달리기 각 8방향을 합쳐 27조건·216장을 검사한다. 후자는 몸 방향 유지, 로컬 애니메이션 방향, 실제 속도, 발 포즈 변화를 검증한다. 검사용 근접 카메라를 사용하며 이동·입력·애니메이션·GAS는 실제 경로를 사용한다.

## 검증 상태

### 측면·후방 이동 후속 검증

- Development 에디터 빌드 통과(`Saved/PlayerTurns/build.log`).
- 적용·재로드 및 27개 블렌드 표본/방향별 걷기·달리기 참조 검사 통과: `Saved/PlayerTurns/20261009T105547265689/report.json`.
- Bokusei 실제 입력 27조건·216장 통과: `Saved/PlayerTurns/20261009T105633195813/report.json`. 전투 방향 유지 중 걷기 170/달리기 600, 8방향 애니메이션 각도, 몸 방향 유지와 발 움직임을 확인했다. 이전 턴 11조건도 같은 실행에서 통과했다.
- 측면·후방 걷기 연속 화면 확인: `Saved/PlayerTurns/strafe-review.png`. 수동 키보드 조작감과 모든 외형의 추가 검수는 별도다.
- 기존 휴머노이드 전체 검증 재실행은 과거 실행의 보호 에셋 해시와 현재 다른 작업의 에셋이 달라 실패했다. 해당 에셋을 복구하거나 기준 해시를 덮어쓰지 않았다. 이번 변경은 위 별도 실행으로 현행 플레이어 데이터와 런타임 동작을 검증했다.

- UE 5.8 Development 에디터 빌드 성공: `Saved/PlayerTurns/build.log`.
- 최종 에셋 적용·새 프로세스 재로드 PASS: `Saved/PlayerTurns/20261009T101740833963/report.json`. 여섯 방향의 루트는 고정되며 골반 시작·끝 방향 차이는 0.001° 미만이다.
- Bokusei 실제 입력·근접 렌더 11조건/88장 PASS: `Saved/PlayerTurns/20261009T102732501216/report.json`.
- Hwarin 실제 입력·근접 렌더 11조건/88장 PASS: `Saved/PlayerTurns/20261009T102934050208/report.json`.
- PG 자동 테스트 51개 PASS(일반 성공 42개, 경고 포함 성공 9개, 실패 0개), 공간 판정·콤보 및 Bokusei의 이동 공격 100/101/102/112/114, 히트스톱·대시 예약·벽 충돌 회귀 PASS: `Saved/QA/20261009T102757Z_4eac61_mobile_combat/report.json`.
- 실제 90°/180° 연속 포즈와 두 외형의 근접 화면을 확인했다. 검수용 배열은 `Saved/PlayerTurns/turn-review-final.png`이며 원본 PNG는 각 프리뷰 실행의 `User/Saved/QA/PlayerTurns`에 있다.

최초 검증에서 리타겟 골반에 남은 중복 회전, 회피 시작 시 한 프레임 늦은 턴 해제, 입력을 뗀 동안 만료되지 않던 재시작 보호 시간을 발견해 수정했다. 실패 실행 기록은 보존했다. 별도 전투 검증의 파일 잠금으로 중단된 `20261009T101430527937` 트랜잭션은 잠금 해제 후 복구를 완료했고, 위 최종 실행으로 재적용·검증했다. 생성 도구는 잠긴 대상 에셋이 해제될 때까지 기다리며 다른 에디터를 종료하지 않는다.

직접 키보드 장시간 플레이, 모든 의상의 관통, 경사면 접지와 DebugGame/패키징 검수는 별도다. 기본 Development 실행 스크립트로 적용된 동작을 확인할 수 있다.
