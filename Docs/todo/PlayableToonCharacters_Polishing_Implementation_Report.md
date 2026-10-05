# 툰 캐릭터 폴리싱 구현 기록

작성일: 2026-10-05. [설계](PlayableToonCharacters_Polishing_Design.md)의 P0 재생성 안전장치와 P1 장비 부착 기반을 구현했다. **7종의 그립 시각 튜닝이나 P2–P5 완료를 의미하지 않는다.** 기존 FK 출력과 P09 루트→골반 단위 복원은 유지한다.

최신 상태: 아래 **Bokusei 실제 검 그립 적용** 기록에서 사진의 손 위치·쥐기 포즈 보정을 완료했다. 앞선 빈 프로필/미적용 설명은 해당 단계 당시의 기록이다. 다른 6종의 개별 그립 튜닝과 P2–P5는 남아 있다.

## 이번 변경

- `Tools/Validation/Data/PlayableCharacterPolish.json`에 11종의 외형·본 매핑·부위·소스/타깃 리그·리타게팅 포즈·Op 설정 44개를 내보냈다. `configure`는 체인을 추론하거나 포즈를 자동 정렬하지 않고 이 원본을 재적용한다. P09 부위 목록도 원본에 포함하여 `Saved/P09Modular/configure.json` 의존성을 제거했다.
- 모든 변경 예정 패키지 53개를 먼저 기록하고 `.uasset` 및 `.uexp/.ubulk/.uptnl`과 JSON 원본을 백업한다. 적용 후 별도 에디터 프로세스에서 기존 참조 검사와 의미상 설정 비교가 모두 통과해야 PASS다. 저장·실행·재로드 실패 시 에디터 종료 후 원래 파일로 복원하며, 새로 만든 패키지 파일만 제거한다.
- 원본과 마지막 적용 상태를 비교해 내보내지 않은 편집을 탐지한다. 스키마 1은 Pelvis Motion/FK Chains Op만 재현한다. 다른 Op, IK Goal/Solver, 제외 본이 있으면 변경 전에 거부한다. 지원하지 않는 설정을 지우고 계속하지 않는다.
- `Portrait`는 생성 소유권에서 제외하고 재로드 후 경로를 비교한다. 다른 미소유 필드도 setter를 호출하지 않는다. 실행별 보고서를 사용하고, 복구 실패 시 잠금과 백업을 남긴다.
- 기존 적·스탯·사망 테이블 행은 보존하고 없는 P09 행만 초기 생성한다. 재로드 검사에서 기존 전체 행을 비교하여 외형 재생성이 수정된 전투 밸런스를 초기화하지 않는지 확인한다.
- `FPGAppearanceGripProfile`에 기존 무기 GameplayTag, 원본 소켓, 표시 본/소켓, 로컬 `GripOffset`을 추가했다. 프로필이 정상일 때 명시 부착점을 우선하며 자동 Anchor 오프셋과 중복 합성하지 않는다. 프로필 부재·중복·잘못된 대상/Transform은 기존 부착 계산으로 복귀한다.
- 장비 생성·장착·납도에서 기존 무기 태그를 전달한다. Anchor는 소켓과 태그를 함께 식별하고 장착 요청마다 Transform을 갱신한다. 외형 교체 시 소켓으로 복귀한 실제 무기의 등록 태그를 찾아 다시 연결한다.
- `PGCharacterProbe`에 7종의 태그별 Anchor 분리, 같은 Anchor의 오프셋 갱신, 잘못된 대상·중복 프로필 복귀 검사를 추가했다. 기본 공격의 원본/표시 오른손·양발·머리 월드 위치와 표시 스케일을 프레임별 CSV로 기록한다. 이것은 이후 튜닝의 기준 관측이며 접지·그립 오차의 수용 판정은 아니다.

`GripOffset`은 **타깃 본/소켓 로컬 좌표**다. 이동값에는 본과 컴포넌트 스케일이 적용되며 월드 cm로 해석하지 않는다. 필드 툴팁에도 이를 명시했다. 현재 원본의 `grip_profiles`는 모두 빈 배열이므로 출시용 시각 오프셋을 임의로 적용하지 않는다.

## 사용

UE 5.8 Development Editor를 빌드한 후 엔진에 포함된 Python으로 실행한다. PowerShell 예:

```powershell
$uePython = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
& $uePython Tools/Validation/RunPlayableCharacters.py --step polish-export
& $uePython Tools/Validation/RunPlayableCharacters.py --step configure
& $uePython Tools/Validation/RunPlayableCharacters.py --step polish-validate
& $uePython Tools/Validation/RunPlayableCharacters.py --step polish-check
& $uePython Tools/Validation/RunPlayableCharacters.py --step runtime
& $uePython Tools/Validation/RunPlayableCharacters.py --step automation
```

