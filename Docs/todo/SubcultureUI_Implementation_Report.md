# 달빛 성소 UI / UX 구현 보고서

## 범위

2026-10-05 중단된 `SubcultureUI_Design.md` 작업을 이어서 구현·검증한다. 기존 아트 8종과 PGUI 초안을 유지하며, 캐릭터 애니메이션·장비 부착 등 다른 진행 중 변경은 이 UI 작업의 범위에 포함하지 않는다.

- `I → 전투 준비`: 달빛 성소 배경, 잉크 네이비 패널, 라벤더 선택 표시와 민트 확인 동작. 배경은 화면 비율을 유지하며 채우고 모든 탭에 선택 표시를 제공한다.
- 장비: 장착 / 가방 / 비교, 고정 장착·버리기 버튼, 영역별 스크롤과 기존 저장 경로를 유지한다.
- 검술: 아이콘 목록 / 선택 검술 상세 / 장착 슬롯 및 강화의 세 영역. 저장 전 구성과 실제 장착을 구분하고 두 슬롯을 함께 저장한다. 다른 슬롯의 스킬을 배치하면 교환한다. 장착 구성으로 되돌리면 미적용 상태를 해제한다.
- 닫기: 변경 중에는 적용 후 닫기 / 계속 편집 / 변경 취소 후 닫기를 표시한다. 적용 실패는 창과 편집 내용을 유지하며 재시도할 수 있다. 스테이지 전환 등 시스템의 강제 창 정리 경로는 기존 컨트롤러 정책을 따른다.
- 캐릭터: 초상화 7종의 갤러리, 큰 미리보기, 사용 중 상태와 별도 확정 버튼. 현재 외형을 우선 표시하고, 선택 기록이 없거나 목록에서 제거되었으면 유효한 첫 캐릭터를 미리보기만 한다. 빈 목록은 안내한다. 저장에 실패하면 기존 캐릭터를 유지한다.
- HUD: 빌드 요약을 하단 왼쪽으로 이동, 빈 상태 문구 숨김, 스킬 카드 확대, 실제 단축키 유지, ‘시련 시작’ 행동 표시.

## 데이터와 유지보수

- 스타일: `Source/PGUI/Style`, Project Settings → Game → PG UI Style. `Config/DefaultGame.ini`의 `SanctuaryBackground`는 `/Game/DataCenter/UI/Moonlit/T_Sanctuary`를 가리킨다. 공통 스타일 변경은 보상·결과에도 영향을 준다. 정적 스타일 인스턴스의 색상 변경 확인은 새 게임 프로세스에서 한다.
- 아트 원본·프롬프트·가져오기: `Tools/Art/MoonlitUI`. 현재 초상화는 실제 선택용 메시의 정면·얼굴 캡처를 기준으로 다시 제작한 투명 배경 일러스트 7종이다. `ModelPortraits.json`에 모델 참조·캐릭터별 특징·생성 프롬프트·원본 경로를 보존한다. 상세 재개 기록은 아래를 따른다.
- 런타임 텍스처 8종: `/Game/DataCenter/UI/Moonlit`. 캐릭터 초상화는 `PGCharacterAppearance.Portrait`로 연결한다. `/Game/DataCenter`는 기존 AlwaysCook 대상이다.
- 기존 가져오기 결과: `Saved/MoonlitUI/Import_20261005T043130/result.json` PASS. 원본 에셋 백업과 7종 연결 목록: `Saved/MoonlitUI/20261005T043141`.
- PGUI는 저장·장착을 `PGProfileSubsystem`에 요청한다. 별도 저장 경로나 GAS 수치 권한을 추가하지 않는다.

## 검증

재현: UE 5.8의 번들 Python으로 `Tools/Validation/RunInventoryPresentation.py`를 실행한다. `--cases`로 일부 사례를 선택할 수 있다. 매 프로세스에서 새 `UI_*` 프로필을 사용하며 실제 플레이 저장은 건드리지 않는다. 화면은 오프스크린 SM6 렌더이며 입력 검사는 Slate 키 이벤트와 실제 UI 콜백에 기반한다.

초기 조사에서 기존 720p 캐릭터 캡처의 상세 영역이 비어 있었고 첫 검술 캡처에 텍스처 준비 문구가 남아 있었다. 기본 미리보기와 준비 완료 대기를 보완했다. 재검증 첫 실행의 캐릭터는 준비가 15초 제한을 넘겨 캡처하지 못했다(`Saved/QA/20261005T045536Z_7f6e483d_inventory`). 실패를 보존하고 실행기를 캡처 완료 기준 종료 및 90초 상한으로 수정했다.

