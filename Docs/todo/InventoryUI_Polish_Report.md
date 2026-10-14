# 인벤토리·능력치 내부 UI 개선

## 2026-10-12 ImageGen·타이포그래피 정리

제공된 목업 설명을 참고하되 기존 월드 위 좌우 패널 구조와 실제 게임 데이터를 유지했다. 공개 Artifact URL은 이 환경에서 열리지 않아 원본 목업 자체의 렌더 검수는 하지 않았다.

- 아트: 내장 ImageGen으로 차콜·상아·황동·가넷 프레임을 생성했다. 외곽 색 번짐이 있는 투명 초안을 버리고 불투명 최종본을 선택했다. 이전 프레임은 보존하고 `/Game/UI/Inventory/T_PGForgePanel`을 인벤토리와 능력치에 연결했다. 원본·최종본·전체 프롬프트·256px 변환·Unreal 임포터는 `Tools/Art/InventoryRefinement`에 있다.
- 글꼴: IBM Plex Sans KR Regular/SemiBold를 공통 Slate CompositeFont로 사용한다. 14 미만을 없애고 14/16/20/24/28 단계로 모았다(28 초과의 기존 대형 표시는 유지). 파일과 OFL 1.1 원문·다운로드 출처는 `Content/UI/Fonts`; UFS 스테이징을 설정했다. 타이포그래피 변경은 공통 스타일을 사용하는 HUD에도 적용된다.
- 색: 인벤토리의 선택색·본문·보조 글자·희귀 색을 UI 설정에 노출했다. 희귀는 청색, 고유는 청록이며 슬롯에 등급 표식을 함께 표시한다. 금색 채움은 장착·각인 확정·강화 확정에 사용한다. 증가/감소는 기존 녹색/산호색 의미를 유지한다.
- 탐색: 주 탭을 키우고 능력치 진입 버튼을 제목 옆으로 옮겼다. 비교 화면의 반복 제목을 제거하고 제작·분해 진입을 접는 영역에 모아 수치 비교 공간을 확보했다. 기존 저장·미리보기·두 단계 버리기 확인 경로는 유지한다.
- 가방: 5열, 실제 그리드 폭으로 높이를 계산하는 정사각 칸. 빈 칸의 테두리를 없애고 우하단 위력·좌하단 즐겨찾기/잡동사니·우상단 등급 표식을 배치했다. 아이콘 없는 아이템은 이름을 표시한다. 아이템 툴팁과 스크롤바도 테마 스타일을 사용한다.
- 능력치: 기존 StatsEmblem을 레벨/경험치 헤더에 연결하고 경험치 막대를 추가했다. 값이 0인 항목을 흐리게 표시한다.
- 모션: 패널 열기 시 18 Slate 단위 이동과 ease-out 페이드를 적용했다. `InventoryRevealSeconds` 기본값은 0.18초이며 0으로 끌 수 있다. 기존 틱을 사용한다.

### 검증 기록