`polish-export`는 **에디터에서 저장한** 현재 설정을 읽고 기존 JSON을 실행 폴더에 백업한 뒤 교체한다. 내보낸 JSON diff를 검토한다. 저장하지 않은 열린 에디터 메모리를 읽는 명령은 아니다. 생성/내보내기는 에셋을 편집 중인 에디터를 닫고 실행한다. 설정의 구조체 문자열은 설치된 UE의 `export_text` 형식이며 파싱 후 다시 내보낸 설정과 비교한다.

`polish-check`는 동일 원본으로 생성→새 프로세스 참조 검사→새 프로세스 의미 비교를 두 번 수행한다. 본 이름은 UE `FName`에 맞게 대소문자를 무시하고, 텍스트로 직렬화된 실수는 소수점 5자리로 정규화해 비교한다. 바이너리 일치를 의미상 재현성으로 대체하지 않는다. 파일 해시는 백업 무결성 확인에 사용한다.

실패 주입 및 중단 복구:

```powershell
& $uePython Tools/Validation/RunPlayableCharacterPolish.py --step configure --inject-save-failure 4
# 실패 종료와 transaction.json의 RESTORED를 확인한다.
# 실행기 강제 종료 후에는 해당 에디터 프로세스가 종료된 것을 확인하고 복구한다.
& $uePython Tools/Validation/RunPlayableCharacterPolish.py --step polish-restore --restore-run 'Saved/PlayableCharacters/Runs/<실행>/1'
& $uePython Tools/Validation/TestPlayableCharacterPolish.py
```

복구는 지정된 실행 매니페스트의 백업을 검증한 뒤 수행한다. 다른 프로젝트나 Content 밖 경로, 누락·중복 파일 행, 손상된 백업은 거부한다. 기록된 에디터 프로세스가 살아 있으면 수동 복구도 거부한다. 강제 종료된 실행의 `polish.lock`을 먼저 삭제해 새 생성으로 덮지 않는다.

## 전투 참조 조사

플레이어 `PGPlayerAttackComponent`의 현재 타격 영역·이동은 캐릭터/캡슐과 공격 프로필, 검기 VFX는 캡슐 바닥과 프로필 높이를 기준으로 한다. `PGPlayerSkillProjectile`은 자체 스윕을 사용한다. 따라서 표시 손 Transform을 수정하는 것만으로 이 논리 원점을 직접 변경하지 않는다.

반면 `PGWeaponBase`의 레거시 `WeaponCollisionBox`는 무기 메시 자식이다. 해당 판정을 사용하는 동작에서는 표시 부착 변경이 충돌 위치에 영향을 줄 수 있다. 실제 그립 튜닝 전에 해당 경로의 공격 횟수·피해·Notify·타격 시점·VFX 비교가 필요하다. 이번에 부착 프로필을 빈 값으로 유지한 이유도 이 회귀 기준을 먼저 확보하기 위해서다.

## 검증 기록

실행 경로는 `Saved/PlayableCharacters` 기준이다.

| 검사 | 결과·근거 |
|---|---|
| 변경 전 기존 런타임 | PASS, `Runs/20261005T043246_runtime` |
| Development Editor 빌드 | PASS, `build-polish-final.log`, CSV 추가 후 `build-polish-samples.log` |
| 파일 트랜잭션 테스트 | 8개 PASS: 부분 저장 복구, 새 파일 정리, 기존 부속 파일 복원, 손상 백업·불완전 매니페스트·경로 이탈·미계획 저장·잘못된 프로젝트·중복 실행 거부, 원본 백업 |
| 동일 입력 두 차례 재생성 | PASS, `Runs/20261005T045102874757Z_polish-check`의 `1/2`; 매회 53개 저장 및 44개 의미 비교 |
| 미내보내기 편집·미지원 Op | PASS, 위 실행 각 `polish-validate.json`; 메모리에서 설정을 바꾸어 실제 preflight 거부 후 원상 복귀 |
| Portrait 보존 | PASS, 위 실행의 `configure.json`에 이전 참조 기록 후 새 프로세스에서 비교 |
| 초기 그립 런타임 회귀 | PASS, `Runs/20261005T045005_runtime`; 7종 캐시/복귀와 기존 7종·P09 4종 검사. 최종 순차 실행 결과는 아래에 별도 기록 |
| 실제 저장 실패·복구 | `Runs/20261005T045721408888Z_configure`: 네 번째 저장 후 의도한 FAIL, `transaction.json`은 RESTORED. 53개 패키지의 212개 파일/부재 항목을 백업 해시와 대조해 전부 일치 |
| 복구 후 설정 재로드 | PASS, `Runs/20261005T045958611755Z_polish-validate` |
| 복구 후 최종 런타임 | PASS, `Runs/20261005T045958_runtime`; 7종·P09 4종 및 그립 캐시 회귀. `pose-samples.csv`에 7종·4개 본의 유한 위치/스케일 2,308행 기록 |
| PG 자동 테스트 | PASS, `Runs/20261005T045958_automation/Automation/index.json`; 45개 중 성공 37·경고 동반 성공 8, 실패/미실행 0 |
| 전투 테이블 보존 포함 최종 생성 | PASS, `Runs/20261005T050311533606Z_configure`; 53개 패키지 재생성, 44개 설정 및 Portrait·기존 적/스탯/사망 행 재로드 비교 |
| 실제 그립 데이터 직렬화 | PASS, `Runs/20261005T050706631180Z_polish-validate`; 메모리에 태그·소켓·비영 오프셋 프로필을 만들고 export→비우기→재적용→검증 후 복원. 실제 에셋의 빈 배열뿐 아니라 값이 있는 프로필도 검사 |

