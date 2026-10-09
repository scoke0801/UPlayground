# 숲속 폐허 전투장 구현 기록

## 포석 반복감 수정 — 2026-10-09

촘촘히 겹친 포석이 비늘처럼 보인다는 피드백에 따라 바닥 포석을 230개에서 12개로 줄였다. 중앙은 흙·이끼 지면을 드러내고 접근로와 가장자리 일부에만 포석 흔적을 남겼다. 지면 노멀 혼합 강도도 0.48에서 0.24로 낮췄다. 배치 난수 소비 순서를 유지해 다른 숲·폐허 배치는 재배열하지 않았다. 현재 총 인스턴스는 2,338개다.

같은 플레이 카메라의 실제 렌더와 저장 재로드 검사가 통과했다. 이 장식 수정에서는 전투 시련 검사를 반복 실행하지 않았다. 최신 미리보기는 아래 이미지 링크를 따르며, 이후 절의 2,556개 배치와 전투 검증은 수정 전 이력이다.

- 생성: `Saved/ForestRuins/Runs/20261009T055806147224Z/build.log`
- 미술 렌더: `Saved/ForestRuins/Preview/20261009T060949856884Z/`
- 재로드: `Saved/ForestRuins/Runs/20261009T061355515078Z/validate.log`

## 고디테일 환경 교체 — 2026-10-09

현재 `/Game/Maps/L_PG_ForestRuins`는 아래 초기 LowPoly 배치를 교체한 **이끼 낀 석조 성소** 버전이다. 실행은 그대로 `Tools/PlayForestRuins.ps1`을 사용한다.

보유 Environment 폴더의 7개 팩을 다시 조사하고 `PolyartStudio/DreamscapeEastLands`의 개별 모델 28종과 텍스처 75개를 가져왔다. Unity 씬·프리팹 배치는 사용하지 않았다. `DetailedSource/Normalized`는 개별 FBX에서 LOD0만 추출한 복사본이며 원본 파일의 SHA-256을 보존한다.

| 리소스 | 이전 삼각형 수 | 새 LOD0 삼각형 수 |
|---|---:|---:|
| 아치 | 4,184 | 21,366 |
| 큰 암석 | 122 | 2,468 |
| 포석 | 40 | 4,592–4,798 |
| 나무 | 4,263 | 활엽수 3,705–5,903 / 굽은 침엽수 10,739 |

모든 모델의 폴리곤 수가 일률적으로 증가한 것은 아니다. 나무껍질·잎·석재의 전용 텍스처와 노멀·ORM, 잎의 알파 마스크, 석재의 이끼·레이어 마스크를 연결한 점이 외관 변화의 주요 부분이다. 텍스처의 실행 최대 크기는 2,048로 제한했다. 원본 Unity 셰이더의 완전한 이식은 아니며 Unreal용 재질로 재구성했다.

### 미술 변경

- 큰 석조 아치와 수호상을 시각 중심으로 배치하고, 외곽 석벽·암석·관목·나무로 깊이를 구성했다.
- 중앙의 반복 사각 타일을 불규칙한 회색 포석으로 바꾸고, 흙·낙엽·이끼를 월드 좌표로 혼합했다. 포석은 전투 지면 위 0.3–3.3cm의 장식이다.
- 중앙의 산개한 큰 돌을 정리하고, 네 잔해 주변에 낮은 자갈·관목·풀을 모았다. 전투 중앙은 큰 식생으로 가리지 않는다.
- 주광원·푸른 환경광·낮춘 노란 등불·얕은 안개를 조정했다. 하늘과 넓은 배경 지형, 추가 숲으로 전체 카메라의 지면 끝을 가렸다.
- 최종 2,556 인스턴스를 25 HISM 그룹에 저장한다. 나무 164개에는 외곽 생성 160개와 근경 4개가 포함된다. 저장 Actor는 22개이며 충돌 Actor 6개와 전투 영역은 유지한다.

![현재 성소 근접 렌더](../../Tools/Art/ForestRuins/Preview_Sanctuary.png)

![현재 전체 배치](../../Tools/Art/ForestRuins/Preview_Overview.png)

![현재 기본 플레이 시점](../../Tools/Art/ForestRuins/Preview_Gameplay.png)

### 생성·조정