- 에디터 빌드 및 새 텍스처 임포트 성공. 폰트의 폐기 예정 파일명 생성자를 CompositeFont로 교체했다.
- 첫 렌더 9사례 PASS: full, open1080, stats720, stats1080, growth720, interaction, empty, hud720, build. 근거: `Saved/QA/20261012T134626Z_620dfaad_inventory/report.json`.
- 720p 능력치·1080p 비교 화면을 직접 확인했다. 지원하지 않는 장식 기호가 발견되어 글리프 지원을 Windows API로 검사하고 지원되는 기호로 교체했다. 비교 본문이 너무 짧아 제작·분해 영역을 접도록 추가 보정했다.
- `RunInventoryPresentation.py`에 bag720/bag1080을 추가했다. 격리 프로필에 채운 가방을 표시하고, 그리드의 `ArrangeChildren` 결과로 전체 칸의 폭/높이 오차가 1 이하인지와 가방 용량 일치를 검사한다. 개별 자식의 페인트 캐시는 컬링 시 오래된 배치를 보존하므로 측정에 사용하지 않는다.
- 두 번째 렌더에서 bag1080·open1080·interaction·salvage720·imprint720·craft720·hud720 통과. bag720은 미배치 자식 9개를 오류로 세던 최초 검사 실패를 보존했다: `Saved/QA/20261012T135246Z_b527517f_inventory/report.json`. 검사 조건을 보정하고 비교 수치를 옵션 설명 앞으로 이동했다.
- 0 크기 제외와 캐시 좌표의 뷰포트 교차 검사도 오래된 자식 캐시를 판별하지 못했다. `Saved/QA/20261012T135649Z_208e26ce_inventory/report.json`과 `Saved/QA/20261012T140027Z_faac2677_inventory/report.json`에 실패를 보존했다. 실패한 9개 칸은 폭 159.62/높이 72의 초기 캐시였으며, 최종 검사는 부모 그리드의 현재 배치를 직접 사용한다. 이 실행들의 bag1080·open1080·options720·interaction은 PASS다.
- PGUI 소스의 `TEXT` 리터럴을 실제 폰트 문자 맵과 대조하여 누락 글리프가 없는 것을 확인했다. 신규 아트 원본/임포터는 루트 `.gitignore` 예외에 포함했다. `Content`는 별도 Git 저장소이므로 새 폰트·OFL·`.uasset`은 그 저장소의 신규 파일이며 루트 코드 변경과 함께 보관해야 한다.
- 최종 에디터 빌드 성공: `Saved/InventoryRefinementBuildFinal.log`. 최종 bag720/bag1080 모두 PASS이며 각 해상도에서 24칸 전부 측정, 오차 실패 0을 확인했다: `Saved/QA/20261012T140345Z_47ac977f_inventory/report.json`. 기존 엔진 API 폐기 예정 및 권장 컴파일러 버전 경고는 남는다.
- 최종 비교 화면: `Saved/QA/20261012T135649Z_208e26ce_inventory/open1080_PGInventory00235.png`. 720p 옵션 2개 비교도 실제 캡처로 확인했으며 후속 변경은 검사 태그·검사 로직 및 재생성 시 열기 모션 초기화다.

### 범위

중앙 캐릭터 미리보기·부위별 추천 알고리즘·신규 획득 상태 저장·옵션 4열/롤 막대·길게 눌러 버리기·닫기 애니메이션·장착 플래시/카운트업은 추가하지 않았다. 기존 캐릭터 탭과 월드 캐릭터를 사용한다. 물리 입력·연속 모션 및 새 패키지의 폰트 스테이징은 별도 수용 검증이 필요하다. 에디터 렌더 PASS를 배포 패키지 검증으로 간주하지 않는다.

## 2026-10-11 변경

기존 차콜·샴페인 골드 외곽 프레임을 유지하고 내부의 정보 위계와 탐색을 정리했다.

- 가방: 3열의 긴 이름 카드에서 6열 아이콘 슬롯으로 변경. 이중 테두리를 제거하고 등급·표식은 슬롯 하단, 이름은 툴팁과 비교 화면에 표시한다. 아이콘이 없으면 이름을 표시한다. 빈 가방과 필터 결과 없음의 안내를 구분한다.
- 탐색: 상단 탭의 기본 테두리를 제거하고 활성 항목을 골드로 표시한다. 가방·장착·비교·재료를 같은 탐색 줄에 배치한다. 필터는 가방에서만 표시한다.
- 능력치: 이름과 현재 수치를 좌우로 정렬하고 계산 출처는 펼침 영역으로 제공한다. 7개 현재 수치를 빠르게 읽을 수 있도록 제목과 수치 크기를 늘렸다.
- 분배: 공통 버튼 스타일과 38×38 클릭 영역을 사용한다. 포인트가 없으면 접어서 표시하고 포인트가 있는 상태에서 능력치를 열거나 미적용 변경의 닫기 확인이 발생하면 펼친다. 저장 실패·미확정 분배·GAS 적용 경로는 유지한다.
- 비교: 아이템 이름을 표식 버튼보다 먼저 표시하고 표식 버튼을 한 줄로 묶었다. 0단계 명품화 설명을 숨기고 교체 예상 수치를 한 행에 정렬한다.