## 단계별 남은 수용

| 단계 | 상태 |
|---|---|
| P0 | 안전한 생성·내보내기·복구·재로드와 아래 프로필 8공격 전후 계측 구현. 원본 아트의 별도 체크아웃 복구, 전체 동작 영상과 레거시 무기 판정의 수용은 남음 |
| P1 | 태그 기반 부착·캐시 회귀, 접촉 좌표 후보 계산·전투 회귀 게이트 구현. 7종의 접촉 좌표계 저작, 실제 오프셋·손가락 포즈·가중치 커브 및 대기/공격/회피 시각 수용은 남음. 플레이어 납도는 현 코드에서 비활성 |
| P2–P3 | 발 IK·접지 상태·LOD 품질·P09 IK 단위 실험 미구현 |
| P4 | 헤어 물리·Cloth 파일럿 미구현 |
| P5 | 패키지 쿠킹·목표 부하·직접 플레이 미실행 |

`Content` 제외 정책은 유지한다. JSON이 참조하는 `/Game/Art/ToonTest`, `/Game/Art/ToonCharacters`, `/Game/Art/P09Modular`와 소스 캐릭터, 기존 게임플레이 템플릿/데이터는 별도 Content 백업에서 확보해야 한다. **이번 JSON은 FBX·텍스처·물리 에셋의 배포/보관소를 대신하지 않으며, 독립 체크아웃에서 원본 아트 복구를 검증하지 않았다.** 이 때문에 P0 전체 종료나 전체 폴리싱 완료로 표시하지 않는다.

## 2026-10-05 후속: P1 접촉 좌표 계산과 전투 회귀 게이트

실제 그립 튜닝에 앞서 필요한 전투 전후 비교와 접촉 좌표 후보 생성 경로를 추가했다. **이 후속 작업도 P1 시각 튜닝 완료는 아니다.** 원본 `grip_profiles`는 빈 배열을 유지하며 출시용 오프셋·손가락 보정·커브를 임의로 만들지 않았다.

### 구현 범위

- `RunPlayableCharacterGrip.py` / `RunPlayableCharacters.py --step grip-check`는 각 외형을 새 프로세스와 격리 저장 프로필로 실행한다. 기본 3타와 액티브 5종을 실제 GAS로 발동하고, 무기 Anchor를 그대로 둔 실행과 월드 X 10cm·Z축 20도 회전을 적용한 실행을 비교한다. 이 변형은 해당 프로세스에만 존재하며 에셋에 저장하지 않는다.
- `PGHackSlashProbe`가 실제 무기의 이동과 매 공격 중 부착 관계·유한 Transform을 확인한다. 무기 루트의 손 기준 상대 위치·회전도 기록한다. 손목과 무기 원점 사이 거리는 저작한 손 접촉점 오차가 아니므로 그립 품질 점수로 사용하지 않는다.
- 기존 공간 판정의 타깃별 HP와 GAS 관측을 교차 사용해 공격 순서·피해·타격 수·대상·Phase·처리 횟수·이동량을 비교한다. 60Hz 고정 시간 실행의 타격·종료·Notify 시각 허용치는 `1/60 + 0.002`초로 고정한다. 실패 시 자동으로 확대하지 않는다.
- `PGAnimNotifyState_ToggleWeaponCollision`의 실제 Begin/End와 원본 메시 여부, 전투 컴포넌트의 충돌 요청/유효값, `PGPlayerSlashFX::Spawn`의 실제 호출 좌표·방향·반경을 기록한다. 표시 메시가 전투 Notify를 중복 발생시키거나 프로필 공격에서 레거시 무기 충돌이 켜지면 실패다. NullRHI 검사이므로 VFX의 화면 출력·입자 시뮬레이션 수용은 포함하지 않는다.
- 실행 시작 시 DLL·모듈 매니페스트·Config·Source·폴리싱/스킬 JSON을 실행 폴더의 `Project`에 복사하고 복사 전후 해시를 확인한다. 로그에서 `PGActor`와 `UPlayground`가 사본 DLL을 실제 로드했는지 검사하고, 종료 시에도 사본 해시가 같아야 한다. 원본 프로젝트의 다른 빌드가 비교 도중 DLL을 교체하는 문제를 막는다. 호출 명령, 원본 로그, 정규화 관측과 집계는 실행별 폴더에 남는다.