- 재개 빌드: `Saved/MoonlitUI/ResumeBuild.log`, UE 5.8 Development Editor 성공.
- 후속 빌드: `Saved/MoonlitUI/FinalBuild.log`, Development Editor 성공. 스킬 이름의 중간점 분리, 미적용 변경 안내 캡처와 전투 중 변경 제한 검사를 포함한다.
- 최종 폴리싱 빌드: `Saved/MoonlitUI/PolishBuild.log`, Development Editor 성공. 720p의 ‘적용 후 닫기’ 버튼 줄바꿈을 제거했다. `Saved/QA/20261005T051320Z_d5732988_inventory/report.json`의 디자인 동작 및 720p 캡처 재검증 PASS다. 엔진의 기존 deprecated API·컴파일러 버전·순환 참조 경고가 남아 있다.
- 기존 자동 테스트: `Saved/QA/20261005T045606Z_8238c57b/Automation/index.json`, 45개 성공(일반 38, 경고 포함 7), 실패 0. 에셋 검사도 통과했다. 경고는 기존 테스트 월드의 시작 무기·GameplayCue 검색 경로·월드 종료 등에 관한 것이며 전체 결과는 `PASS_WITH_WARNINGS`다.
- 새 UI 동작 검사: 기본 미리보기, 미리보기 비저장, 캐릭터 저장 실패·재시도, 스킬 변경 비저장, 저장 실패 시 프로필·실제 슬롯 유지, 재시도 후 실제 슬롯 반영, 원래 구성 복원, 변경 취소 닫기, 적용 후 닫기 실패·재시도, 중복 배치 교환과 입력 복귀를 검사한다.

화면·동작 16개 사례가 `Saved/QA/20261005T050344Z_48fa4ab3_inventory/report.json`에서 모두 PASS다. 장비·검술·캐릭터 각각 1280×720 / 1920×1080 / 2560×1080, 가방 포화·빈 가방·저장 실패, 기존 입력 회귀·전투 모달 전환·새 디자인 동작, 1080p HUD를 포함한다. 720p HUD는 `Saved/QA/20261005T045536Z_7f6e483d_inventory`의 `hud720` 사례에서 별도로 PASS다. 준비 완료 후 실제 PNG의 크기를 검사하고 패널·고정 버튼·초상화·상태 문구를 시각 검수했다.

대표 캡처(프로젝트 루트 기준):

- 캐릭터: `Saved/QA/20261005T050344Z_48fa4ab3_inventory/character1080_PGInventory00044.png`
- 검술: `Saved/QA/20261005T050344Z_48fa4ab3_inventory/build1080_PGInventory00042.png`
- 장비: `Saved/QA/20261005T050344Z_48fa4ab3_inventory/open1080_PGInventory00037.png`
- 전투 중 변경 제한: `Saved/QA/20261005T050344Z_48fa4ab3_inventory/states_PGInventory00034.png`
- 저장 실패: `Saved/QA/20261005T050344Z_48fa4ab3_inventory/failure_PGInventory00046.png`
- HUD: `Saved/QA/20261005T050344Z_48fa4ab3_inventory/hud1080_PGInventory00047.png`
- 미적용 변경 안내(최종): `Saved/QA/20261005T051320Z_d5732988_inventory/design_PGInventory00048.png`

검증 실행기는 요청한 사례를 보고서에 남기고 전체가 종료되기 전에는 PASS로 기록하지 않도록 보완했다. 중단된 실행의 일부 성공을 전체 완료로 오인하지 않는다.

공통 스타일 영향 확인: `Saved/QA/20261005T051401Z_82d7266e_reward_loot/report.json`의 `reward720`, `result720` 모두 PASS. 실제 720p 캡처에서 보상 카드·설명·선택 버튼과 결과의 전리품·강화·새 도전 버튼 배치를 확인했다. 이번 후속 검사에서 보상 지급 로직이나 결과 창 코드는 변경하지 않았다.

## 2026-10-05 모델 기반 초상화 재개 및 표시 비율 수정

중단 상태를 다시 조사한 결과 PNG 교체와 7종 가져오기는 이미 완료되어 있었다. 기존 결과를 보존하고 원본·저장 에셋·실제 선택 화면을 대조했다.

