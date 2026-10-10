# 액션 카메라 상하 입력·지형 충돌 수정

2026-10-09, UE 5.8.2 / Development Editor.

## 원인과 변경

- 액션 모드는 `GetInputMouseDelta`의 Y를 빼고 있었다. 요청대로 더하도록 반전했다. 마우스 변위에는 프레임 시간을 곱하지 않는다. 기존 Pitch -80°~65°, Yaw 정규화, Roll 0, UI/Alt/포커스 입력 차단과 모드별 줌 저장은 유지한다.
- `BuildForestRuins.py::solid`가 모든 전투 지형의 Camera 응답을 Ignore로 지정했다. 기존 숲 맵의 연속 지면·석재 외곽 경계·장애물 4개 모두 동일했다. 생성 코드와 저장 맵의 해당 6개 응답을 Block으로 수정했다.
- `ConfigureQuarterView`에서 블루프린트 기본값 적용 후 Spring Arm 충돌을 켜고 ECC_Camera와 반경 24cm를 적용한다. 자식 카메라 상대 위치/회전은 0으로 설정해 충돌 검사 종점과 실제 시점을 일치시킨다. 프레이밍은 기존 Spring Arm SocketOffset/TargetOffset을 사용한다.
- 회전 지연은 끄고 이동 추종은 1/60초 서브스텝과 최대 지연 거리 60cm로 제한한다. `PGQuarterViewData.CameraProbeRadius`, `CameraMaxLagDistance`로 튜닝한다. 기존 줌 보간을 유지하며 충돌 보정은 사용자의 요청 거리를 덮어쓰지 않는다.

## 조사 근거

