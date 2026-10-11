# 인벤토리·능력치 UI — 2026-10-11

## 범위

서브컬처 카툰 렌더링에 맞춰 차콜 에나멜, 아이보리, 샴페인 골드와 셀 명암으로 전투 준비 화면을 구성한다. Diablo/POE 계열의 장착 장비·가방 그리드·옵션 비교 흐름을 사용하되, 기존 가방과 장착 규칙을 유지한다.

- `I` 또는 기존 HUD 장비 버튼으로 열고, 상단 `인벤토리` / `능력치` 탭으로 전환한다. 닫기는 기존 `I`·`Esc`·닫기 버튼을 사용한다.
- `능력치`는 현재 캐릭터 초상화, 공격, 생존·기동 세 영역이다. 초상화가 없는 기본 외형은 모험가 문장을 표시한다.
- 공격력·치명타 확률·치명타 추가 피해·최대 생명력·방어력·회복량·이동 속도는 기존 `UPGStatComponent::GetStat`으로 읽는다. 이 경로는 기존 GAS 수치에 연결되어 있다. 별도 계산·저장·능력치 분배는 추가하지 않는다.
- 기존 검술·강화, 전리품, 캐릭터 선택에도 같은 준비 화면 테마를 적용한다. 공유 HUD/보상 테마 설정은 변경하지 않는다.
- 가방의 긴 이름은 한 줄 말줄임으로 등급 표시와 분리한다. 툴팁·비교 영역은 전체 이름을 표시한다.
- 다중 칸 아이템, 드래그 배치, 새 장비 부위, 힘·민첩·지능·저항, 스탯 분배, 스킬 트리·성장은 후속 시스템 과제다. 미구현 수치를 가짜 값으로 표시하지 않는다.

## 에셋과 연결

내장 `image_gen`으로 생성했다. 최종 프롬프트 전문과 원본→런타임 매핑은 [Generation.json](../../Tools/Art/InventoryUI/Generation.json)에 있다.

| 원본 (`Tools/Art/InventoryUI`) | 크기 | 용도 |
|---|---|---|
| `T_PGInventoryFrame.png` | 1536×1024 | 전투 준비 공통 프레임 |
| `T_PGStatsFrame.png` | 1024×1536 | 공격·생존 능력치 패널 |
| `T_PGStatsEmblem.png` | 1254×1254, RGBA | 초상화 없는 기본 외형의 문장 |

Unreal 텍스처는 `/Game/UI/Inventory`에 저장한다. `PGUIStyleSettings`의 `InventoryFrame`, `StatsFrame`, `StatsEmblem`에서 교체한다. 한국어 텍스트·수치·입력은 네이티브 Slate이며 이미지에 굽지 않는다. 문장 알파를 보존하고 초상화는 원본 비율을 유지한다.

`PGInventoryStyle.h`는 준비 화면의 브러시를, `PGUIInventoryStats.cpp`는 읽기 전용 능력치 탭을 소유한다. 패키지 포함 경로는 `DefaultGame.ini`에 명시했다. 저장소의 기존 정책상 `Content` 바이너리는 Git 제외 상태이며, 원본 PNG와 임포트 스크립트는 추적 가능하게 등록한다.

## 재현

1. `UPlaygroundEditor Win64 Development`를 UE 5.8로 빌드한다.
2. Unreal Python commandlet에서 `Tools/Art/InventoryUI/ImportInventoryUI.py`를 실행한다. `-EnablePlugins=PythonScriptPlugin -run=pythonscript -script=<절대 경로> -nullrhi -unattended -DisablePlugins=RiderLink -ddc=InstalledNoZenLocalFallback`을 사용한다.
3. `python Tools/Validation/RunInventoryPresentation.py --cases open720,stats720,open1080,stats1080,interaction,states,design,character`를 실행한다. 매 프로세스마다 `UI_*` 격리 프로필을 만들며 사용자 저장을 변경하지 않는다.
4. `Automation RunTests PG.UI.`로 기존 UI 자동 테스트를 실행한다.

## 검증

- 1차 렌더·회귀 8개 PASS: `Saved/QA/20261011T040515Z_bd60a54b_inventory/report.json`.
- 능력치 렌더 프로브는 실제 Slate 텍스트 7개를 플레이어 수치와 대조한다. 두 해상도 모두 `STATS values=7 failures=0`을 확인했다.
- 아이템 선택/장착/삭제의 저장 실패 처리, 모달 입력 복귀, 검술 변경·취소·저장 실패, 캐릭터 선택 검사를 통과했다.
- `Saved/InventoryUI/Automation/index.json`: UI 자동 테스트 3개 성공, 실패·경고 0.
- 1차 육안 검사에서 프레임 모서리와 안내 문구의 겹침, 기본 외형의 빈 초상화 영역을 발견해 여백과 문장 표시를 보완했다. 최종 렌더 결과는 아래에 기록한다.
- 여백·문장 보완 후 720p/1080p 인벤토리·능력치와 검술 확인창 5개 PASS: `Saved/QA/20261011T041206Z_8442a898_inventory/report.json`. 두 해상도의 능력치 7개 대조도 통과했다. 문장의 실제 PNG 모서리 알파가 0임을 확인했고 네이티브 렌더에서도 배경이 드러난다.
- 최종 긴 이름 표시 보완 후 두 해상도 PASS: `Saved/QA/20261011T042456Z_d89d3a5a_inventory/report.json`. UE 5.8은 가운데 정렬 단일 행에서 말줄임 방향을 설정하지 않으므로 이름은 왼쪽 정렬을 사용한다. 최종 캡처에서 이름의 시작·말줄임과 등급 표시가 분리됨을 확인했다.
- 최종 UE 5.8 Development 에디터 빌드 성공: `Saved/InventoryUI/Build.log`. 에셋 3종 임포트·크기·저장 PASS: `Saved/InventoryUI/import.json`. 변경 파일 `git diff --check` 통과.

최종 화면: [인벤토리 1080p](../../Saved/QA/20261011T042456Z_d89d3a5a_inventory/open1080_PGInventory00097.png), [능력치 1080p](../../Saved/QA/20261011T041206Z_8442a898_inventory/stats1080_PGInventory00090.png).

패키지 빌드와 직접 마우스·키보드 장시간 플레이는 별도다. 이번 수용 범위는 UI 생성·기존 데이터 연결이며, 인벤토리·스탯·스킬 시스템 고도화 완료를 뜻하지 않는다.
