# 서브컬처 UI 프레임 적용 기록

2026-10-11. 내장 ImageGen으로 아이보리 에나멜·샴페인 골드·옥색 모서리와 짙은 차콜 배경의 프레임을 생성했다. 원본/전체 프롬프트는 Tools/Art/ToonUI/Generation.json 및 T_PGToonFrame_Source.png에 보존한다.

## 적용

- 인벤토리/능력치: /Game/UI/Inventory/T_PGToonPanel, 256×256. 기존 ArtTextures UPROPERTY 캐시가 텍스처를 소유하며 위젯 멤버 브러시가 수명을 유지한다. 패널 내부 여백은 24다.
- 전투 HUD: /Game/UI/Combat/T_PGToonPlate, 128×128. 하단 스킬 바·스킬 버튼·시련 시작/장비/설정·성장 안내·보스 패널에 적용한다. 호버/누름/비활성 브러시와 기존 입력 처리를 유지한다.
- 두 텍스처 모두 Box 9분할, Margin=0.14. UE 5.8의 ElementBatcher는 브러시 ImageSize가 아닌 텍스처 실제 픽셀로 모서리 크기를 계산하므로 런타임 해상도를 분리했다. 원본을 알파 보존 리샘플링만 했다.
- DefaultGame.ini의 기존 Art 설정으로 연결한다. 두 폴더는 기존 DirectoriesToAlwaysCook 범위다. 게임 데이터/저장/스킬 동작은 변경하지 않는다.

## 재현

1. Tools/Art/ToonUI/PrepareTextures.ps1 실행.
2. UE 5.8 UnrealEditor-Cmd.exe에 프로젝트와 -run=pythonscript -EnablePlugins=PythonScriptPlugin -script=<프로젝트>/Tools/Art/ToonUI/ImportToonUI.py -nullrhi -unattended 전달.
3. UPlaygroundEditor Win64 Development 빌드 후 Tools/Validation/RunInventoryPresentation.py --cases open720,open1080,stats720,stats1080,hud720,hud1080,interaction 실행.

## 검증

- 임포트 PASS: Saved/ToonUI/import.json. 텍스처 형식·실제 크기·저장을 확인했다.
- 에디터 빌드 PASS: Saved/ToonUI/build.log.
- 첫 화면 검수에서 인벤토리 모서리가 비정상적으로 작게 표시되어, 에디터 비동기 텍스처 컴파일 완료 후 Slate 리소스를 등록하도록 보완했다. 수정 빌드 PASS: Saved/ToonUI/build-final.log.
- 최초 자동 렌더 검사는 기능/레이아웃 PASS였지만 아트 표시 문제로 시각 검수 FAIL이다. 첫 실행의 로그/화면은 C:/UsingProject/UnrealProject/UPlayground/Saved/QA/20261011T114519Z_788fc254_inventory에 보존한다. 수정 후 전체 대상 화면 재검증 PASS: `C:/UsingProject/UnrealProject/UPlayground/Saved/QA/20261011T114859Z_e4eeeb09_inventory/report.json`.
- 최종 7사례 PASS: 인벤토리/능력치/HUD 각각 720p·1080p 및 interaction 입력 회귀. 6개 해상도별 화면을 직접 열어 프레임 모서리·본문·아이콘/키 표시를 확인했다.
- 새 배포 패키지 빌드, 보스 전투 중 프레임 실화면, 장시간 물리 입력은 이번 검증에 포함하지 않았다.
- 최종 화면: [1080p 양쪽 패널](../../Saved/QA/20261011T114859Z_e4eeeb09_inventory/stats1080_PGInventory00193.png), [1080p HUD](../../Saved/QA/20261011T114859Z_e4eeeb09_inventory/hud1080_PGInventory00195.png).
- 최초 제한 환경 빌드는 사용자 AppData 접근 권한으로 중단됐다. 승인된 빌드 실행 경로로 재시도했다.

## 남은 범위

기존 배포 패키지는 자동 갱신되지 않는다. 물리 키보드/마우스 장시간 플레이 및 새 패키지 실행 검증은 별도다.