검증 사본의 `Content`는 원본 폴더로 향하는 디렉터리 연결이다. 이 게임 프로브는 Content를 저장하지 않지만, **Content 자체의 스냅샷이나 원본 아트 독립 복구 검증은 아니다.** 동일 사본으로 기존 런타임/자동 테스트를 할 때는 `RunPlayableCharacters.py`의 `runtime` 또는 `automation` 단계에 `--runtime-project '<실행>/Project/UPlayground.uproject'`를 전달한다. 이 옵션은 생성·내보내기 단계에서는 거부한다.

현재 플레이어 8개 공격은 `PlayerProfile` 공간 판정을 사용한다. 이번 게이트는 그 경로에서 표시 부착 변경이 전투를 바꾸지 않는지 검사한다. **레거시 무기 충돌을 실제 켜는 공격의 공간 회귀 수용은 아니다.** 또한 `PGAbilityUnEquipWeapon::CanActivateAbility`가 플레이어 납도를 거부하므로, 기존 설계의 플레이어 납도 연출은 현 플레이 흐름에 존재하지 않는다. 납도 연출의 도입/수용은 별도로 결정해야 한다.

### 접촉 좌표 후보 생성

`CalibratePlayableCharacterGrip.py`는 UE의 Transform 연산으로 다음 관계를 푼다.

```text
WeaponContact * WeaponRelative * GripOffset = HandContact
```

UE의 왼쪽→오른쪽 적용 순서다. 위치·회전을 정렬하고 명시한 `anchor_scale`을 보존해 무기 크기가 손 접촉 프레임 스케일에 따라 바뀌지 않게 한다. 현재 파일럿은 기존 `Weapon.Sword`와 양의 균일 스케일만 지원하며 비균일·음수·0 스케일과 비정상 Quaternion은 거부한다.

입력은 `schema_version: 1`, `profiles` 배열을 갖는 JSON이다. 각 항목은 다음 필드를 사용한다.

| 필드 | 좌표계/내용 |
|---|---|
| `identity`, `weapon_tag` | 기존 외형 ID, `Weapon.Sword` |
| `source_socket`, `target_socket` | 실제 장착 경로의 원본 소켓, 표시 메시의 부착 본/소켓 |
| `hand_contact` | 표시 부착 본/소켓 로컬의 저작한 손 접촉점과 방향 |
| `weapon_contact` | 무기 Actor 루트 로컬의 칼자루 접촉점과 방향 |
| `weapon_relative` | 장착 후 무기 Actor 루트의 Anchor 상대 Transform. 스폰/장착 스케일 규칙까지 반영 |
| `anchor_scale` | 보존할 Anchor의 양의 균일 스케일 |

세 Transform 객체는 `translation: [x,y,z]`, `rotation: [x,y,z,w]` 단위 Quaternion, `scale: 숫자` 형식이다. 위치는 **각 로컬 좌표의 임포트 단위**다. 월드 cm나 스태틱 메시 로컬 좌표를 무기 Actor 루트 좌표로 그대로 넣지 않는다. 대상 본의 스케일을 포함한 월드 접촉 오차와 손가락·칼자루 관통은 별도 연속 영상에서 판정한다.

```powershell
$uePython = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
& $uePython Tools/Validation/RunPlayableCharacters.py --step grip-check
& $uePython Tools/Validation/RunPlayableCharacterGrip.py --identity Bokusei
& $uePython Tools/Validation/RunPlayableCharacterGrip.py --calibration-self-test
# 아래 경로는 실제 접촉 좌표를 저작한 입력 파일이다.
& $uePython Tools/Validation/RunPlayableCharacterGrip.py --calibrate Saved/GripContacts.authored.json
& $uePython Tools/Validation/TestPlayableCharacterGrip.py
# 저장된 실행을 최신 검사기로 재검토하며 원래 report.json은 보존한다.
& $uePython Tools/Validation/RunPlayableCharacterGrip.py --review-run 'Saved/PlayableCharacters/Runs/<실행>_grip'
```

후보 생성은 기존 미내보내기 편집을 먼저 검사하고, 입력 키·소켓·Transform을 실제 외형에 대해 메모리에서 검증한 후 원래 값을 복원한다. `candidate.json`은 전체 폴리싱 원본 형식이며 `calibration-input.json`과 계산 잔차를 함께 보존한다. **자동 적용·에셋 저장은 하지 않는다.** 저작 좌표와 후보 diff를 검토한 뒤 원본 JSON에 반영하고 기존 `configure` 트랜잭션/새 프로세스 검증을 사용한다. 계산 잔차 0은 입력 프레임의 수학적 일치이며 시각 품질 수용이 아니다.