## 검증

- 첫 에디터 빌드 성공. 첫 렌더 검사 7사례 PASS: empty, open720, open1080, stats720, stats1080, growth720, interaction.
- 최초 캡처 근거: `Saved/QA/20261011T120300Z_5558ba24_inventory/report.json`.
- 실제 캡처를 보고 행 테두리 제거와 글자 확대를 추가했다. 최종 에디터 빌드 성공. 기존 엔진 API 사용 중단 및 권장 컴파일러 버전 경고는 남는다.
- 10사례 PASS: empty, full, open720, open1080, stats720, stats1080, statsdetail720, growth720, interaction, states. 근거: `Saved/QA/20261011T120959Z_d1e19a41_inventory/report.json`.
- 마지막으로 선택 슬롯에만 강조 테두리를 복원하고 일반 능력치 토글에서도 포인트 분배를 펼치도록 보완했다. 재빌드 성공 후 interaction·growth720 재검사 PASS: `Saved/QA/20261011T121352Z_a67cb52b_inventory/report.json`.
- 720p 아이템 가방·기본 능력치·분배 및 1080p 비교 화면을 직접 이미지로 확인했다. 수치 7개는 분배를 접은 기본 화면에 표시되며, 긴 상세 내용은 스크롤한다.
- 성장 검증에 포인트 지급 후 분배 영역 자동 펼침 확인을 추가했다. 상세 캡처는 능력치 출처를 먼저 펼친 뒤 스크롤한다.

## 범위와 한계

게임 실행은 격리된 UI 시험 프로필을 사용한다. 자동 입력은 물리 마우스·키보드 수용 검증을 대신하지 않는다. 새 패키지 생성 및 기존 배포 실행 파일 갱신은 이번 검증에 포함하지 않는다. 신규 프레임·아이콘 아트, 모션·사운드 추가 없이 기존 Slate UI와 리소스를 사용한다.


## 2026-10-12 독립 능력치 키·고딕 인벤토리 개편

### 변경

- 인벤토리 제목의 능력치 버튼을 제거했다. 기본 I는 인벤토리, K는 능력치다. 기존 Config 키를 유지하고 제목에 실제 설정 키를 표시한다. 컨트롤러의 공통 생성 경로에 초기 패널을 전달해 K가 인벤토리를 먼저 열지 않는다. 기존 Esc·작은 화면의 반대 패널 접기·분배 초안 보호는 유지한다.
- 빈 가방의 셀은 생성되고 있었지만 비활성 버튼에 윤곽선이 없어 배경에 묻혔다. 빈 셀에도 불투명 바탕과 지속적인 금속 테두리를 적용했다. 선택·등급 테두리는 별도로 표시한다.
- 오른쪽 인벤토리에 캐릭터 미리보기, 양옆 장착 10부위, 하단 8열 가방을 함께 배치했다. 비교·제작·분해와 검술·전리품·캐릭터 선택은 기존 경로를 유지한다. 오른쪽 폭은 화면의 46%, 왼쪽 능력치는 큰 화면 32%/작은 화면 46%다.
- 캐릭터는 현재 Pawn과 부착 액터를 ShowOnly SceneCapture로 표시한다. 512×640 렌더 타깃을 장비 개요가 보일 때만 10Hz로 갱신하고 창 종료 때 캡처 컴포넌트를 정리한다. 등록 시 복원되는 렌더 플래그를 등록 후 설정해 하늘·구름·안개를 제외한다. 이름표는 이미지 아래에 별도 배치한다. 장비 데이터에 따른 새로운 방어구 메시 교체 기능을 추가한 것은 아니다.
- 능력치는 캐릭터 이름·레벨·경험치와 검은 두 열의 수치 표로 구성했다. 실제 GAS 7개 수치, 펼쳐보는 출처 및 포인트 초안/확정 기능은 유지한다. 붉은 활성 탭과 석재·금속 프레임을 적용했다.