`layout.json`은 시드·시작 위치·충돌 영역·나무 수·외곽 반경 배수·식생 시도 수·조명을 소유한다. `DetailedForestLayout.py`는 포석 패턴·아치·수호상·식생 군집·지면 재질을, `ImportDetailedResources.py`는 재질 연결을 소유한다. 식생 시도 수는 중앙/동선 제외 조건을 적용하기 전 값이다.

```powershell
# 전체 파이프라인: 원본 준비 → LOD0 추출 → 임포트 → 맵 생성 → 검증
python Tools/Art/ForestRuins/RunForestRuins.py --step all

# 배치만 수정한 경우
python Tools/Art/ForestRuins/RunForestRuins.py --step build
python Tools/Art/ForestRuins/RunForestRuins.py --step validate

# 전투 시련을 돌리지 않는 빠른 미술 프리뷰
python Tools/Art/ForestRuins/RunForestRuins.py --step preview --art-only

# 실제 경로·경계·보스 포함 시련 루프 검사
python Tools/Art/ForestRuins/RunForestRuins.py --step preview
```

재질 수정은 `--step detailed_import`를 먼저 실행한다. **에셋을 다시 저장하는 임포트/생성은 해당 맵의 프리뷰 프로세스가 종료된 뒤 순서대로 실행**해야 한다. 새 맵을 저장하기 전 기존 맵은 `Saved/ForestRuins/Backups`에 보관한다. `detailed_sources.json`에는 원본·재질 GUID 해석·텍스처 해시·팩 조사 결과, `detailed_geometry.json`에는 LOD 제거 결과와 실제 삼각형 수를 기록했다.

### 검증과 한계

기본 시점·전체·성소 근접 렌더를 반복 검수했다. 초기 렌더에서 발견한 마스크 샘플러 불일치를 수정했고, 런너가 `Failed to compile Material`/기본 재질 대체를 실패로 판정하도록 보강했다. 미술 프리뷰는 별도 `art_preview.json`으로 기록하며 전투 검증 성공으로 계산하지 않는다. PIE 종료 후 월드 정리를 기다리도록 프리뷰 종료 순서도 보완했다.

최종 저장 재로드와 전체 프리뷰가 통과했다. 17개 완전 경로, 실제 적 스폰/추격, 네 방향 경계 이동 차단, 일반 15웨이브와 보스 1웨이브, 보상 7회 및 종료 순서를 확인했다. 로그 검사 결과는 오류 0이며 재질 컴파일 실패·기본 재질 대체도 없다. 보조 처치/체력을 사용하는 시련 검사이므로 수동 밸런스 검증은 아니다. 이번 변경은 환경 에셋·Python 생성 도구·문서이며 C++ 재빌드는 수행하지 않았다.

| 최종 근거 | 위치 |
|---|---|
| 재질 임포트 | `Saved/ForestRuins/Runs/20261009T040948553899Z/detailed_import.log` |
| 맵 생성 | `Saved/ForestRuins/Runs/20261009T042546209881Z/build.log` |
| 미술 프리뷰 / 정상 종료 | `Saved/ForestRuins/Runs/20261009T045421511883Z/preview.log` |
| 새 프로세스 저장 재로드 | `Saved/ForestRuins/Runs/20261009T050934305519Z/validate.log` |
| 최종 전투·재질·종료 검증 | `Saved/ForestRuins/Runs/20261009T050936544728Z/preview.log` |
| 최종 렌더 8장 / 경로·경계 결과 | `Saved/ForestRuins/Preview/20261009T050957064226Z/report.json` 및 같은 폴더 |
| 본문 미리보기 원본 | `Saved/ForestRuins/Preview/20261009T045453208755Z/`의 기본 시점·전체·성소 이미지 |

수동 난이도 검수·장시간 GPU 성능·패키징은 이번 미술 교체의 검증 범위 밖이다. 동적 식생 바람·환경 음향·입자 연출은 추가 구현하지 않았다.

## 초기 LowPoly 버전 기록

아래는 교체 전 버전의 구현·검증 이력이다. 현재 외관과 수량은 위 고디테일 교체 기록이 우선한다.

### 초기 결과와 실행

Unity 프로젝트의 `Assets/ExternalAssets/Environment/LowPolyFantasyArena2`에서 개별 FBX 40개와 텍스처 4개를 가져와 Unreal의 새 빈 레벨에 배치했다. Unity 씬·프리팹 배치와 조립된 `Arena_01.fbx`는 사용하지 않았다. 나무·암석·잔해·석재 타일을 이용한 독자 배치이며 연속 지면과 경계 충돌 메시도 새로 제작했다.