`--calibration-self-test`는 0.01/1/100 스케일과 서로 다른 회전·이동으로 계산을 검사하고, 실제 7종 외형에서 합성 프로필 직렬화·메모리 복원을 검사한다. 합성값이 실제 튜닝으로 오인되지 않도록 이 모드에서는 `candidate.json`을 만들지 않는다.

원본 JSON에 미적용 변경이 있어 실제 에셋과 다를 때도 후보 생성을 거부한다. 먼저 `configure` 또는 `polish-export`로 기준을 맞춘다. 후보 파일에서는 `grip_profiles`만 교체하고 나머지 외형 설정을 보존한다.

로그 재검토는 `review.json`에 별도로 저장한다. 원래 프로세스 실패, 실행 중 바이너리 변경, 요청 외형의 일부 누락은 재검토에서도 실패로 유지한다. 실행을 다시 한 것처럼 기록하지 않는다.

### 후속 검증 기록

경로는 `Saved/PlayableCharacters` 기준이다.

| 검사 | 결과·근거 |
|---|---|
| Development Editor 빌드 | PASS, `build-grip-regression.log`, `build-grip-final.log`. 이후 다른 작업의 빌드와 분리하기 위해 최종 런타임은 검증 사본의 DLL 해시를 기준으로 실행 |
| 접촉 좌표 계산·직렬화 | PASS, `Runs/20261005T062402695005Z_grip/calibration.json`. 0.01/1/100 배율의 이동·회전 조합과 Anchor 스케일 보존, 실제 7종 합성 프로필 검사·복원. 계산 위치 잔차 최대 약 `2.55e-5` 로컬 단위, 에셋 저장 0 |
| 비교기 실패 회귀 | `TestPlayableCharacterGrip.py` 7개 PASS. 피해·시간·이동 변경, 이벤트 누락/중복, 잘못된 Notify 원본·VFX 기준점, 무효한 자극·비유한 좌표·레거시 충돌 활성, 재검토의 프로세스 실패·검증 DLL 미로드·불완전 범위 보존 |
| 최종 7종 전후 전투 비교 | PASS, `Runs/20261005T061935558952Z_grip/report.json`. 14개 프로세스에서 112회 공격·224개 타깃 HP 검사, 충돌 Notify 308건·VFX 생성 요청 224건·부착 표본 13,020개. 7종 모두 전후 일치, 오류 0. 사본의 440개 입력 파일 해시 유지 및 사본 DLL 실제 로드 확인 |
| 기존 캐릭터 런타임 | PASS, `Runs/20261005T062442_runtime`. 검증 사본에서 플레이어 7종·P09 4종, 그립 캐시/복귀, 저장 실패·선택 차단과 유한 포즈 CSV 2,496행 |
| PG 자동 테스트 | PASS, `Runs/20261005T062606_automation/Automation/index.json`. 동일 검증 사본에서 45개 중 성공 38·경고 동반 성공 7, 실패/미실행 0 |

중간 실행 `Runs/20261005T054331109820Z_grip`은 낙성참(110)의 원형 VFX를 하나로 예상했던 검사 오류와 외부 Build.bat 잠금 대기로 실패했다. 기대 개수는 공격 JSON의 형태·타격 구간에서 읽도록 수정했다. 투사체는 선택적 `ProjectileSwingVFX`가 있으면 발사체 자체와 시전자 검기 두 요청을 허용하되, 전후의 실제 요청 목록은 같아야 한다. `Runs/20261005T055946157944Z_grip`은 개별 7종 비교가 모두 통과했지만 검사 도중 다른 빌드가 DLL을 교체하여 전체 동일성 검사는 실패했다. 이 두 실행은 최종 수용으로 집계하지 않는다.

## 2026-10-05 Bokusei 실제 검 그립 적용

첨부 화면의 Bokusei 오른손이 열린 채 손잡이와 떨어진 문제를 수정했다. 원본 대기 애니메이션 자체도 손이 열려 있으므로 무기 오프셋만 이동하는 것으로는 해결되지 않았다.

