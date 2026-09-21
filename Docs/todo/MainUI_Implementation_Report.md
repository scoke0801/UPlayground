# 메인 HUD

## 적용

`APGPlayerController`는 로컬 플레이어에게 `UPGUIMainHUD`를 생성한다. 기존 HUD 블루프린트를 수정할 필요가 없다. 컨트롤러의 `PG|UI > Use Legacy HUD`를 켜면 기존 `HUDWidgetClass`로 돌아간다. 기존 보상 창, 인벤토리, 적 네임플레이트와 데미지 표시는 유지한다.

## 표시와 조작

- 좌측 상단: 현재 스테이지, 남은 적, 보상 선택·완료·실패 상태.
- 하단: GAS 생명력/분노, 일반 공격·6개 스킬·회피, 실제 스킬 쿨다운.
- 단축키: 플레이어 InputConfig와 Enhanced Input의 현재 매핑에서 읽는다. 장착 스킬에 현재 매핑이 없으면 `클릭`을 표시한다(예: 무기 입력 컨텍스트 미활성 상태).
- 스킬 버튼: 기존 ASC 입력 처리로 전달한다. 빈 슬롯·쿨다운·행동 불가 상태에서는 비활성화한다.
- 우측 상단 장비 버튼과 I 키: 기존 장비/빌드 인벤토리.
- 중앙 전투 영역은 히트 테스트를 차단하지 않는다. SafeZone과 축소 전용 ScaleBox를 사용한다.

위젯은 0.1초 타이머로 표시를 갱신한다. 플레이어 재생성 시 현재 Pawn을 다시 조회하며 제거 시 타이머를 해제한다. 스킬 아이콘은 슬롯의 스킬 ID가 바뀔 때만 로드한다. 수치는 GAS와 스킬 핸들러가 원본이며 UI가 별도 쿨다운을 진행시키지 않는다.

## 리소스

ImageGen으로 생성한 원본: `Tools/Art/MainUI/T_MainHUDPlate.png`.
런타임 텍스처: `/Game/UI/Main/T_MainHUDPlate`.
재가져오기: Unreal Python으로 `Tools/Validation/ConfigureMainUIAssets.py` 실행.
Content는 별도 저장소이므로 텍스처 uasset도 함께 관리해야 한다.

## 검증

- UE 5.8 Development Editor 최종 빌드 성공: `Saved/Logs/MainUIBuildFinal.log` (92023 suffix).
- 실행 중인 기존 에디터의 DLL 잠금을 피하기 위해 별도 이름 DLL로 링크했다. 열려 있는 에디터에 새 코드가 자동 적용되었다는 뜻은 아니며, 새 에디터 실행 시 로드한다.
- 자동 회귀 검사 2개 통과 / 실패 0: `PG.CombatCycle.RewardSubmission`, `PG.Stage.Lifecycle`. 결과: `Saved/Automation/MainUI/index.json`.
- 모듈 의존성 검사 통과. ImageGen 텍스처 가져오기 및 저장 성공.
- 화면 검증 스크립트: `Tools/Validation/PreviewMainUI.py`. 별도 에디터에 `-ExecCmds="py <스크립트 절대 경로>" -PGTestProfile=MainUI -RenderOffscreen -unattended`로 실행한다.
- 개발 콘솔 `PGHUDCapture`: 3초 뒤 UI를 포함한 스크린샷을 저장하고 최상위 메인 HUD 개수를 로그로 남긴다. Shipping에서는 동작하지 않는다.
- 독립 게임 1920×1080 실행에서 `native HUD count=1` 확인. 실제 GAS 체력, 스킬 아이콘, 스테이지 남은 적 표시와 레이아웃을 화면으로 확인했다. 로그: `Saved/Logs/MainUIFinalPreview.log`.

타이틀 메뉴는 이번 인게임 HUD 범위에 포함하지 않는다. 최종 자연 플레이의 클릭/키보드 입력, 초광폭·작은 화면 가독성, 장시간 플레이와 패키지 빌드는 별도 확인이 필요하다.