- 레벨: `/Game/Maps/L_PG_ForestRuins`
- 리소스: `/Game/Environment/ForestRuins/{Meshes,Textures,Materials,Foliage}`
- 게임 모드: 기존 `/Game/Blueprints/GameMode/GM_StageGameMode`
- 일반 시련 5개와 보스 시련 1개: 기존 `DT_StageData` 사용

프로젝트 루트의 PowerShell에서 실행한다.

```powershell
.\Tools\PlayForestRuins.ps1
```

에디터에서 수정하려면 `-Editor`를 붙이거나 콘텐츠 브라우저의 `Maps/L_PG_ForestRuins`를 열고 Play한다. 실제 실행은 기존 조작·성장·보상 UI와 일반 프로필을 사용한다. 테스트 보조 기능은 실행 스크립트에 넣지 않았다. 기본 시작 맵 `RogueArena`와 게임 설정은 유지했다.

## 전투 배치

약 47×39m의 전투장을 낮은 석재 경계로 둘렀다. 가운데 약 15m 직경은 큰 장애물 없이 비워 기존 스폰 반경 12m와 광역 공격·회피 공간을 확보했다. 중앙의 부서진 석재 마당과 십자 접근로, 흙과 외곽 잔디의 구분으로 이동 방향을 읽을 수 있게 했다.

네 군데 낮은 잔해가 작은 우회 공간을 만든다. 큰 암석·나무와 주요 아치는 경계 밖에 배치했다. 타일과 장식은 충돌을 끄고 지면·경계·잔해 4개만 충돌을 담당한다. 지면 충돌은 타일 사이에서도 끊기지 않으며, 경계 충돌은 보이는 낮은 석재와 이어진다. 전투 지형의 Camera 채널은 Ignore로 설정했다.

953개 장식은 Unreal Foliage의 24개 HISM 그룹으로 저장했다. 개별 장식 Actor 대신 인스턴스를 사용하고 거리 컬링을 적용한다. 잔디·타일 그림자를 끄고 장식의 내비게이션 장애물·물리 충돌을 없앴다. 레벨의 Actor는 13개다. 이는 배치 비용을 줄이는 구조이며 GPU 프레임 성능의 측정 결과를 의미하지 않는다.

쿼터뷰에서 플레이어·공격 예고·드랍을 읽기 쉽도록 수동 노출, 낮은 Bloom, Motion Blur 0을 적용했다. 원본 색상 아틀라스는 Unreal Default Lit 재질로 다시 연결했다. 숲 바닥은 원본 Grass/Mud 텍스처와 월드 좌표 마스크를 사용한다.

## 재생성과 수정

`Tools/Art/ForestRuins/layout.json`에서 시드·시작 위치·중앙 마당·잔해 위치/크기/회전·식생 수·조명을 수정한다. 배치 자체는 `BuildForestRuins.py`, 지면/경계 형상은 `BuildForestGround.py`가 소유한다. 전투장 크기를 바꾸면 두 도구의 지면 충돌·외곽 장식과 내비게이션 범위도 함께 조절해야 한다.

```powershell
# 배치 수정 후 새 맵 생성 및 저장 재로드 검사
python Tools/Art/ForestRuins/RunForestRuins.py --step build
python Tools/Art/ForestRuins/RunForestRuins.py --step validate

# 실제 렌더·경로·경계 이동·6개 시련 흐름 검사
python Tools/Art/ForestRuins/RunForestRuins.py --step preview

# 원본 임포트부터 지면 제작·맵 생성·검증까지
python Tools/Art/ForestRuins/RunForestRuins.py --step all
```

`all`은 설치된 Blender와 프로젝트의 EngineAssociation에 맞는 Unreal 에디터를 사용한다. 먼저 `UPlaygroundEditor Win64 Development`를 빌드해야 한다. `sources.json`은 원본 경로·프로젝트 내 복사본·SHA-256을 기록한다. 원본 Unity 파일은 수정하지 않는다.