- `DA_Bokusei`의 `Weapon.Sword / RightWeaponSocket`을 `Hand_R`에 명시적으로 연결한다. `SM_Sword`의 실제 손잡이 축과 손바닥 좌표로 부착 회전·이동을 저작하고, 기존 무기 크기를 유지했다. 원본 파일은 `Tools/Validation/Data/PlayableCharacterGrip_Bokusei.json`; 계산 결과는 기존 `PlayableCharacterPolish.json`의 Bokusei `grip_profiles` 한 항목에만 반영했다.
- `FPGAppearanceGripFinger`의 참조 포즈 기준 로컬 회전으로 오른손 15개 손가락 본을 보정한다. `PGAppearanceAnimInstance`가 리타게팅/기존 P09 단위 복원 뒤에 적용하며 손목·팔·이동·원본 전투 포즈를 변경하지 않는다. 장착 상태는 게임 스레드에서 읽고, 작업 스레드는 캐시한 본 인덱스·회전·가중치만 사용한다. LOD 본 캐시는 재구성하며 누락된 본은 건너뛴다.
- 실제 장착 무기의 Anchor·무기 태그·원본 소켓이 해당 프로필과 일치할 때만 0.12초에 걸쳐 적용한다. 해제 시 원래 리타게팅 손 포즈로 복귀한다. 중복 프로필·무효 Transform/타깃의 기존 복귀 정책을 유지하고 손 밖 본·중복/누락 손가락 본은 저작 검증에서 거부한다.
- `CalibratePlayableCharacterGrip.py`는 손가락 포즈를 입력받고, 접촉점만 재보정할 때 기존 손가락 데이터를 보존한다. `RunGripPreview.py`는 후보를 메모리에만 적용하는 모드와 저장된 에셋의 근접 렌더/프레임 검사를 제공한다.

검증 근거는 `Saved/PlayableCharacters` 기준이다.

| 검사 | 결과·근거 |
|---|---|
| 최종 Development Editor 빌드 | PASS, `build-grip-final-fingers.log`. 별도 초상화 캡처가 사용하던 DLL 잠금 해제 후 정상 빌드 |
| 후보 계산 | `Runs/20261005T070655006108Z_grip`: 접촉 프레임 복원 오차 0, 원본 JSON 백업 `previous-source.json`; 다른 43개 폴리싱 설정의 변경 없음 확인 |
| 실제 에셋 저장·새 프로세스 재로드 | PASS, `Runs/20261005T070845507283Z_configure`: 기존 전체 패키지 백업·복구 절차, 44개 설정 의미 비교와 Portrait/전투 테이블 보존 |
| 손가락 직렬화·잘못된 데이터 거부 | PASS, `Runs/20261005T071404404713Z_polish-validate`: 비어 있지 않은 손가락 포즈 왕복, 누락 본·손 본 자체·중복 본 거부 및 메모리 원상 복귀 |
| 저장된 Bokusei 렌더·정렬 | PASS, `Runs/20261005T071052641435Z_grip-preview`: 대기 4장 + 기본 3타/액티브 5종/회피/스크립트 부착 해제·복귀 66장. 1,000개 표본에서 상대 위치 변화 0 로컬 단위, 최대 상대 회전 차이 `0.000004°`, 손가락 목표 회전 차이 0. 해제 후 닫힌 손 대비 `86.802°` 변화로 포즈 복귀 확인 |
| 접촉점 전후 | `Runs/20261005T064438724827Z_grip-preview/pose.csv`의 보정 전과 최종 캡처 비교. 저작한 손/무기 접촉 프레임 기준 위치 `7.5407cm → 0.0001cm 미만`, 방향 `36.2702° → 0°`. 이는 메시 표면 전체의 관통 검사 수치가 아님 |
| 실제 GAS 전투 회귀 | PASS, `Runs/20261005T071048705479Z_grip`: Bokusei 8공격 × 기준/부착 변경 2회, 피해·이동·타격 시각·Notify·VFX 요청 일치. `authored-before-after.json`은 기존 `061935558952Z_grip/Bokusei_baseline` 관측과 이번 실제 보정 적용 관측의 비교도 PASS로 기록 |
| 전체 캐릭터 런타임 | PASS, `Runs/20261005T071224_runtime`: 플레이어 7종·P09 4종, 외형 전환·저장 실패·공격 중 선택 차단·그립 캐시와 포즈 검사 |
| PG 자동 테스트 | PASS, `Runs/20261005T071232_automation`: 45개(성공 38, 경고 동반 성공 7), 실패/미실행 0 |

최종 근접 화면은 `Runs/20261005T071052641435Z_grip-preview/idle_1.png`와 `motion_10_05.png`, 해제 포즈는 `motion_09_05.png`다. 정렬 수치의 상대 좌표는 타깃 손 본 로컬 단위이며 접촉점 오차만 월드 cm다. 초기 결과 JSON의 `max_relative_drift_cm` 명칭은 이후 검사기에서 `max_relative_drift_local`로 정정했다(해당 실행의 값은 0).

재현:

```powershell
& $uePython Tools/Validation/RunPlayableCharacterGrip.py --calibrate Tools/Validation/Data/PlayableCharacterGrip_Bokusei.json
# candidate.json의 grip_profiles diff 검토 후 원본에 반영하고 기존 트랜잭션으로 저장한다.
& $uePython Tools/Validation/RunPlayableCharacters.py --step configure
& $uePython Tools/Validation/RunGripPreview.py --motion --calibration Tools/Validation/Data/PlayableCharacterGrip_Bokusei.json
& $uePython Tools/Validation/RunPlayableCharacterGrip.py --identity Bokusei
```

