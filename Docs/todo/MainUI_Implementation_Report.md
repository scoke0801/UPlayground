# 메인 HUD

## 2026-09-21 디자인 개편

- 장식형 금속 프레임을 제거하고 네이비 반투명 패널, 민트 생명력, 라벤더 분노/회피, 둥근 스킬 카드로 구성했다.
- 좌측 상단은 시련·웨이브·목표, 우측 상단은 장비/빌드 버튼이다. 하단 중앙은 얇은 리소스 게이지와 8개 스킬 카드다.
- 하단 조작 안내와 슬롯별 키 매핑 라벨을 제거했다. 입력 매핑 조회도 제거했으며 실제 키 바인딩은 변경하지 않았다.
- 체력 25% 이하는 붉은색으로 표시한다. 스킬은 호버/눌림 테두리, 쿨다운 음영과 남은 시간을 표시한다.
- 플레이어 발밑 HP 위젯은 표시·틱·갱신을 제거했다. 기존 BP 호환을 위해 PlayerHpWidget 이름의 컴포넌트는 유지하고 BeginPlay에서 다시 숨김 및 위젯 해제를 적용한다. 적 네임플레이트는 유지한다.

## 연결과 수명

APGPlayerController가 로컬 플레이어에게 UPGUIMainHUD를 생성한다. bUseLegacyHUD를 켜면 기존 HUDWidgetClass를 사용한다.
GAS 생명력/분노, 스킬 핸들러 쿨다운, 스테이지 상태, 기존 인벤토리에 연결한다. 스킬 클릭은 기존 ASC 입력 경로를 사용하고 빈 슬롯·쿨다운·행동 불가 상태에서는 비활성화한다.

0.1초 타이머로 현재 Pawn을 조회하며 제거 시 타이머를 해제한다. 스킬 아이콘은 ID 변경 시에만 로드한다. SafeZone과 축소 전용 ScaleBox를 적용하며 중앙 전투 영역은 히트 테스트를 차단하지 않는다.

## 리소스와 참고

새 프레임은 Slate 벡터 브러시로 그리므로 별도 텍스처 가져오기가 필요 없다. 기존 T_MainHUDPlate와 Tools/Art/MainUI 원본은 더 이상 메인 HUD에서 사용하지 않는다. 스킬 아이콘은 기존 데이터 테이블을 유지한다.
디자인 방향 참고: [원신 공식 PlayStation 소개](https://www.playstation.com/en-us/games/genshin-impact/), [젠레스 존 제로 공식 영상](https://zenless.hoyoverse.com/m/en-us/video?catchSpider=1&id=124310). UI/아트 에셋 복제 없이 정보 위계와 간결한 표현을 프로젝트에 맞게 구성했다.

## 검증

- UE 5.8 Development Editor 빌드 성공: Saved/Logs/CartoonHUDBuild.log.
- 별도 StageDevMap 실행 및 1920×1080 실제 게임 캡처 확인: Saved/Screenshots/WindowsEditor/PGMainHUD00002.png.
- 캐릭터 발밑 HP 바와 하단 키 안내 제거, 한글·게이지·스킬 아이콘·웨이브 텍스트 표시 확인.
- 실행 로그: Saved/Logs/CartoonHUDPreview.log. 테스트 프로필을 사용해 실제 플레이 저장과 분리했다.
- 낮은 체력 색상 전환, 쿨다운 전환, 클릭/호버, 리스폰은 구현했지만 이번 정지 화면 검증만으로 동작 검증을 완료한 것은 아니다.
- 1280×720 실제 게임 캡처도 확인: Saved/Screenshots/WindowsEditor/PGMainHUD00003.png. 상·하단 패널 겹침과 잘림 없음. 실행 로그: Saved/Logs/CartoonHUDPreview720.log.

## 모션 기반 아이콘 (2026-09-21)

- 기존 외부 아이콘을 ImageGen으로 제작한 8종으로 교체했다. 원본은 Tools/Art/SkillIcons, 런타임은 /Game/UI/SkillIcons이며 DT_Skill.SkillIconPath로 연결한다. 슬롯 내부 여백을 줄여 식별 크기를 키웠다. 모션 근거와 궁극기 미지정 상태는 SkillIcons_Implementation_Report.md를 참고한다.
