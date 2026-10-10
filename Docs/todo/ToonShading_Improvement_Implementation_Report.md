# 툰 셰이딩 개선 — 진행 기록

## 목표 및 수용 기준

2026-10-09 사용자 요청: 웹 조사에 근거한 P0 → P1 → P2 전체 구현 및 검증.
기존 수정 사항과 전투·카메라 기능을 보존한다. 단계별 적용·재로드·SM6 렌더·런타임 회귀·패키지 검증을 구분하며, 생성 성공을 시각 품질 수용으로 간주하지 않는다.

| 단계 | 범위 | 상태 |
|---|---|---|
| P0 | 명시적 주광원, 확산/림/하이라이트/상태 발광 분리, 해상도·겹침 외곽선 | 구현·자동 검증 완료 |
| P1 | 단순 헤어 투사체, 독립 이방성 하이라이트, 공식 Substrate Toon BSDF 비교·채택 판단 | 구현·후보 비교 완료, 기존 SDF 채택 |
| P2 | 모델별 얼굴 SDF 확장, 거리별 표현 품질, 전투·두 카메라·패키지 성능 | 구현·자동 검증·성능 측정 완료, 에디터 60fps 수용 미달 |

## 작업 기준

- EngineAssociation 5.8. 설치 엔진에서 Experimental Substrate Toon BSDF 확인.
- Bokusei 게임 얼굴은 Unlit SDF, 월드 수광과 헤어 투사체는 비교 맵의 6/8단계에 한정된다.
- 기존 명암에 fwidth AA, 외곽선에 깊이 가림 검사 및 Before DOF 처리가 있다.
- 기존 월드 수광은 셀·림·반사·상태색을 포함한 Color를 BaseColor와 Emissive에 함께 분배한다.
- 외형 컴포넌트는 첫 번째 DirectionalLight를 선택하며 명시적인 주광원 식별이 없다.
- 작업 시작 시 카메라 관련 별도 Unreal 검증 프로세스 실행 중. 다른 작업의 프로세스를 종료하지 않는다.

## 검증 계획

고정 노출의 정면/측면/역광/그늘, 근접/쿼터뷰/3D 액션, Screen Percentage 50/67/100%, 캐릭터 겹침·환경 가림, 회전·공격·대시·VFX, 1/10/50 캐릭터와 실제 숲 전투, 저장 재로드 및 패키지 실행을 비교한다. GPU 평균과 프레임 p95/p99를 기록하고 측정 환경·실행 시간을 명시한다.

## 조사 근거