범위는 사진의 **Bokusei와 현재 한손검 메시**다. 플레이어 납도 입력/Ability는 기존 정책으로 비활성이어서 검증은 격리 프로필에서 동일한 부착·장착 상태 전환을 직접 실행했다. 실제 입력의 납도 기능을 새로 활성화하지 않았다. 다른 6종의 개별 보정, 다른 크기의 검 접촉 프레임, 연속 영상/직접 플레이·패키지 성능·발 IK·물리는 이번 완료 범위에 포함하지 않는다. 자동 렌더 결과의 `visual_acceptance=false`는 자동 검사만으로 전체 아트 수용을 선언하지 않기 위한 표기다.

## 2026-10-05 플레이어 8종 검 그립 확장

미보정 플레이어 8종의 `Weapon.Sword / RightWeaponSocket`에 개별 손 접촉 프레임과 오른손 15개 본의 참조 로컬 쥐기 포즈를 저장했다. Yura는 저장 ID `Hichi`를 사용한다. **Bokusei의 기존 설정, 손목·팔과 전투 모션, 검의 월드 크기는 유지했다.**

### 조사와 적용

- 대상 8종은 모두 `grip_profiles`가 비어 있었다. 공통 소켓 오프셋과 열린 손 포즈 때문에 칼자루가 손에서 벗어나 있었다.
- 실제 런타임 `pose.csv`의 참조 본·부모·회전·스케일에서 손의 길이/폭/손바닥 축을 구분했다. LianLian/Lili/Nenmir의 손 월드 스케일은 약 0.81, Honoka/Yura/Siuha/Hwarin/Arin은 약 81이다. 후자는 Anchor 스케일 0.01로 기존 검의 월드 스케일 약 1을 유지한다.
- `Tools/Validation/Data/PlayableCharacterGrip_Players.json`에 접촉점과 120개 손가락 회전을 보존하고 UE Transform 계산 결과를 `PlayableCharacterPolish.json`의 8개 `grip_profiles`에만 반영했다. 적용 전 원본과 비교해 그 외 필드와 Bokusei가 동일함을 확인했다.
- `ConfigurePlayableCharacterGrips.py`는 변경된 그립 외의 미적용 변경을 거부하고 기존 트랜잭션 백업·복구 및 의미 재로드 검증을 사용한다. 저장 패키지는 외형 에셋 8개뿐이며 초상화 참조도 재로드 후 비교했다.

접촉 오차는 **저작한 손/칼자루 프레임 사이의 월드 거리**다. 메시 표면 전체의 관통이나 의상 충돌 오차를 의미하지 않는다.

| 캐릭터 | 보정 전 거리 / 방향 | 저장 후 거리 | 최종 근접·동작 실행 |
|---|---:|---:|---|
| LianLian | 7.104cm / 36.191° | 0.0000004cm | `20261005T101438658781Z_grip-preview` |
| Honoka | 7.090cm / 38.082° | 0.0000235cm | `20261005T101529415203Z_grip-preview` |
| Yura (Hichi) | 8.090cm / 26.103° | 0.0000587cm | `20261005T101623727879Z_grip-preview` |
| Siuha | 6.562cm / 42.668° | 0.0000308cm | `20261005T101344109014Z_grip-preview` |
| Lili | 7.649cm / 29.580° | 0.0000003cm | `20261005T101202551890Z_grip-preview` |
| Nenmir | 7.104cm / 36.191° | 0.0000003cm | `20261005T101251258170Z_grip-preview` |
| Hwarin | 7.090cm / 38.082° | 0.0000443cm | `20261005T101712182745Z_grip-preview` |
| Arin | 7.104cm / 36.191° | 0.0000439cm | `20261005T101803825950Z_grip-preview` |

### 검증

실행 폴더는 `Saved/PlayableCharacters/Runs` 기준이며 집계는 `Saved/PlayableCharacters/AllGrips/verification.json`에 있다.