### 아트·재현

- 내장 ImageGen 사용. [전체 프롬프트](../../Tools/Art/GothicInventory/Generation.json), [원본 PNG](../../Tools/Art/GothicInventory/T_PGGothicPanel.png), [임포터](../../Tools/Art/GothicInventory/ImportGothicInventory.py).
- 런타임: /Game/UI/Inventory/T_PGGothicPanel, /Game/UI/Inventory/T_PGGothicStats. 원본은 공통이며 Slate가 실제 텍스처 크기로 모서리를 계산하므로 각각 최대 리소스 크기 256/512를 적용한다. Config의 InventoryFrame/StatsFrame으로 연결한다. 두 uasset은 별도 Content 저장소의 신규 파일이다.
- 임포트: UnrealEditor-Cmd.exe UPlayground.uproject -run=pythonscript -script=Tools/Art/GothicInventory/ImportGothicInventory.py -unattended -nullrhi -DisablePlugins=RiderLink -ddc=InstalledNoZenLocalFallback

### 검증

- UE 5.8 UPlaygroundEditor Win64 Development 최종 빌드 성공: [빌드 로그](../../Saved/QA/GothicInventory/build-final.log). 프레임 2종 임포트 성공: [임포트 로그](../../Saved/QA/GothicInventory/import-final.log).
- [기능·화면 8사례](../../Saved/QA/20261012T143223Z_690efcb7_inventory/report.json) PASS: 빈/가득 찬 가방, 720p/1080p 능력치, 장착·입력, 분배, 제작. 격리 UI 시험 프로필을 사용했다.
- [최종 입력 검사 포함 5사례](../../Saved/QA/20261012T143650Z_4bf41896_inventory/report.json) PASS: 독립 능력치 생성, Slate K 닫기, I/Esc, 20회 열기·닫기 및 기존 이동 잠금 보존 포함.
- [최종 레이아웃 5사례](../../Saved/QA/20261012T144152Z_0cdcede4_inventory/report.json) PASS: empty, bag1080, stats720, stats1080, growth720. 24칸 개수·정사각 비율뿐 아니라 마지막 줄의 실제 스크롤 표시 영역 포함 여부를 검사한다.
- 최종 캡처: [720p 빈 가방](../../Saved/QA/20261012T144152Z_0cdcede4_inventory/empty_PGInventory00264.png), [1080p 가방](../../Saved/QA/20261012T144152Z_0cdcede4_inventory/bag1080_PGInventory00265.png), [720p 능력치](../../Saved/QA/20261012T144152Z_0cdcede4_inventory/stats720_PGInventory00266.png), [1080p 능력치](../../Saved/QA/20261012T144152Z_0cdcede4_inventory/stats1080_PGInventory00267.png).
- 최초 컴파일의 제목 괄호·멤버 이름 충돌·SharedRef 브러시 타입 오류를 수정했다. 초기 8사례의 수치 검사는 통과했으나 캡처에서 하단 잘림·큰 모서리를 발견해 실제 가시성 검사를 보강했다. 이후 캐릭터 캡처의 하늘 노출과 이름표 가림을 수정하고 재빌드·재검증했다.
- Python 구문 검사와 git diff --check 통과. 기존 엔진 폐기 예정 API/툴체인 경고는 이번 작업에서 변경하지 않았다.

### 남은 수용 범위

- 실제 키보드·마우스 조작, 패키지 빌드 및 장시간 GPU 비용 측정은 하지 않았다. 위 입력 결과는 Slate 합성 입력이며 화면은 독립 게임 렌더 캡처다.