- [Epic: Spring Arm API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/USpringArmComponent): 구체 스윕, 충돌 시 거리 축소, 장애물 해제 시 복원, 소켓 오프셋과 이동 지연 설정. 설치된 5.8 엔진 `SpringArmComponent.cpp`와 대조했다.
- [Epic: Collision Overview](https://dev.epicgames.com/documentation/unreal-engine/collision-in-unreal-engine---overview?lang=en-US): 카메라 추적을 차단하려면 지형의 해당 Trace Response가 Block이어야 한다.
- [Epic: Third Person Template](https://dev.epicgames.com/documentation/unreal-engine/third-person-template-in-unreal-engine?lang=en-US): 캐릭터 뒤·위의 시점, 마우스 시점 조작과 WASD 이동을 기본으로 삼는다. 이 프로젝트는 기존 카메라 기준 이동을 유지했다.

## 검증

- `Saved/QA/CameraModes/action-build.log`: Development 빌드 성공. 기존 엔진 폐기 예정 API·컴파일러 권장 버전 경고가 있다.
- `Saved/QA/ActionCameraCollision/Automation/index.json`: `PG.Camera.*` 3개 성공, 실패 0. 실제 물리 월드의 바닥·벽·경사면, 120/30/10Hz, 이동 추종, 장애물 제거 후 거리 복원, 모드 전환·상하 입력·설정 저장 검사다. 1개 테스트의 경고는 기존 GameplayCueNotifyPaths 미지정이다.
- `Saved/QA/ActionCameraCollision/verify.json`: 새 프로세스에서 충돌체 6개 Block 재로드, 실제 숲 바닥의 Camera 스윕 중심 Z=23.999995cm 확인.
- `Saved/QA/ActionCameraCollision/Presentation/report.json` 및 `UpwardGroundCollision.png`: 실제 BP_LocalPlayer로 PIE 실행, 상향 65°, 충돌 보정 활성, 카메라 중심 Z=23.999999cm. 렌더 이미지를 확인했다. 에디터 시작 시 한국어 로캘의 기존 smoke test Condition failed 로그와 다중 방향광 화면 경고는 별도로 존재한다.

## 적용·재현

- `Tools/Validation/ConfigureActionCameraCollision.py`를 UE Python commandlet로 실행하면 해당 맵을 백업하고 6개 충돌체만 수정한다. `-PGCameraCollisionVerify`를 추가하면 저장 없이 재로드·스윕 검사만 한다.
- 최초 백업: `Saved/QA/ActionCameraCollision/Backup_20261009_230436/L_PG_ForestRuins.umap`. 최초 적용은 저장 후 Python HitResult 파싱에서 실패했고, 파싱 수정 후 새 프로세스 검증이 통과했다. 초기 로그와 백업을 보존한다.
- 렌더: `-ExecutePythonScript=Tools/Validation/PreviewActionCameraCollision.py -PGTestProfile=ActionCameraCollision -RenderOffscreen`을 사용한다. 테스트 중 ActionPitch와 카메라 모드만 메모리에서 변경하고 복원하며 환경 설정을 저장하지 않는다.

## 범위와 남은 폴리싱

마우스 부호는 자동 검사로, 실제 지형 관통은 물리 검사와 렌더로 확인했다. 장시간 직접 마우스 조작감과 좁은 공간의 캐릭터 가림은 별도 플레이 검수 범위다. 충돌 해제는 엔진 기본 즉시 복원이다. 장식용 NoCollision 식생·고디테일 메시에는 새로운 충돌을 만들지 않으며 기존 전투 지형의 충돌 프록시를 사용한다.

## 후속 수정: 플레이어 내부 노출·몬스터에 의한 카메라 당김

- `EnemyCharacter`에 Camera Ignore 응답이 없어 기본 Block이 적용됐다. 프로필을 수정하고 `APGCharacterEnemy.PostInitializeComponents`에서 캡슐·메시·신체 히트박스의 Camera 응답을 Ignore로 확정한다. 기존 BP의 개별 오버라이드도 적용 후 보정한다. `APGWeaponBase`에도 동일한 정책을 적용해 자신 또는 적의 장비에 카메라가 당겨지는 일을 막는다. 나머지 충돌 채널과 전투 판정은 유지한다.
- Spring Arm의 충돌 보정 거리는 사용자가 지정한 줌 최소 거리보다 짧아질 수 있다. 최소 거리를 강제하면 벽을 뚫게 되므로 지형 스윕은 유지한다. 최종 렌더 시점이 플레이어 캡슐에 가까워지면 `APGPlayerController.UpdateHiddenComponents`에서 플레이어 메시·재귀 부착 장비를 해당 뷰의 숨김 목록에 추가한다. 외형 자체의 Visibility나 HiddenInGame은 변경하지 않는다.
- 캡슐 표면에서 `CameraBodyClearance`(45cm) 이내에 진입하면 숨기고, 추가 `CameraBodyHideHysteresis`(15cm)만큼 벗어나야 복원한다. 외형별 튜닝은 `PGQuarterViewData`의 Collision 항목에서 한다. 3D 액션에만 적용하며 쿼터뷰·다른 ViewTarget·리스폰의 새 캐릭터에서는 이전 숨김 상태를 이어받지 않는다.
- 회귀 검사에 BP 형태의 기존 Camera Block 응답 보정, 몬스터/장비 무시, 적과 벽 동시 존재, 근접 임계·히스테리시스, 시점별 외형/장비 제외와 복원, 다른 시점·모드 전환을 추가했다. 기존 지면·경사면·프레임률 검사를 유지한다.
- 이번 실행 근거는 `Saved/QA/CameraBodyCollision`에 보관한다. `build-tests-final.log`의 UE 5.8 Development Editor 빌드 성공, `AutomationVerified/index.json`의 카메라 테스트 3개 통과(경고 포함 1개, 실패 0)를 확인했다. 경고는 기존 GameplayCueNotifyPaths 미지정이다. 초기 검사에서 테스트 월드의 액터 초기화 누락 및 이동 추종 보간을 위치 완전 일치로 비교하던 문제를 수정했으며 초기 실패 보고서도 보존한다.
- `Presentation/report.json`과 `UpwardGroundCollision.png`에서 실제 BP_LocalPlayer의 상향 65°·바닥 충돌 보정·카메라 중심 Z=24cm를 확인했고, 렌더에서 플레이어 내부와 장비가 화면을 덮지 않는 것을 확인했다. 기존 다중 방향광 경고는 남아 있다. 이 렌더는 바닥 근접의 정지 화면 검사이며 모든 외형·연속 조작의 수용 검사를 대신하지 않는다.
- 근접 숨김은 즉시 전환이며 투명도 페이드는 포함하지 않는다. 모든 외형의 돌출 장식과 밀집 전투에서의 장시간 조작감은 직접 플레이 검수 범위다.