| 검사 | 결과·근거 |
|---|---|
| 후보 계산 | PASS, `20261005T101024395197Z_grip`. `previous-source.json`이 적용 전 원본이며 8개 그립 외 필드 동일 |
| 실제 에셋 저장 / 의미 재로드 | PASS, `20261005T101120929069Z_configure-grips`. 8패키지 백업, 트랜잭션 VERIFIED, 전체 폴리싱 원본 및 초상화 보존 검증 |
| 저장 에셋의 그립 동작 | 8종 모두 PASS. 각 기본 3타·액티브 5종·대시·스크립트 장비 해제/재장착. 6,908개 표본·560장 PNG. 상대 위치 변화 0, 손가락 목표 회전 차이 최대 0.000002° |
| 접촉 프레임 / 검 크기 | 위치 오차 최대 0.000059cm, 방향 오차 최대 0.000071°. 검 월드 스케일 약 1 유지 |
| 전체 외형 런타임 | PASS, `20261005T102035_runtime`. 플레이어 9종·P09 4종, 전환·저장 실패 원자성·공격 중 선택 제한·그립 캐시/복귀 |
| PG 자동 테스트 | PASS, `20261005T101359_automation`. 46개(성공 38, 경고 동반 성공 8), 실패/미실행 0 |
| 접촉 계산 자기 검사 | PASS, `20261005T101938028531Z_grip`. 0.01/1/100 배율 및 9종 프로필 직렬화. 동일한 반올림 Quaternion의 허위 회전 오차 검사 |
| Python 전투 비교 검사 | `TestPlayableCharacterGrip.py` 8개, `TestHackSlashMetrics.py` 8개 PASS. 정상 후반 타격 허용 및 범위 밖/누락 접점 거부 |
| 전투 부착 전후 비교 | PASS, `20261005T101202562464Z_grip/review.json`. Bokusei 포함 9종 × 8공격 × 기준/부착 변형 2회 = 144회 시전·288개 타깃 HP 검사. 피해·이동·타격 시각·Notify·VFX 요청 일치. 실행 중 사본 바이너리/입력 해시 유지 및 실제 사본 DLL 로드 확인 |

### 검사 도구 보완과 한계

- 근접 카메라를 손의 해부학적 축 기준 4방향으로 배치하고 카메라 모션 블러를 껐다. 동작 검사 시 66개 PNG의 존재도 확인한다.
- 접촉 계산기의 Quaternion 각도 비교는 반올림된 성분의 길이를 정규화한다. 기존 raw dot은 동일한 회전을 약 0.036° 오차로 오판했다. 오차 허용치를 늘리지 않았다.
- 전체 캐릭터 검사에서 Yura의 동기 메시 로딩이 0.5초 대기 시간을 소모하여 첫 평가 전 포즈를 검사하는 실패를 재현했다(`20261005T101357_runtime`). 대기 시작을 로딩 완료 뒤로 옮긴 검증 코드의 Development 빌드 후 전체 런타임이 통과했다. 그립 런타임 구현과 팔 포즈는 변경하지 않았다.
- UE 검증 프로세스에 `-Multiprocess`를 사용해 Turnkey의 중복 SDK 빌드 잠금 대기를 피한다. 초기 Hwarin/Siuha 실행의 타임아웃은 성공으로 집계하지 않고 저장 후 새 실행 결과만 위 표에 집계했다.
- 최근 공격 접점 확장으로 실제 모션은 총 24접점(110:5, 111:4, 112:6, 113:5, 나머지 각 1)을 사용한다. 기존 12개 템플릿 기준과 분리해 `--motion-contacts`로 명시적으로 검증한다. `RunPlayerSlashFX.py --all-phases`의 원본 접점 수와 동일하며, 타격/표현 개수·위치·시각·피해 비교와 원본 프로세스/DLL 실패 보존은 유지한다.
- 전투 실행의 구판 검사 `report.json`은 FAIL로 보존했다. 18개 프로세스 모두 실제 공간 검사와 정상 종료를 통과했고, 최신 접점 기준으로 보존 로그를 재검토한 `review.json`은 9종 모두 PASS다. 다시 실행한 결과로 표기하지 않으며 이 검사는 보정된 부착점에 추가 변형을 가한 전후 비교이지, 이전 미보정 에셋을 복구한 재실행은 아니다.
- LianLian은 원본 정면 T포즈부터 긴 소매가 손 전체를 덮는다(`Saved/MoonlitUI/ModelReferences/LianLian_Front.png`). 손 본과 검의 정렬·쥐기 포즈는 적용/검사했으나 소매 안 접촉면의 시각 수용 및 칼자루와 소매의 겹침은 해결하지 않았다. 의상 형상이나 물리는 수정하지 않았다.
- 자동 캡처의 `visual_acceptance=false`는 전체 아트 수용이 아님을 뜻한다. 다른 7종의 손등/측면 근접 화면에서 손잡이를 감싸는 포즈를 확인했다. 실제 입력 플레이·패키지 성능·다른 무기 메시 크기의 그립은 이번 범위가 아니다.

재현:

```powershell
$uePython = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
& $uePython Tools/Validation/RunPlayableCharacterGrip.py --calibrate Tools/Validation/Data/PlayableCharacterGrip_Players.json
# 후보의 grip_profiles만 원본에 반영한 뒤:
& $uePython Tools/Validation/RunPlayableCharacterPolish.py --step configure-grips
& $uePython Tools/Validation/RunGripPreview.py --identity Hichi --motion --calibration Tools/Validation/Data/PlayableCharacterGrip_Players.json
& $uePython Tools/Validation/RunPlayableCharacterGrip.py --motion-contacts
```