- [UE 5.8 출시 노트](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes): 실험 단계 Toon BSDF, Blendable GBuffer와 로컬 광원/Lumen 지원.
- [TSR](https://dev.epicgames.com/documentation/en-us/unreal-engine/temporal-super-resolution-in-unreal-engine): Before DOF는 내부 렌더 해상도로 실행된다.
- [VSM](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine): 큰 Source Angle 및 겹친 투사체의 품질/성능 제한.

전체 수용 완료 전에는 이 문서의 단계 상태를 완료로 변경하지 않는다.

아래의 구현·자동 검증 완료는 기록된 시험 범위의 완료를 뜻한다. 장시간 직접 플레이와 모든 조건의 60fps 품질 수용은 별개이며, P2의 에디터 프레임 예산 미달을 포함한 최종 판정은 마지막 측정 절을 따른다.

## 구현 및 중간 검증 (2026-10-09)

- `PGToonPresentationComponent`: 태그 `PGToonKeyLight` 우선, 밝기/경로 순 안정적 fallback. 소실 시에만 초당 1회 재탐색. 거리별 림·반사 감쇠와 원거리 머리 방향 15Hz 업데이트, 머리 본 부착 정적 그림자 프록시 추가.
- World Lit 재질은 ToonDiffuse만 BaseColor로 보내고 림·반사·상태 발광은 Emissive로 분리. Unlit 결과는 유지. 이방성 헤어 반사를 수광 모드와 독립적으로 제공.
- Before DOF 외곽선에 `View.ViewResolutionFraction`을 적용하여 출력 픽셀 기준 폭 유지. 동일 스텐실 캐릭터 사이 깊이 불연속의 가까운 쪽에만 내부 겹침선 추가.
- UE 5.8 `DeleteAllMaterialExpressions`의 live-array 삭제 후 잔여 노드를 확인. 공유 생성기는 복사한 목록을 개별 삭제하고 빈 그래프를 확인하도록 수정. 두 실패 실행은 트랜잭션 백업으로 복구.
- 최종 마스터 적용 및 별도 프로세스 재로드 PASS: `Saved/ToonImprovement/20261009T115847978573Z`. 인스턴스/메시 해시와 기존 기본값 보존 검사 포함.
- Development Editor 빌드 PASS: `Saved/ToonImprovement/quality-build2.log`. `PG.Rendering.Toon` 주광원 및 거리 품질 테스트 PASS: `Saved/ToonImprovement/20261009T120134014253Z`.
- 추가 얼굴 SDF 10종 베이크 PASS: Arin, Hichi, Honoka, Hwarin, LianLian, Lili, Nenmir, P09 Female/Male, Siuha. 머리 영역 추출, UV 충돌/유한값/방향 극단 검사. 후보 재질 생성까지 완료, 실제 렌더 수용 전에는 원본 인스턴스를 변경하지 않음.
- Bokusei 앞머리 형상에서 경량 프록시 224삼각형 생성. 아직 비교 후보이며 게임 외형에 연결하지 않음.

## 공식 Toon BSDF 비교

설치 엔진의 노드/ToonProfile API와 Substrate 활성화, Blendable GBuffer=0을 검증한 격리 후보를 생성했다. 프로젝트 렌더 설정은 변경하지 않았다. `Saved/ToonImprovement/20261009T114244786523Z`의 PIE/TSR 24장(근접·쿼터뷰, 0/60/180도, SP 50/100) 렌더 PASS. 기본 프로필에서 얼굴의 물리적 헤어 그림자와 반사 띠가 기존 SDF보다 강해 현재 전면 교체의 시각적 이점이 입증되지 않았다. 기본 후보 결과이며 공식 Toon 자체의 품질 한계로 일반화하지 않는다. 기존 SDF 경로를 채택하고 공식 후보는 별도 비교용으로 보존한다.

## 최종 에셋 적용 및 회귀

- 얼굴 10종의 기존 재질에 모델별 SDF 부모/텍스처/얼굴 파라미터를 적용했다. 다른 재질 값은 유지한다. P09 남녀 기본/Armor007은 공통 얼굴 재질을 사용하므로 함께 반영된다.
- 근접 시각 확인: 플레이어 8종은 `20261009T120422486543Z`의 해당 16장. P09는 `20261009T121317977539Z`의 실제 몬스터 클래스 4장. 앞선 플레이어 기반 P09 캡처는 외형 적용 거부를 감지하지 못한 잘못된 테스트였으므로 수용 근거에서 제외했다. 검사기에 반환값과 실제 메시 일치 조건을 추가했다.
- 경량 헤어/카드/없음 비교 18장: `20261009T120927474117Z`. 0/60/120도와 Source Angle 3/20도에서 확인. 얼굴을 가로지르는 카드의 가는 그림자가 줄어 경량 프록시와 3도를 개선 비교 맵에 채택했다.
- `/Game/Art/ToonTest/Improvement/Maps/L_PGToon_ImprovedComparison`: 기존 8단계 기준 맵에서 파생한 별도 맵. 6/8단계가 224삼각형 정적 프록시를 사용하며 기존 J 토글과 거리별 비활성화를 지원한다. 원본 기준 맵은 유지한다. 게임 Bokusei 얼굴은 기존 Unlit이므로 그림자 프록시를 불필요하게 생성하지 않는다.
- 최종 적용/별도 프로세스 재로드 PASS: `20261009T121626077535Z`. 최종 C++ 빌드: `final-build.log`.
- 개선 맵 60장 PASS: `20261009T122020415869Z`. SP 50/67/100, 근접/쿼터뷰, 정면/측광/역광 및 겹침 6장을 포함. 화면에서 해상도 변경에 따른 과도한 외곽선 팽창이나 가려진 뒤쪽 캐릭터의 선 투과는 관찰되지 않았다.
- PG 자동 테스트 45개 PASS: `20261009T122247429705Z/Automation`. 기존 도구의 `-Multiprocess`가 설정 저장 검사를 막은 첫 실패는 별도 UserDir 실행으로 해소했다. 테스트 코드는 변경하지 않았다.
- 플레이어 전체 및 P09 4종 런타임/리타게팅/장비 연결 PASS: `Saved/PlayableCharacters/Runs/20261009T122307_runtime`.
- 신규 Development 패키지 빌드/쿠킹/스테이징 PASS: `Package_20261009T121853Z` (389초). 숲·RogueArena·개선 비교 맵, SM5/SM6 셰이더 포함. 실행/성능 검증은 아래에 별도 기록한다.

위 `2026...` 실행 디렉터리는 별도 경로를 명시하지 않으면 `Saved/ToonImprovement/` 아래에 있다.

## 중단 작업 재개 및 최종 검증 (2026-10-09)

중단 시 마지막 실행은 `FinalChecks_20261009T123552Z`였다. 패키지 장면, 헤어/거리 토글, 이동 전투는 통과했고 다수 캐릭터 측정의 `no_post` 준비 단계에서 실패했다. 실패 기록은 그대로 보존한다.

- `ProbeToonPerformance.py`에서 렌더 상태 변경 후 첫 틱이 준비 시간보다 늦게 도착하면 본 움직임의 두 번째 표본을 얻기도 전에 실패하는 경로를 수정했다. 서로 다른 틱의 애니메이션 시계·손 본 움직임을 확인한 뒤에만 CSV 측정을 시작한다. 추가 대기는 30초로 제한하고 전체 실행 제한도 유지한다. 정지 애니메이션을 통과시키거나 실패 판정을 삭제하지 않았다.
- `python -m unittest discover -s Tools/Validation -p TestToon*.py`: 기존 13개 검사 PASS. 불완전 CSV, 동시 실행 오염, 정지 포즈, 반복 편차, 프레임 예산 집계를 포함한다.
- Git 제외 규칙에서 신규 SDF/헤어/공식 Toon 후보 생성기와 적용·검증 도구를 명시적으로 추적 가능하게 했다. 생성된 대용량 지오메트리·베이크·실행 로그는 기존 정책대로 로컬에 둔다.
- `FaceDistanceField.py`의 빈/가득 찬/비대칭/직사각형 필드를 직접 거리 계산과 비교하는 수치 검사 PASS. 이 머신의 기본 Python과 UE 번들 Python에는 NumPy가 없어 `C:/Program Files/Blender Foundation/Blender 5.2/5.2/python/bin/python.exe`로 실행했다. 베이크에는 NumPy/Pillow가 있는 환경이 필요하다.
- `Saved/ToonTest/Performance/20261009T130230894217Z`: 수정 후 12개 단계의 애니메이션·CSV 완료 검증은 PASS. 다만 측정 중 다른 작업의 `UnrealEditor-Cmd` PID 13724/39072가 관측되어 종합 성능 판정은 FAIL이다. 해당 수치를 최종 성능이나 기능별 절감 근거로 사용하지 않는다. 다른 작업 프로세스는 종료하지 않았다.
- `Saved/ToonTest/Performance/20261009T131214087289Z`: 동시 실행 없이 12단계와 CSV 저장을 완료했으나 에디터 종료 코드가 `0xC0000005`여서 FAIL을 유지했다. 검사기를 PIE 종료 요청 → 게임 월드 소멸 확인 → 3초 후 에디터 종료 순서로 보완하고 틱 콜백 재진입을 차단했다. 최종 재측정 전까지 이 실행도 수용하지 않는다.
- `20261009T123926961165Z`: 헤어 끔/켬, 가림막 끔/켬, 원거리 프록시 그림자 비활성화 5조건 PASS. 이는 Pawn 메서드와 런타임 상태 검증이며 사람의 키보드 조작 검수로 간주하지 않는다. 재개 시 헤어·겹침·공식 Toon 후보와 패키지 두 카메라의 PNG도 다시 열어 확인했다.
- `Saved/QA/20261009T124039Z_75bf36_mobile_combat`: Bokusei/Hwarin/Hichi 이동 공격·대시 렌더 회귀 PASS.

### 패키지 전투 성능

최종 실행 패키지는 `Package_20261009T123338Z/Package`이며 `Scene_20261009T123552Z/report.json`과 카메라별 원본 CSV가 근거다. Windows 11, i5-12400F, RAM 64GB, RTX 3060 Ti, 드라이버 581.29, 1920×1080, 각 카메라 준비 후 60초를 측정했다. 실제 숲 맵·AI·GAS를 사용하되 입력은 스크립트이며 플레이어 체력을 복원한다.

| 카메라 | 표본 프레임 | GPU 평균 | 프레임 평균 | 프레임 p95 | 프레임 p99 | 16.67ms 초과 |
|---|---:|---:|---:|---:|---:|---:|
| 쿼터뷰 | 6,919 | 7.916ms | 8.672ms | 9.698ms | 11.113ms | 17회 (0.246%) |
| 3D 액션 | 6,829 | 8.114ms | 8.786ms | 9.627ms | 10.546ms | 11회 (0.161%) |

두 카메라에서 프레임 p95/p99는 60fps 예산 안이다. 최대 프레임은 각각 71.995/147.719ms이므로 일시적인 끊김이 전혀 없다는 결론은 내리지 않는다. 이 결과는 해당 머신의 60초 스크립트 전투 측정이며 무보조 직접 플레이, 장시간 안정성, 모든 하드웨어의 성능 보증과 구분한다.

### 재현 및 조절

- 개선 비교 맵 실행: `Tools/PlayToonImprovedComparison.ps1`. 6/8단계 경량 헤어 그림자, J 헤어 토글, H 가림막, 방향키 조명, F 근접/Tab 쿼터뷰/0 전체 보기를 사용한다.
- 데이터 원본: `Tools/Art/ToonTest/improvement.json`. 게임 얼굴 10종과 별도 개선 맵, 프록시, Source Angle, 공식 Toon 채택 판단을 기록한다.
- 머티리얼 업그레이드: `python Tools/Validation/RunToonImprovement.py --step apply`. 백업·보존 검사·새 프로세스 재로드를 수행한다. 얼굴/개선 맵 적용은 `--final-assets`를 추가한다. 해당 적용은 먼저 생성된 얼굴/헤어 후보를 필요로 한다.
- 생성 순서: `InspectToonImprovement.py`(에디터) → `BakeCharacterFaces.py`(외부 Python, NumPy/Pillow) → `ConfigureCharacterFaces.py`(에디터). 헤어는 `BuildHairShadowProxy.py`(백그라운드 Blender, bmesh/NumPy) → `ConfigureHairShadowProxy.py`(에디터)다. 각 생성기가 남기는 JSON/에셋을 후속 단계가 검사한다.
- 렌더: `python Tools/Validation/RunToonImprovement.py --step render --improved`; 거리/토글 검사는 `--controls` 추가. 공식 후보는 `--step native-render`로 격리 Substrate 설정에서 비교한다. 기본 게임 설정은 전환하지 않는다.
- 자동 검사: `python Tools/Validation/RunToonImprovement.py --step automation --all-tests`.
- 패키지 생성: `python Tools/Validation/RunToonPackage.py`. 결과 `report.json`을 `RunToonFinalChecks.py --package-report <경로>`에 전달하면 패키지 전투·토글·이동 전투·1/10/50캐릭터를 순차 검사한다.
- 주광원: 명시적 `KeyLight` 참조 또는 레벨 방향광의 `PGToonKeyLight` 태그를 사용한다. 태그 없는 기존 맵은 보이는 방향광 중 가장 밝은 광원, 동률이면 경로순으로 선택한다. 선택된 광원이 소실된 경우에만 재탐색한다.
- 거리 품질: `PGToonPresentationComponent`의 `DetailDistance=600`, `SimpleDistance=1600`, `FarDetailWeight=0.35`; 원거리 머리 방향 15Hz. 헤어 프록시는 `HairShadowDistance=700`과 ±50cm 히스테리시스를 사용한다. SDF/실루엣은 유지하고 림·반사 디테일만 감쇠한다.

게임의 Unlit 얼굴은 의도적으로 기존 SDF 표현을 유지한다. 물리적 헤어 그림자와 월드 수광은 개선 비교 맵의 6/8단계에서 채택했으며 모든 게임 얼굴에 월드 그림자를 강제 적용한 것은 아니다. 공식 Toon BSDF는 기본 후보의 얼굴 그림자·반사띠가 현재 목표에 더 적합하다는 근거가 없어 전면 채택하지 않았다. 프로필 추가 튜닝 뒤 재평가할 수 있다.

### 1/10/50캐릭터 최종 성능 측정

`Saved/ToonTest/Performance/20261009T131754133532Z/report.json`: **CHARACTERIZED**, 오류 0, 종료 코드 0. 모든 12단계의 본 움직임과 최종 CSV 저장, PIE 정리·정상 종료를 확인했다. 측정 구간의 Unreal/게임/셰이더 컴파일 동시 실행은 없었다. 이 실행으로 앞선 준비 판정 및 종료 실패의 재검증을 마쳤다. 앞선 실패 디렉터리와 `FinalChecks_20261009T123552Z/report.json`의 FAIL은 이력으로 유지한다.

조건은 위와 동일한 머신, 1920×1080, TSR, Screen Percentage 100%, VSync/프레임 제한 해제, VSM/그림자 품질 3, Inori 후보 메시 강제 LOD1이다. 각 단계 준비 8초·측정 12초이며 50캐릭터 전체 표현을 기능별 비교 사이에 5회 반복했다. 실제 AI 전투가 없는 에디터 PIE 애니메이션 장면이다.

| 캐릭터 수 / 반복 | 표본 프레임 | GPU 평균 | 프레임 평균 | 프레임 p95 | 프레임 p99 |
|---|---:|---:|---:|---:|---:|
| 1 | 1,238 | 9.044ms | 9.692ms | 17.684ms | 38.729ms |
| 10 | 1,188 | 9.463ms | 10.104ms | 20.118ms | 49.924ms |
| 50 A | 967 | 11.434ms | 12.410ms | 30.736ms | 77.920ms |
| 50 B | 992 | 11.397ms | 12.094ms | 28.769ms | 87.556ms |
| 50 C | 984 | 11.428ms | 12.197ms | 25.262ms | 72.291ms |
| 50 D | 957 | 11.431ms | 12.536ms | 29.654ms | 76.478ms |
| 50 E | 995 | 11.412ms | 12.069ms | 27.502ms | 72.383ms |

50캐릭터 GPU 반복 최대/최소 비율은 1.00317(약 0.32% 편차), 1캐릭터 복귀는 1.01131이다. 양쪽 전체 표현 반복으로 보정한 GPU 절감은 후처리 외곽선 제거 0.144ms, 후처리+캐릭터 CustomDepth 제거 0.462ms, 반투명 오버레이 제거 0.028ms, 캐릭터 그림자 제거 0.831ms다. 기능을 제거한 진단치이며 생산 품질 프리셋으로 채택하지 않는다. 효과를 단순 합산하지 않으며 0.028ms처럼 반복 변동폭에 가까운 차이는 실질적 개선으로 단정하지 않는다.

**최종 판정:** P0/P1/P2의 구현, 후보 비교·채택 판단, 데이터 적용·재로드, 자동 전투·카메라·패키지 및 성능 측정·문서화 범위를 완료했다. 패키지 숲 전투 두 카메라는 기록된 60초 구간에서 p95/p99 60fps 예산을 충족한다. 반면 다수 캐릭터 에디터 장면은 평균 GPU 시간만으로 성공 판정할 수 없으며 프레임 p95/p99의 60fps 수용은 미달이다. 후속 성능 폴리싱은 CPU trace로 프레임 지연 원인을 분리하고 동일 장면의 패키지 측정으로 검증해야 한다. CSV의 RenderThreadTime이 거의 0이라 해당 카운터로 CPU 원인을 확정하지 않았다. 직접 조작감·장시간 안정성·다른 하드웨어 검수까지 완료했다고 주장하지 않는다.