- 모델 캡처 오류는 Unreal `Array`를 JSON에 직접 기록한 `TypeError`였다. 기존 수정본은 `extract_filenames()`를 Python `list`로 변환하고 보고서 저장의 `finally`에서 콜백 해제와 에디터 종료를 수행한다. `Saved/MoonlitUI/ModelReferences/capture.json`은 PASS이며 정면·얼굴 14장 모두 1536×1536이다. 재개 검사 `reference-validation.json`에서 원본 FBX 7개의 SHA-256도 기존 검사와 동일함을 확인했다.
- 7종 PNG는 1024×1536이며 생성 원본과 SHA-256이 일치한다. 투명 배경의 알파 최솟값은 0, 최댓값은 254다. 근거: `Saved/MoonlitUI/ModelReferences/png-validation.json`.
- 이전 가져오기: `Saved/MoonlitUI/Import_20261005T073414/result.json`, 에셋 백업과 연결 목록: `Saved/MoonlitUI/20261005T073440`. 최신 별도 UE 프로세스의 재로드 검사는 `Saved/MoonlitUI/Import_20261005T075956/result.json` PASS다. `ModelReferences/reload-validation.json`에 7종 `Portrait` 경로·가져오기 경로·원본 해시를 보존하며 UI 압축·알파 유지 설정도 검사한다.

화면 검수에서는 기존 PASS 캡처에서도 세로 초상화가 가로로 늘어나 있었다. 에디터의 비동기 텍스처 컴파일 중 `GetSizeX/Y()`가 임시 정사각형 크기를 반환하는데, `PGUIInventory::Art`가 이를 브러시에 한 번 저장하고 유지한 것이 원인이다. `GetImportedSize()`로 원본 종횡비를 사용하도록 변경했다. 이 값은 cooked 빌드에도 보존된다. 갤러리 카드는 원본 비율로 영역을 채우되 상단을 기준으로 잘라 얼굴·뿔·귀를 유지하고, 상세 미리보기는 전체 이미지를 비율대로 맞춘다. 성소 배경도 같은 원본 크기 처리를 따른다.

`PGInventoryProbe`의 격리 프로필 전용 검사에 갤러리 버튼별 미리보기와 이미지 배치 비율 검사를 추가했다. `RunInventoryPresentation.py --portrait-previews`로 대상을 지정한다. 캡처마다 카드 7장과 상세 1장, 캐릭터별 누락 여부, 선택한 상세 이미지, 원본 대비 브러시·배치 비율, 미리보기의 비저장을 확인한다. 실제 저장 프로필은 사용하지 않는다.

재현(UE 5.8 번들 Python):

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Art/MoonlitUI/RunMoonlitUI.py --validate-model-portraits
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunInventoryPresentation.py --cases character,character1080,characterwide,design,interaction --portrait-previews LianLian Honoka Hichi Siuha Lili Nenmir
```

- Development Editor 빌드: `Saved/MoonlitUI/PortraitResumeBuild.log` 성공. 기존 deprecated API·컴파일러 버전·순환 참조 경고는 남아 있다.
- 수정 후 1080p 및 8개 이미지의 비율 검사: `Saved/QA/20261005T080730Z_eb8b4f76_inventory/report.json` PASS. 대표 화면은 같은 폴더의 `character1080_PGInventory00056.png`다.
- 720p·와이드·나머지 6종 상세·저장 실패/재시도·입력 회귀: `Saved/QA/20261005T080839Z_463751e3_inventory/report.json`의 요청 10개 전부 PASS. 위 1080p와 합쳐 11개 사례를 완료했다. 7종 상세 화면, 720p·와이드 갤러리, 이미지 비율·투명 배경·이름·선택 표시·확정 버튼을 시각 검수했다. 최종 상세 화면은 같은 폴더의 `portrait_<이름>_PGInventory00061.png`부터 `00066.png`까지이며 Bokusei는 위 `00056.png`다.

모델 정면과 일러스트를 대조해 머리·눈·의상 색과 주요 장식의 대응을 확인했다. 일러스트화에 따른 선·문양·표정의 차이는 있으며 3D 렌더와 픽셀 단위로 일치하는 산출물은 아니다. 검증은 오프스크린 실제 게임 렌더와 Slate 이벤트 기반이며 물리 마우스/키보드의 직접 플레이 검수는 포함하지 않는다.

## 후속 폴리싱

물리 키보드·마우스의 체감 평가, 패드 전용 포커스 탐색, 초상화의 정식 캐릭터 설정 감수, 보이스·전환 연출, 패키지 GPU 비용 측정은 별도다. 이번 자동 검사와 정지 화면 검수를 직접 플레이·성능 인증으로 간주하지 않는다.