`build`는 이 생성 맵을 다시 만들므로 에디터에서 직접 고친 배치를 덮어쓴다. 이전 맵 파일은 `Saved/ForestRuins/Backups/<UTC>/`에 복사한다. 직접 수정한 버전을 유지하려면 별도 이름으로 Save As하거나 변경을 생성 도구에 반영한다. 단계별 실행 로그·명령은 `Saved/ForestRuins/Runs/<UTC>/`, 최신 요약은 `import.json`, `build.json`, `validate.json`, `preview.json`에 남는다.

에디터 전용 `PGEditorProbeTools`에 빈 월드의 내비게이션 데이터 생성과 PIE의 완전 경로 조회를 추가했다. 생성 레벨은 NavMeshBoundsVolume 및 Dynamic Recast를 저장하고 기존 스테이지 시작 시의 경로 갱신을 사용한다. Python에서 NavigationSystem의 CDO를 호출해 발생하던 ensure는 네이티브 경로 조회로 피하고, 런너가 전체 로그의 Error·ensure·크래시와 웨이브/보상 완료 순서를 검사한다. 런타임 전투·AI·GAS 코드는 변경하지 않았다.

## 검증 범위

- Development 에디터 빌드 성공.
- 새 프로세스 저장 재로드: 13 Actor, 24 HISM 그룹, 953 인스턴스, 충돌 Actor 6개와 모든 리소스 참조 유지.
- 원본 FBX·텍스처와 기존 `RogueArena`, `DefaultEngine.ini`, `DT_StageData`, `DA_PGProgression`의 SHA-256 유지.
- 새 지형의 전투장 영역 정점 525개가 모두 Z=0임을 Blender 저장 파일에서 확인해 모서리의 시각 지면과 평평한 충돌면을 맞췄다. 근거: `Saved/ForestRuins/ground_geometry_check.log`.
- 별도 `-game` 실행: 동적 Recast 90개 타일, 플레이어 지면 투영, 실제 적 5마리의 완전 추격 경로 확인. 근거: `Saved/ForestRuins/NavNative/engine.log`.
- 렌더 PIE: 전투장 17곳의 완전 경로, 실제 적 스폰·이동, CharacterMovement 입력으로 네 방향 경계 차단과 지면 유지 검사.
- 보조 시련 검사: 기존 `PGCombatCycleSmoke`로 일반 시련 15웨이브와 보스 1웨이브, 보상 선택 7회 및 종료를 검사한다. 높은 체력과 보조 처치를 사용하므로 수동 난이도 플레이테스트와 구분한다.

렌더 검증은 별도 테스트 프로필과 실행 디렉터리를 사용하며 일반 진행 데이터에 테스트 결과를 저장하지 않는다.

## 완료 기록 — 2026-10-09

최종 지형 재생성·맵 저장·재로드·렌더 검증이 모두 통과했다. 최종 렌더 로그에는 Error·ensure·크래시가 없고, `PGCombatCycle COMPLETE rewards=7`과 16개 웨이브의 정확한 순서를 확인했다. 플레이어 시작·17개 경로·네 방향 경계 검사도 통과했다.

| 근거 | 위치 |
|---|---|
| 최종 맵 생성 로그 | `Saved/ForestRuins/Runs/20261009T015833118510Z/build.log` |
| 새 프로세스 재로드 로그 | `Saved/ForestRuins/Runs/20261009T015937874862Z/validate.log` |
| 최종 렌더·완주 로그 | `Saved/ForestRuins/Runs/20261009T020010338007Z/preview.log` |
| 경로·경계 좌표·렌더 6장 | `Saved/ForestRuins/Preview/20261009T020026933690Z/report.json` 및 같은 폴더 |
| 요약 미리보기 | `Tools/Art/ForestRuins/Preview_Overview.png`, `Preview_Gameplay.png` |

![초기 숲속 폐허 전체 배치](../../Tools/Art/ForestRuins/Before_Overview.png)

![초기 기본 쿼터뷰 플레이 시점](../../Tools/Art/ForestRuins/Before_Gameplay.png)

## 추가 폴리싱 범위

현재 결과는 기존 전투 루프를 플레이할 수 있는 첫 환경 맵이다. 손으로 플레이하는 난이도·장애물 활용도 검수, 대규모 적/이펙트/드랍 상황의 GPU 성능 측정, 패키징 실행, 환경 음향·폐허 디테일·색상 균형의 추가 폴리싱은 별도다. 렌더/보조 완주 통과를 상용 출시 품질 판정으로 해석하지 않는다.
