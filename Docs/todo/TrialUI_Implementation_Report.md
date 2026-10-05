# 도전 결과 UI · 한국어 표기

## HUD · 전투 준비 후속 개선

- 시련 안내, 전투 준비/시작 동작, 강화 공명 HUD에 별도의 ImageGen 프레임을 적용했다. 시련 이름과 준비/전투/종료 상태를 구분하고, 강화 공명은 GAS의 실제 출혈·충격파·격분 활성/전용 강화/핵심 획득 상태를 표시한다. 기존 중첩·발동 안내는 유지한다.
- 전투 준비 창은 투명한 은빛 달 프레임을 배경 일러스트 위에 겹친다. 닫기 버튼에는 `닫기`만 표시하며 I/Escape 동작과 미적용 검술 확인 흐름은 유지한다. 빈 가방 칸의 반복 문구도 제거했다.
- 검술 설명은 `PGPlayerSkillText::MakeView`에서 이름·공격 방식·총 피해·기본 재사용 시간을 분리한다. 실제 공격 프로필의 판정 형태, 도약 여부, 타격 수와 배율로 설명을 구성한다. 회피 취소 시각, 판정 원판/각도 등 개발용 세부 수치는 UI에서 제외한다.
- 강화 창은 획득한 계열의 효과와 선택한 강화를 먼저 표시한다. 아무 계열도 없다면 다음 시련에서 강화를 고르는 방법을 안내하며, 미획득 계열의 상태 나열은 표시하지 않는다. 공통 효과 문구는 행동과 결과를 설명하는 한국어 문장으로 정리하고 표시 범위 단위를 m로 맞춘다.
- 새 원본: `Tools/Art/TrialUI/T_HUDPlaque.png`(2172×724), `T_MenuFrame.png`(1536×1024). 두 이미지 모두 내장 `image_gen` 생성이며 알파를 보존한다. 최종 프롬프트는 `hud-generation.json`, 가져오기는 `ConfigureHUDFrames.py`다. 런타임 에셋은 `/Game/DataCenter/UI/Moonlit`이며 `PGUIStyleSettings.HUDPlaque/MenuFrame`에서 교체한다.
- UE 5.8 Development 최종 빌드 성공: `Saved/HUDPolish/build-final.log`. 프레임 2종 가져오기·크기·알파 검사 통과: `Saved/HUDPolish/import.json`.
- HUD 720p/1080p, 장비, 검술, 변경 적용/저장 실패 재시도, 닫기/반복 열기 총 6사례 통과: `Saved/QA/20261005T094555Z_faa8dfe5_inventory/report.json`. 프레임 장식과 제목·하단 안내의 간격을 넓힌 뒤 장비 720p·검술 1080p·변경 후 닫기 3사례 재검증 통과: `Saved/QA/20261005T095520Z_0d22cc18_inventory/report.json`. 최종 캡처에서 제목·설명·닫기 확인 영역이 프레임과 겹치지 않음을 확인했다.
- 관련 UI 자동 테스트 3개(InventoryComparison, LootLabelLayout, RewardBuildContext) 통과, 경고·실패 0개: `Saved/HUDPolish/UIAutomation/index.json`. 전체 PG 테스트 시도는 별도 모션 테스트 `FPGPlayerMotionSwingTest`의 배열 요소 자기 추가 assertion으로 중단됐다(`Saved/HUDPolish/automation.log`). 이번 후속 변경에 대해 전체 테스트 통과로 간주하지 않는다.
- 공통 강화 문구를 사용하는 보상 선택 720p 사례도 통과했다: `Saved/QA/20261005T095923Z_294f9696_reward_loot/report.json`.
- 화면 검증은 격리 프로필·자동 입력으로 수행했다. 직접 조작 체감, 성능, 실행 중인 DebugGame 편집기에 대한 반영은 이 검증에 포함하지 않는다.

## 변경 범위

