# 전투 HUD 재제작 — 2026-10-09

## 디자인 기준과 조사

- [Diablo IV 공식 UI 설계 글](https://news.blizzard.com/en-us/article/23308274/diablo-iv-quarterly-updatefebruary-2020): PC 하단 중앙 액션바, 낮은 배경 채도, 재질과 대비를 통한 정보 위계. 이 글은 2020년 개발 과정의 설명이며 현재 제품 사양으로 인용하지 않는다.
- [Path of Exile 2 UI 가이드](https://mobalytics.gg/poe-2/guides/user-interface): 생명력·자원·플라스크·스킬을 빠르게 식별하는 전투 정보 구성 참고.
- 레퍼런스의 로고·원본 이미지·프레임은 복제하지 않았다. 정보 구조는 핵앤슬래시 방식으로 구성하되 **최종 이미지는 사용자 지시에 따라 서브컬처/애니메이션 판타지 게임풍**으로 제작한다. 차콜 에나멜·샴페인 골드·아이보리, 선명한 윤곽과 넓은 셀 명암을 사용하며 생명력은 루비색, 격분은 호박색이다.

## 적용

- `PGUIMainHUD` 하단 중앙: 생명력 구슬 → 회복약·스킬 슬롯 → 격분 구슬. 실제 GAS 수치에 따라 액체 높이를 채우며 빈 자원도 금속 테두리와 숫자로 식별한다. 쿨다운 숫자, 현재 입력 키, 환급·잔상 준비 표시와 툴팁을 유지한다.
- 우측 상단: 시련 제목·현재 목표·시작·장비를 한 영역으로 묶었다. 목표는 별도 대형 패널 없이 그림자가 있는 아이보리 문자와 황동 구분선으로 표시한다. 전투 시작 버튼이 사라져도 장비 버튼은 우측 정렬을 유지한다.
- 좌하단의 미획득 강화 안내를 제거했다. 획득한 강화만 전투 바 위에 표시하며 발동/대상 상태를 한 줄로 요약한다. 긴 상태는 생략 부호로 잘라 전투 바 크기를 늘리지 않는다.
- 준비 중 스킬 아이콘의 과도한 회색 처리를 없애고 반복 문구를 툴팁으로 옮겼다. 클릭은 기존의 조작 가능·장착·쿨다운 조건을 통과한 경우만 전송한다.
- `PGCombatHUDStyle`은 전투 HUD 전용이다. 인벤토리·보상 창의 테마 및 전투 데이터는 이번 변경에 포함하지 않는다.
- 하단 레이아웃은 900×226 논리 좌표와 축소 전용 ScaleBox를 사용한다. 회복약 알림·강화 상태 영역을 예약해 상태 변화 시 버튼 위치가 바뀌지 않는다.

## 아트와 재현

- 내장 `image_gen`으로 제작한 최종 원본: `Tools/Art/CombatHUD/T_PGCombatOrbFrame_Anime.png`, `T_PGCombatPlate.png`, `T_PGCombatPotion.png`. 구슬 테두리·스킬바/버튼 프레임·실제 회복약 버튼에 각각 연결한다. 가로 프레임은 투명 여백을 UV로 제외하고 Image 방식으로 표시한다. 고해상도 원본의 모서리 픽셀 크기를 사용하는 Box 슬라이스는 채택하지 않는다.
- 정확한 최종 제작 프롬프트: `Tools/Art/CombatHUD/AnimeGeneration.json`. `Generation.json`과 `T_PGCombatOrbFrame.png`는 사용자 그림체 보정 전의 **미사용 초안**으로 보존한다.
- 런타임: `/Game/UI/Combat/T_PGCombatOrbFrame`, `T_PGCombatPlate`, `T_PGCombatPotion`. `PGUIStyleSettings`의 `CombatOrbFrame`, `CombatPlate`, `CombatPotionIcon`으로 교체할 수 있다. 누락 시 기존 네이티브 브러시·병 도형으로 표시한다.
- `ImportCombatHUD.py`는 UI 텍스처 그룹·알파·sRGB를 설정하고 저장한다. 구슬 액체와 유리 반사광은 Slate에서 그린다.
- UE 5.8 Development 에디터 빌드 후 아래 명령을 실행한다. 해상도 검사는 GPU 초기화 부하 때문에 **순차 실행**한다.

```powershell
python Tools/Art/CombatHUD/RunCombatHUD.py --step all --width 1600 --height 900
python Tools/Art/CombatHUD/RunCombatHUD.py --step runtime --width 1280 --height 720
python Tools/Art/CombatHUD/RunCombatHUD.py --step runtime --width 1920 --height 1080
```

- 실제 플레이: `Tools/PlayForestRuins.ps1`.
- 검사마다 고유 `PGTestProfile`·`UserDir`을 사용한다. 일반 플레이 저장은 사용하지 않는다.

## 검증 기록

- UE 5.8 `UPlaygroundEditor Win64 Development` 빌드 통과. 기존 엔진 deprecated 경고와 기존 모듈 순환 참조 경고는 남아 있다.
- `Saved/CombatHUD/20261009T055545129209Z/report.json`: 아트 임포트, 1600×900 실제 회복약 검사 PASS.
- `Saved/CombatHUD/20261009T060112536097Z/report.json`: 1920×1080 실제 회복약 검사 PASS.
- `Saved/CombatHUD/20261009T060355543836Z/report.json`: 1280×720 단독 재실행 PASS.
- 검사는 준비·체력 25%·회복 후 65%·소진 캡처, HUD 버튼 콜백, Q 누름 유지, 경직·공격·대시 중 회복, 인벤토리 일시정지, 프로필 기록·웨이브 전환·구간 보급·사망 제한과 HUD 크기/위치 불변을 포함한다. 밸런스 수용 검사가 아니다.
- 동시 실행했던 720p 검사(`20261009T060111392846Z`)는 그래픽 초기화 지연 때문에 중단했다. 실패 기록은 보존하고 위 단독 PASS로 대체한다.
- 첫 별도 프리뷰(`20261009T060606978407Z`)는 이미 존재하는 출력 폴더의 생성 오류로 실패했다. `exist_ok=True`로 도구를 수정했다.
- 구슬 가로 음영 방향의 마지막 보완은 위 기능 검사 이후에 적용했다. 최종 시각 검사 근거는 아래에 기록한다.
- `20261009T060657540783Z`: 준비·격분 10/10·보스전 시각 검사 PASS. 이후 사용자 지시로 거친 실사 금속 초안을 서브컬처 그림체로 교체했으므로 이 캡처도 중간 결과다.

### 서브컬처 아트 최종 적용 검증

- `Saved/CombatHUD/FinalBuild.log`: 최종 프레임 표시 수정이 포함된 Development 빌드 성공.
- `Saved/CombatHUD/20261009T064329322221Z/report.json`: 서브컬처 아트 3개 임포트·900p 회복약 검사·준비/격분/보스 캡처 PASS. 시각 검토에서 고해상도 Box 슬라이스의 모서리 확대를 발견해 Image 표시로 보완했다.
- `Saved/CombatHUD/20261009T073916039349Z/report.json`: **최종 버전 1280×720** 회복약·입력·레이아웃 검사와 정상 종료 PASS.
- `Saved/CombatHUD/20261009T074054088329Z/report.json`: **최종 버전 1920×1080** 동일 검사와 정상 종료 PASS.
- 최종 준비 화면에서 상단 목표/시작/장비, 하단 생명력/격분과 이미지 기반 회복약/스킬바를 확인했다. 회복·소진 상태에서 자원 수치와 쿨다운, 프레임 안의 문자 배치가 유지된다.
- `20261009T070055857038Z`: 최종 900p 이미지 세 장은 정상 생성됐지만 로그를 닫은 뒤 프로세스가 `0xC0000005`로 종료돼 전체 실행은 FAIL로 유지했다. 로그에 콜스택이나 새로운 CrashContext는 없어 원인을 단정하지 않는다. 이 실패를 성공으로 재분류하거나 검사에서 제외하지 않았다.
- `Saved/CombatHUD/20261009T074253698727Z/report.json`: **최종 1600×900 준비·격분 10/10·보스전 프리뷰와 정상 종료 PASS**. 같은 조건의 새 프로세스에서 위 종료 오류는 재현되지 않았다. 최종 화면은 해당 실행의 `presentation/01_Preparation.png`, `02_Frenzy.png`, `03_Boss.png`이다.

## 범위와 후속 폴리싱

- 패키지 쿠킹, 초광폭·컨트롤러 UI, 장시간 다수 몬스터 전투 성능은 이번에 검증하지 않았다.
- 기존 스킬 그림은 보존했다. 새 프레임과 비교해 아이콘별 선 굵기·명암을 통일하는 별도 아트 패스는 후속 폴리싱 범위다.