- 실패 화면을 보상 선택 화면과 분리해 중앙의 작은 결과 패널로 구성한다. 종료 안내, 실제 도달 구간과 누적 강화 선택 횟수, 재도전 버튼, 초기화 안내 순서로 표시한다. 내용 영역은 스크롤하고 동작 버튼은 고정한다.
- 실패·강화 선택·승리 화면에 ImageGen으로 생성한 달빛 성소 프레임을 사용한다. `PGUIStyleSettings.TrialFrame`으로 교체할 수 있으며 참조가 없으면 기존 공통 패널을 사용한다. 텍스처는 위젯의 transient UObject 참조로 유지한다.
- HUD의 영어 시련 제목, 전투 준비의 영어 성소/검술 제목, 보상·결과의 영어 장식 제목, 레거시 구간 표기를 한국어로 바꾼다. 실제 키 표시는 유지한다.
- Stage의 진단 원문은 로그·이벤트·진행 기록에 유지한다. 화면에서는 사망·저장 실패·기타 중단에 맞는 한국어 안내를 사용한다. 성장 데이터 검증 실패도 한국어로 표시한다.
- 보상 토큰, 지급, 저장 확정, 재도전 권한과 입력 소유권은 기존 경로를 유지한다.

## 아트 원본과 재현

- 원본: `Tools/Art/TrialUI/T_TrialFrame.png` (1536 × 1024).
- 내장 `image_gen` 사용. 전체 최종 프롬프트와 출력 경로는 `Tools/Art/TrialUI/generation.json`에 보존한다. 핵심 사양은 어두운 남색 내부, 얇은 은빛 테두리, 달 장식, 민트·라벤더의 절제된 장식, 문자가 없는 프레임이다.
- 런타임: `/Game/DataCenter/UI/Moonlit/T_TrialFrame`. 기존 DataCenter 패키징 범위에 포함한다.
- 가져오기: Unreal Python 명령릿에서 `Tools/Art/TrialUI/ConfigureTrialUI.py` 실행. 원본 이미지 및 재현 스크립트는 프로젝트 안에 보존한다.
- 검증: `Tools/Validation/RunRewardLootPresentation.py`에 실패 화면 720p/1080p/와이드 및 일반 중단 사례를 추가했다.

## 검증

- UE 5.8 Development 빌드, PNG 가져오기·텍스처 크기 검사 통과. 기록: `Saved/TrialUI`.
- PG 자동 테스트 45개 통과(그중 기존 경고를 포함한 성공 7개), 실패 0개. 기록: `Saved/TrialUI/Automation/index.json`.
- 최초 렌더에서 중앙 정렬한 짧은 문구의 자동 줄바꿈이 통계 영역을 밀어내는 문제를 발견해 해당 문구는 한 줄로 유지하고 실패 패널 크기를 보정했다.
- 실패 3해상도·일반 중단·강화 선택·승리 2해상도·저장 재시도·선택 실패/재시도·빈 보상 총 10사례 통과: `Saved/QA/20261005T091714Z_4a1cec26_reward_loot/report.json`.
- 최종 줄바꿈 보정 후 실패 720p/1080p/2560×1080과 승리 720p의 4사례 재검증 통과: `Saved/QA/20261005T092411Z_9d93e17a_reward_loot/report.json`. 실제 캡처에서 문구·통계·버튼 잘림 없음 확인. 최종 720p 실패 캡처에는 동시 실행 환경의 엔진 비디오 메모리 초과 경고가 있었으므로 이 검증을 성능 수용으로 보지 않는다.
- 최종 Development 빌드 성공: `Saved/TrialUI/build-layout.log`. DebugGame 추가 빌드는 실행 중인 `UnrealEditor-Win64-DebugGame.exe`가 DLL을 점유해 `LNK1104`로 링크할 수 없어 중단했다(`Saved/TrialUI/build-debuggame.log`). 사용자 편집기는 종료하지 않았다.
- 화면·상호작용 검증은 격리 `UI_*` 프로필과 자동 입력으로 실행한다. 직접 키보드·마우스 조작 체감과 실행 중인 DebugGame 편집기에 대한 적용은 별도이며, 최신 소스로 해당 구성 빌드 후 편집기를 다시 실행해야 한다.
