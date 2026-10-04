# Unity lilToon 캐릭터 Unreal 이전 테스트

## 9차 — Inori LOD 연속 화면 품질 (2026-10-03~04)

6모션의 720p/1080p LOD0·1·2 비교, LOD1 근접 및 자동 LOD 왕복 영상 6종을 만들었다. 유효 4,798프레임의 해상도·시간 간격·LOD·본 움직임과 인코딩 결과를 검사했다. 상세 영상 링크와 문제 재현 조건은 [LOD 화면 품질 보고서](ToonLODQuality_Validation_Report.md)를 따른다.

기존 바운드에서 Screen Size 0.5/0.25, 히스테리시스 0.02와 얼굴·헤어 축소율을 유지하는 것을 권장한다. 큰 LOD1 형상 팝은 검토 샘플에서 관찰하지 못했으나, 얇은 정수리 헤어 선과 자기 그림자 점무늬·VSM 잔상이 남았다. 짧은 달리기 실험에서 Always 무효화+Bounds Scale 1.5로 고정 잔상이 사라져 수정 후보로 기록했다. 바운드 변경 후 LOD 거리/비용은 다시 검사해야 하며, 공용 에셋에는 적용하지 않았다. 얼굴 SDF/헤어 방향 맵은 추가하지 않았다.

이번 기록은 화면 품질 검증이며 아래 8차 성능 수치를 갱신하지 않는다. 실제 전투 맵의 조명·피격/사망/장비 연결과 에디터 외·패키지 성능 검증은 남아 있다.

## 8차 — 원본 지연 경로·CSV 저장 보정·LOD1 재측정 (2026-10-03)

**최신 판정은 이 절을 우선한다.** 단독 원본 CPU 추적으로 GPU 가림 쿼리 결과 대기를 확인했고, 측정 종료 시 마지막 CSV가 잘리던 도구 오류를 수정했다. 수정 후 LOD1 50개·720p의 60초×3회 측정은 저장 무결성과 반복 안정성을 모두 통과했다. 런타임 C++·엔진 설정·메시·머티리얼은 변경하지 않았다.

### CSV 저장 오류와 이전 판정 정정

`CsvProfile STOP`은 비동기 저장을 끝낸다는 보장이 없다. 이전 `ProbeToonPerformance.py`는 마지막 구간 직후 PIE/에디터를 종료해 버퍼의 끝부분과 최종 메타데이터를 누락했다. 예를 들어 수정 전 LOD1 실행 `20261003T133941615048Z`의 마지막 구간은 로그에 5,244프레임이 기록됐지만, CSV에서는 유효 FrameTime 5,110개만 읽혔고 마지막 행도 잘려 있었다.

- 마지막 캡처의 파일 잠금 해제와 최종 메타데이터를 확인한 뒤 종료한다. 완료 대기 중에는 측정을 재개하지 않으며 30초 내 저장되지 않으면 실패한다.
- 실행 요약은 모든 CSV의 완료 메타데이터·캡처 시간·파일 크기 제한 상태를 검사한다. 샘플 수가 충분해도 마지막 저장이 끝나지 않은 파일은 `FAIL` 처리하고 절감량 계산을 차단한다.
- FrameTime에 60fps/30fps 예산 및 50/100ms 초과 횟수·비율을 추가했다. 임계값과 같은 프레임은 초과로 세지 않는다.
- `Saved/ToonTest/performance_csv_integrity_audit.json`은 이전 전체 실행의 완료 판정을 정정한다. 원시 CSV·로그·기존 report JSON은 보존했다. **6차 LOD 비교의 마지막 `full_e`, 7차 기능 제거 비교 두 실행의 `one_return`, 7차 LOD1 추적의 `full_c`는 불완전하다. 해당 실행 전체가 완전하게 끝났다는 기존 판정과 복귀 안정성 주장은 철회한다.** 완료된 개별 구간과 CPU 추적의 관찰 근거는 보존하지만 이전 전체 실행을 현재의 저장 무결성 통과로 취급하지 않는다.

### 원본의 큰 지연: GPU 가림 쿼리 결과 대기

`Saved/ToonTest/Performance/20261003T133433994200Z/`에서 원본 50개를 12초 준비·30초 측정으로 3회 CPU 추적했다. 다른 Unreal/game/compiler 프로세스는 측정 중 관찰되지 않았다. GPU 평균 최대/최소 비율 1.262로 불안정하며 마지막 CSV도 불완전해, 이 실행의 GPU 절감량이나 전체 CSV 분포를 성능 결론에 사용하지 않는다. 별도로 정상 종료된 `.utrace`의 측정 영역 내 타임라인을 지연 진단에 사용한다.

`full_b`의 trace 시각 146.784829–147.762420초에서 다음 대기를 확인했다. 이 프레임은 측정 구간 안에 완전히 포함되며 CSV 종료/에디터 종료 프레임이 아니다.

| 동일 프레임의 관찰 범위 | 시간 ms | 의미 |
|---|---:|---|
| GameThread `FEngineLoop::Tick` | 977.591 | 전체 프레임 |
| GameThread `Sync_RenderingThread` | 954.926 | 렌더 진행을 기다리는 구간 |
| RenderThread `VisibilityCommands` | 970.656 | 가시성 처리 완료 지연 |
| Background Worker `OcclusionCullPipe`의 `GPUBound_WaitingForGPUForOcclusionQueries_SeeGPUTrack` | 970.710 | GPU 가림 쿼리 결과 대기 |
| 같은 작업의 `RHIGetRenderQueryResult_GPU_Wait` | 970.709 | RHI 쿼리 결과 대기 |

위 범위는 중첩되거나 동시에 진행하므로 합산하지 않는다. 설치된 UE 5.8의 `SceneVisibility.cpp:3303`에서도 해당 추적 범위가 `bWait=true`로 `RHIGetRenderQueryResult`를 호출하는 지점임을 대조했다. 원본의 이 큰 피크는 에디터 파일 감시나 캐릭터 MID 갱신만으로 설명할 수 없고, 가림 쿼리 결과를 기다리는 렌더 경로까지 좁혀졌다. GPU/드라이버/리소스 상주 중 무엇이 결과 반환을 늦췄는지는 이번 CPU 추적만으로 확정하지 않는다. 다른 프레임의 모든 지연에도 같은 원인을 일반화하지 않는다.

근거는 `cpu.utrace`, `cpu_summary.json`, `cpu_worst_frame.csv`, `cpu_worst_frame_summary.json`이다. `ExportToonCPU.py`의 측정 영역 내 통계와 별도 `export_worst_frame.txt`의 전체 스레드 이벤트 내보내기를 사용했다. Insights 분석은 렌더 측정이 끝난 뒤 순차 실행했다.

### 저장 보정 후 LOD1 결과

최종 실행은 `Saved/ToonTest/Performance/20261003T134830750689Z/`다. RTX 3060 Ti / 581.29, UE 5.8 DX12 SM6, 1280×720 PIE, 애니메이션 캐릭터 50개, 강제 LOD1, 외곽선·투명·그림자 모두 켬, ScreenPercentage 100, VSync 0, t.MaxFPS 0, 기존 `mass.UseProcessingQueue=0` 조건이다. 각 회차 12초 준비 후 60초 측정했으며, 준비 구간을 사이에 둔 총 3분 샘플이지 연속 장시간 soak는 아니다.

| 구간 | GPU 평균 ms | 프레임 p95 ms | 프레임 p99 ms | 최대 ms | 프레임 수 |
|---|---:|---:|---:|---:|---:|
| full_a | 8.775 | 13.181 | 15.480 | 45.296 | 5,532 |
| full_b | 8.788 | 14.529 | 17.863 | 46.770 | 5,124 |
| full_c | 9.024 | 13.768 | 15.798 | 58.505 | 5,338 |

판정은 `CHARACTERIZED`, GPU 평균 최대/최소 비율 1.028이다. 모든 구간의 실제 애니메이션 시간·본 움직임, 1280×720 뷰포트, 다른 Unreal/game/compiler 프로세스 미관찰, 세 CSV의 최종 메타데이터, `runtime.csv_finalized=true`를 확인했다. 다른 데스크톱 앱 및 OS 스케줄링까지 제거한 환경은 아니다.

합계 15,994프레임의 p95는 13.892ms, p99는 16.398ms, 최댓값은 58.505ms다. 16.667ms 초과 139개(0.869%), 33.333ms 초과 11개(0.069%), 50ms 초과 2개, 100ms 초과 0개다. **LOD1의 반복 GPU 안정성은 확인했지만 안정적인 60fps 인증은 아니다.** 두 번째 회차 p99도 16.667ms를 초과한다. 최종 실행은 CPU 추적을 끈 상태이므로 여기서 발생한 58.505ms 지연을 원본의 가림 쿼리 대기와 같은 원인이라고 단정하지 않는다.

수정 전 재측정 `20261003T133941615048Z`는 마지막 CSV 누락으로 전체 성능 표에서 제외했다. 그 실행에 기록된 `CHARACTERIZED`는 저장 무결성 감사에 의해 정정된다. 수정 후와 수정 전의 수치 차이를 런타임 최적화 효과로 주장하지 않는다. 종합 결과는 `Saved/ToonTest/performance_hitch_followup.json`, 최신 실행 전체는 `performance_latest.json`이다.

### 재현·검증과 다음 단계

```powershell
$pythonExe = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
& $pythonExe Tools/Validation/RunToonPerformance.py --fixed-lod 1 --repeats-only --warmup 12 --sample 60
& $pythonExe Tools/Validation/RunToonPerformance.py --repeats-only --trace --warmup 12 --sample 30
& $pythonExe Tools/Validation/ExportToonCPU.py Saved/ToonTest/Performance/<추적실행ID>
& $pythonExe -m unittest discover -s Tools/Validation -p TestToonPerformance.py
```

순차 실행한다. Python 회귀 9개 PASS, 변경된 계측 스크립트 구문 검사 PASS, 수정 후 실제 세 구간과 최종 CSV 저장 확인 PASS다. C++·아트 에셋 변경이 없어 C++ 재빌드·에셋 재생성은 수행하지 않았다.

LOD1을 움직임 품질 검토 후보로 유지한다. 다음 품질 작업은 연속 영상의 헤어·그림자 깜빡임, 얇은 외곽선, 해상도·LOD 전환이다. 성능 인증에는 에디터 외 실행과 실제 전투·Development 패키지의 추가 계측이 필요하다. 가림 쿼리 대기가 그 환경에서도 재현되는지 먼저 확인한다. 입력부터 디스플레이까지의 지연은 이번 프레임 시간 검사 범위가 아니다. 얼굴/헤어 축소 비율·LOD 전환 거리 확정, 전투 표현 연결, 모든 광원/그림자의 일정한 셀 단계화와 의상 물리·스키닝은 기존 후속 범위로 유지한다.

## 범위

- 원본: `C:/UsingProject/UnityProject/UPlayground/Assets/ExternalAssets/Character/Inori`
- 결과: `/Game/Art/ToonTest`
- 엔진: Unreal Engine 5.8, DX12, SM6
- 목적: Unity lilToon 전용 캐릭터의 메시·텍스처를 재사용해 Unreal 카툰 렌더링 가능성을 검증한다.

lilToon 셰이더와 Unity `.mat` 파일은 Unreal에서 직접 사용할 수 없다. 이번 테스트는 Unity 머티리얼의 `_BaseMap` 연결을 추출하고 Unreal 전용 마스터 머티리얼과 인스턴스로 재구성했다.

## 생성 결과

- `SK_Inori_ToonTest`: 스켈레탈 메시와 전용 Skeleton
- `M_PGToonCharacter`: Unlit 기반 3단 셀 명암, 림라이트, `StateColor`, `StateGlow`, `DissolveAmount` 지원
- `M_PGToonOutline`: 뒤집힌 Hull 방식 외곽선 테스트 머티리얼
- 베이스 텍스처 9개, 머티리얼 인스턴스 11개
- 활성 메시 슬롯 9개 전부 자동 연결
- 최초 임포트 24개 에셋, 약 85.72MB (아래 추가 테스트 에셋 제외)

임포트 결과는 `Saved/ToonTest/import.json`, 렌더 결과는 `Saved/ToonTest/Preview/preview.json`에 기록된다. 최종 프리뷰는 `Saved/ToonTest/Preview/Inori_Toon.png`다.

## 실행 방법

에셋 재생성:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  "$PWD\UPlayground.uproject" -unattended -NullRHI -NoSplash `
  -ddc=InstalledNoZenLocalFallback -EnablePlugins=PythonScriptPlugin `
  -run=pythonscript "-script=$PWD\Tools\Validation\ConfigureToonCharacterTest.py"
```

렌더 프리뷰:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  "$PWD\UPlayground.uproject" -unattended -NoSplash -RenderOffscreen `
  -windowed -ResX=1280 -ResY=720 -ddc=InstalledNoZenLocalFallback `
  -EnablePlugins=PythonScriptPlugin `
  "-ExecutePythonScript=$PWD\Tools\Validation\PreviewToonCharacterTest.py" -log
```

Unreal 셰이더 컴파일러는 사용자 프로필의 `UnrealShaderWorkingDir`에 쓸 수 있어야 한다. 해당 경로가 제한되면 회색 기본 체크 재질이 렌더되고 ShaderCompilingThread가 실패한다.

## 검증 결과

- 에셋 임포트: PASS
- 머티리얼 슬롯 매핑: 9/9 PASS
- PCD3D_SM6 머티리얼 컴파일: PASS
- 1280×720 전면 3/4 렌더: PASS
- 원본 텍스처, 셀 명암, 림라이트, 외곽선 출력: 육안 확인 PASS

## 알려진 제한

- 원본 FBX의 Bind Pose가 유효하지 않아 Unreal이 타임 제로 포즈로 재바인딩한다.
- 일부 메시에는 스무딩 그룹 정보와 유효한 탄젠트가 부족하다.
- 첫 임포트에서 버텍스당 스킨 영향도 25개가 UE 한도 12개로 절삭되는 경고가 확인됐다.
- 최초 프리뷰는 T 포즈 정적 렌더였다. 아래 2차 테스트에서 기본 이동·전투 모션을 변환했지만, 치마·머리카락 보조 물리와 얼굴 모프는 아직 포함하지 않았다.
- 외곽선은 별도 중복 SkeletalMeshActor를 사용하는 검증용 Inverted Hull 방식이다. 다수 몬스터가 등장하는 본 게임에는 Custom Depth 기반 화면 공간 외곽선과 비용을 비교해야 한다.
- lilToon의 MatCap, 세부 Shadow Map, 얼굴 SDF, 다중 Emission 등은 이번 최소 검증 범위에 포함하지 않았다.

## 다음 단계

1. 원본을 Blender 또는 DCC에서 올바른 Bind Pose, 스무딩 그룹, 최대 8개 이하 스킨 영향도로 정리한다.
2. 아래 테스트 모션의 변형 문제를 정리한 뒤 발 고정 IK, 얼굴·보조 물리, 테스트 전용 플레이어 AnimBP를 연결한다.
3. 얼굴 SDF 및 머리카락 전용 명암을 추가해 정면 조명의 품질을 보강한다.
4. 쿼터뷰 실전 카메라에서 Inverted Hull과 Custom Depth 외곽선의 가독성·GPU 비용을 비교한다.

## 2차 테스트 — 애니메이션 리타기팅

`ConfigureToonRetargetTest.py`가 UE 5.8의 Pelvis Motion/FK Chains 연산으로 기존 ElfSelena 모션을 **Inori 전용 Skeleton으로** 변환한다. Inori 메시를 ElfSelena Skeleton에 강제로 재바인딩하지 않는다.

- 소스/타깃 IK Rig 2개, Retargeter 1개, 공통 체인 21개
- 대기, 걷기, 달리기, 공격 1, 피격, 사망: AnimSequence 6개
- 결과 폴더: `/Game/Art/ToonTest/Inori/Animation`
- 5개 시점의 주요 관절 위치가 유한값이고 실제 움직임이 존재하는지 검사
- 원본 메시·소스 애니메이션 SHA-256 불변 검사 PASS
- 재실행 전 기존 테스트 애니메이션 폴더를 `Saved/ToonTest/Retarget/<실행ID>/backup`에 백업
- 결과: `Saved/ToonTest/retarget.json` (이 검사는 최종 변형 품질을 보증하지 않음)

이 단계는 FK 기반의 오프라인 변환이다. 발 접지 IK, 루트 모션 게임플레이, 무기 소켓/공격 판정, 기존 플레이어 AnimBP·GAS 연결은 테스트 범위가 아니다. 원본 플레이어 Blueprint와 일반 게임 맵은 변경하지 않는다.

다시 생성하려면 위 commandlet 실행 예시에서 스크립트를 `ConfigureToonRetargetTest.py`로 변경한다. 먼저 `ConfigureToonCharacterTest.py`의 임포트 결과가 있어야 한다.

### 모션 갤러리 맵과 렌더

`PreviewToonMotionTest.py`는 `/Game/Art/ToonTest/Maps/L_PGToon_Inori_MotionTest`에 테스트 전용 맵을 생성한다. 6개의 애니메이션 캐릭터와 외곽선 follower를 배치하고 카메라는 프로젝트 기본 쿼터뷰 회전(-55°, -45°), 거리 1200cm를 사용한다. 외곽선 follower는 Leader Pose로 본 포즈를 공유한다.

에디터에서 이 맵을 열고 Play하면 전용 CameraActor로 모션 갤러리를 확인할 수 있다. 실제 이동 조작/전투가 가능한 플레이어 교체 맵은 아니다. 렌더 재생성은 위 프리뷰 명령의 스크립트를 `PreviewToonMotionTest.py`로 변경한다.

캡처 파일은 `Saved/ToonTest/MotionPreview/<실행ID>/`에 저장된다. 매번 새 경로를 사용해 오래된 이미지가 성공 판정을 만드는 것을 방지한다. `motion_preview.json`의 `CAPTURED`는 파일 생성 완료를 의미하며 육안 품질 검증과 구분한다.

`ConfigureToonOutlineTest.py`는 슬롯별 텍스처 알파를 따르는 `M_PGToonOutlineMasked`와 9개 인스턴스를 만든다. 눈 알파·표정 오버레이에는 Hull을 끄고, 안경 0.08cm/머리 0.12cm/나머지 0.4cm로 두께를 분리했다. 눈·안경의 부분 투명도는 `M_PGToonAlphaOverlay`와 2개 인스턴스로 처리한다. 이는 테스트 맵의 컴포넌트 override이며 메시 기본 머티리얼과 최초 정적 프리뷰는 그대로 유지된다. 알파 블렌드 정렬 비용/아티팩트는 별도 평가 대상이다.

프리뷰 맵에는 고정 수동 노출과 Bloom=0의 전용 PostProcessVolume이 있다. 조명이 없는 Unlit 검증 월드에서 자동 노출이 캐릭터를 과다 노출시키는 것을 방지한다. **현재 Toon 명암의 광원 방향은 머티리얼 파라미터**이며, 실제 Directional Light/Lumen 조명이나 그림자를 자동 수신하는 구현은 아니다.

재현 순서는 `ConfigureToonCharacterTest.py` → `ConfigureToonRetargetTest.py` → `ConfigureToonOutlineTest.py` → `PreviewToonMotionTest.py` → `ValidateToonMotionTest.py`다. 캐릭터 임포트가 이미 완료됐다면 2번째 단계부터 실행한다.

### 저장/수치 검증

- `ValidateToonMotionTest.py`는 저장된 맵을 새 프로세스에서 다시 읽는다.
- 6개 Sequence의 21개 시점, 8개 팔·다리 구간 길이 변동 < 0.01cm: PASS
- 원본 메시·애니메이션 해시 불변: PASS
- 6개 반복 재생 설정(PlayRate=1), 6개 외곽선 Leader Pose 참조의 저장 유지: PASS
- 카메라 Pitch=-55°, Yaw=-45°, Roll=0° 및 Player 0 자동 활성화: PASS
- 결과: `Saved/ToonTest/motion_validation.json`

이는 본 데이터와 저장 참조의 검사다. 전체 프레임 스킨 변형, 의상 관통, 실제 플레이어 게임플레이, 얼굴 품질이나 GPU 성능을 자동 보증하지 않는다.

### 최종 캡처와 PIE 확인 (2026-10-03)

- 최종 실행: `Saved/ToonTest/MotionPreview/20261003T080733499307Z/`
- `QuarterView_AllMotions.png`, `Idle_Close.png`, `Run_Close.png`, `Attack_Close.png`: 1280×720 파일 생성 및 육안 확인 완료
- 셀 명암·외곽선·부분 투명 안경·팔/다리 변형이 대표 포즈에서 출력되는 것 확인. 근접 캡처에 주요 캐릭터의 전신이 들어온다.
- 실제 PIE 월드에서 6개 Sequence의 재생 시간이 진행되고, 6개 외곽선이 PIE 내부 Leader Pose를 참조하는 검사: PASS (`motion_preview.json`의 `pie_playback`)
- 최종 렌더 로그에 Python 실행 오류 및 머티리얼 셰이더 컴파일 실패 없음
- 아직 부족한 부분: 얼굴 명암/헤어 실루엣 세부 품질, 동적인 발 접지, 의상·머리카락 관통, 무기 없는 공격 모션의 연출, 화면 공간 외곽선 비용 비교

`Content`와 `Saved`는 현재 저장소의 ignore 대상이다. 생성 도구/설정/문서와 별개로 바이너리 테스트 에셋을 공유하려면 해당 Content 폴더의 별도 백업 또는 에셋 버전 관리가 필요하다.

## 3차 테스트 — Bokusei / Honoka / LianLian 추가

요청한 세 캐릭터의 기본 FBX를 전용 Skeleton과 함께 임포트했다. 별도 악기 FBX(Bokusei), 상반신/대체 팔레트(Honoka), Kisekae 의상 변형(LianLian)은 이번 범위에서 제외했다.

| 모델 | FBX 상대 경로 | 머티리얼 슬롯 | 사용 텍스처 |
| --- | --- | ---: | ---: |
| Bokusei | `ROKO SHOP/Bokusei/00_FBX/Bokusei.fbx` | 10 | 10 |
| Honoka | `Honoka/FBX/Honoka.fbx` | 16 | 12 |
| LianLian | `LianLian/FBX/LianLian.fbx` | 6 | 6 |

- 에셋: `/Game/Art/ToonTest/<모델명>/SK_<모델명>_ToonTest`
- 비교 씬: `/Game/Art/ToonTest/Maps/L_PGToon_CharacterGallery`
- 씬에는 Inori(기존 Idle)와 새 세 모델(Reference Pose/T 포즈), 이름 표기, 외곽선 follower 4개, 고정 노출/CameraActor를 배치했다. Play 시 비교 카메라를 사용하며 조작 가능한 플레이어 맵은 아니다.
- Unity `.mat`의 GUID를 해당 패키지의 `.meta`와 대조해 실제 텍스처를 연결한다. 파일명을 보고 텍스처를 추정하지 않는다.
- 전용 신규 마스터 `M_PGToonCharacterMulti`, `M_PGToonCharacterMultiTransparent`, `M_PGToonOutlineMulti`는 별도 알파 마스크의 대체/곱/합/차 모드를 지원한다. 기존 Inori 마스터는 재생성하지 않는다.
- Honoka의 눈물 재질은 원본에 메인 텍스처가 없으므로 흰 기본 텍스처에 원본 색상/알파(약 0.11)를 적용한다. 얼굴·투명 슬롯의 Hull을 꺼 내부 얼굴 덮임을 줄였다.
- 기존 테스트 폴더와 비교 맵은 재생성 전에 실행별 `Saved/ToonTest/AdditionalImport/.../backup` 또는 `Gallery/.../previous_map.umap`으로 보관한다. 기존 메시가 있으면 FBX를 재임포트하지 않고 슬롯 연결만 갱신한다.

Honoka는 전체 FBX 바운드 기준 바닥 배치에서 발가락 본이 약 26.53cm 위에 있어 `characters.json`의 `gallery_z_adjust_cm=-23.5`로 비교 씬 배치만 보정했다(보정 후 약 3.03cm). 원본 메시·Skeleton·게임플레이 스케일은 변경하지 않는다.

최종 캡처는 `Saved/ToonTest/Gallery/20261003T084118534876Z/`의 1280×720 이미지 4장이다. 전체/개별 화면의 기본 색상·셀 명암·외곽선 출력과 Honoka 바닥 배치를 육안 확인했다. 최종 렌더 로그에 Python 실행 오류 및 머티리얼 컴파일 실패는 없으며, 바닥 보정 후 새 프로세스의 저장 참조/원본 불변/Inori 모션 회귀 검증도 PASS다. 이는 실전 애니메이션·얼굴 품질·성능 인증과 구분한다.

### 재현 및 검증

기존 Inori 테스트 에셋이 있는 상태에서 실행한다. 위 commandlet/렌더 명령의 스크립트를 각각 다음 순서로 바꾼다.

1. Commandlet: `ConfigureToonAdditionalCharacters.py`
2. 전체 에디터/RenderOffscreen: `PreviewToonCharacterGallery.py`
3. Commandlet: `ValidateToonCharacterGallery.py`

설정은 `Tools/Art/ToonTest/characters.json`에 있다. 임포트 결과 `Saved/ToonTest/additional_import.json`은 세 모델 모두 PASS(슬롯 32/32, 텍스처 28개). `gallery_validation.json`은 원본 FBX·머티리얼·텍스처 63개 해시 불변, 저장된 4개 본체/4개 Leader Pose, 올바른 마스터/스켈레톤 연결, Player 0 비교 카메라와 기존 Inori 모션 회귀 검사를 통과했다.

렌더 결과는 `Saved/ToonTest/Gallery/<실행ID>/`의 `AllCharacters.png`, `Bokusei_Close.png`, `Honoka_Close.png`, `LianLian_Close.png` 및 `Saved/ToonTest/gallery.json`으로 확인한다. 초기 60초 워밍업과 카메라 변경 후 별도 안정화 프레임을 두며, 전체 화면은 개별 모델 촬영 뒤 마지막에 촬영한다. 파일 생성 완료와 육안 품질 확인은 별도이다.

새 모델은 **애니메이션 리타기팅을 하지 않았다**. 원본 FBX의 Bind Pose/노멀·탄젠트 경고가 있고, Unity Prefab의 모프 기본값·의상 활성화·보조 물리·lilToon 고급 효과는 자동 이전하지 않는다. 현재는 메시/재질 비교용 프로토타입이며 실전 채택 전 모델별 얼굴·스킨 변형·투명 정렬 QA가 필요하다. Honoka의 전체 FBX 바운드는 약 244cm로 가시 전신보다 크며, 게임용 스케일은 바운드만으로 자동 정규화하지 않았다.

## 4차 — 재질 이전 오류 수정 및 셰이딩 폴리싱 (2026-10-03)

### 조사와 확인한 문제

- [lilToon 그림자 공식 문서](https://lilxyzw.github.io/lilToon/ja_JP/color/shadow.html): 그림자 색·경계 폭·강도를 별도로 조절하고 얼굴 등의 일부 영역을 약하게 처리한다. 기존 테스트는 모든 부위에 같은 차가운 3단 명암을 적용했다.
- [lilToon 림 공식 문서](https://lilxyzw.github.io/lilToon/ja_JP/reflections/rimlight.html): 광원 방향, 그림자 및 뒷면 마스크를 사용한다. 기존 단순 Fresnel 가산은 어두운 옷·소매 안쪽까지 푸르게 빛나게 했다.
- [Epic Utility Expressions](https://dev.epicgames.com/documentation/en-us/unreal-engine/utility-material-expressions-in-unreal-engine): 픽셀 셰이더 미분과 픽셀 커버리지에 따른 경계 완화 설명을 참고했다. 새 구현은 `fwidth`에 기반한 최소 전이 폭을 가진 셀 경계를 사용한다.
- [lilToon 외곽선 공식 문서](https://lilxyzw.github.io/lilToon/ja_JP/advanced/outline.html): 하드 노멀과 얼굴용 수정 노멀에서 Hull 외곽선이 깨질 수 있다는 제한을 확인했다. 이번에는 거리·부위별 두께를 조절하며, 별도 스무딩 노멀 베이크는 아직 하지 않았다.

근접 렌더에서 **Honoka의 눈동자가 사라지고 눈 전체가 하얗게 덮이는 실제 이전 오류**를 확인했다. `ho_eye`의 원본은 불투명 `lilToon`인데 `_Color.a=0`을 불투명도에 곱해 지워졌고, `ho_eye.hi`는 `Hidden/lilToonTransparent`인데 기본 렌더 큐 -1과 이름만으로 판정해 불투명 마스크로 처리했다. 원본 PackageCache의 `.shader.meta` GUID와 셰이더 선언을 대조해 원인을 확인했다. 앞선 전체/전신 렌더 PASS는 이 얼굴 오류를 검출하지 못했다.

### 변경 내용

- `ToonMaterialSource.py`는 원본 셰이더 GUID를 읽어 불투명/컷아웃/투명을 구분한다. 불투명은 메인 텍스처·색상 알파를 무시하고, 컷아웃과 투명은 원본 알파를 유지한다. 셰이더를 못 찾은 경우 폴백임을 보고서에 명시한다. 모든 lilToon 고급 블렌드 모드를 재현하는 범용 변환기는 아니다.
- `ToonShading.hlsl`에 안전한 벡터 정규화, 미분 기반 3단 명암 전이, DiffuseWrap, ShadeStrength, 광원·뒷면 제한 림, 선택적 셀 하이라이트를 구현했다. `StateColor`, `StateGlow`, `DissolveAmount` 인터페이스는 유지한다.
- `shading_profiles.json`에서 얼굴/피부/머리/의상/금속/디테일의 색과 강도를 명시적으로 매핑한다. 이는 프로젝트용 아트 설정이며 원본 lilToon의 모든 파라미터를 1:1 복제한 값은 아니다. 얼굴은 따뜻한 저대비 음영, 머리는 제한된 하이라이트, 의상은 낮은 림 강도를 사용한다.
- 외곽선은 기준 거리 400cm 대비 0.4~1.6배로 제한한다. 화면 픽셀 두께 고정 방식은 아니므로 FOV·해상도에 따라 달라진다. 추가 모델의 머리는 0.18cm, 의상은 0.3cm 기준이며 얼굴·투명 오버레이의 Hull은 계속 비활성화한다. 본체와 외곽선의 알파 의미 및 Dissolve를 맞춘다.
- `ConfigureToonShading.py`는 변경 전 재질을 실행별 백업하고 기존 에셋을 갱신한다. FBX 재임포트와 맵 저장은 하지 않는다. 추가 모델 메시의 슬롯 참조를 재저장하며 기존 Skeleton·애니메이션·맵 17개 해시를 검사한다.

### 재현과 검증

기존 1~3차 테스트 에셋이 있는 상태에서 실행한다.

1. Commandlet: `ConfigureToonShading.py`
2. 전체 에디터 / RenderOffscreen / SM6: `PreviewToonShading.py`
3. 새 Commandlet 프로세스: `ValidateToonCharacterGallery.py`
4. 일반 Python: `Tools/Validation/TestToonMaterialSource.py`

`PreviewToonShading.py`는 저장된 갤러리를 로드하되 저장하지 않는다. 광원 변화는 임시 MID로만 적용하며, 고정 구도·노출·해상도의 근접/쿼터뷰/측면광/역광 이미지를 실행별 폴더에 남긴다. `PG_TOON_CAPTURE_TAG` 환경변수의 기본값은 `after`이며 `before`로 지정하면 비교 전 기록을 만들 수 있다. Honoka의 별도 `Honoka_Face`는 머리 본을 기준으로 구도를 잡는다. 원래의 `Honoka_Portrait`는 전후 비교를 위해 기존 흉상 구도(머리 상단이 잘림)를 유지한다.

- 알파 처리 회귀 테스트 4개: PASS (불투명 알파 0, 기본 큐 -1의 투명 하이라이트, 컷아웃, 부분 투명 눈물).
- 업그레이드 에셋 저장: PASS (`Saved/ToonTest/shading_upgrade.json`).
- 새 프로세스의 프로파일 값·부모 재질·알파/외곽선 연결 검사: PASS.
- 원본 63개 파일 불변, 기존 Inori 모션 6개와 저장 갤러리 참조 회귀: PASS (`gallery_validation.json`).
- GPU 컴파일 및 전후 육안 검토 결과는 아래 최종 렌더 기록에 별도로 남긴다. NullRHI 검사를 GPU 품질 검사로 취급하지 않는다.

### 남은 품질 과제

실제 월드 광원·그림자 수신, 얼굴 SDF 또는 본 추종 명암 제어, 헤어 전용 이방성 하이라이트/스무딩 노멀, 투명 정렬과 모프/보조 물리, 움직이는 카메라에서의 시간축 안정성, 다수 캐릭터 GPU 비용 측정이 남아 있다. 현재 HLSL의 `LightDirection`은 빛이 진행하는 방향이며, 새로운 파라미터 조절만으로 Unreal 광원과 자동 동기화되지는 않는다. 거리별 정지 렌더 확인은 움직임의 깜빡임 검증을 대체하지 않는다.

### 최종 렌더 기록

- 변경 전: `Saved/ToonTest/ShadingPreview/20261003T100344798905Z_before/` (8장).
- 변경 후: `Saved/ToonTest/ShadingPreview/20261003T101646469693Z_after/` (9장, 1280×720, DX12/PCD3D_SM6, RTX 3060 Ti).
- 비교 가능한 8개 구도와 추가 Honoka 머리 구도를 확인했다. Honoka 눈동자·하이라이트 복원, Bokusei의 푸른 림 감소, LianLian 얼굴의 강한 회색 명암 경계 완화를 확인했다. Inori의 안경·눈 투명 오버레이도 유지된다.
- 최종 `shading_after.log`에 Python 실행 오류 및 머티리얼/셰이더 컴파일 실패가 없고 에디터가 정상 종료됐다. 첫 sandbox 렌더는 사용자 프로필의 `UnrealShaderWorkingDir` 쓰기 제한으로 중단하고, 허용된 실행으로 재검증했다. 이 실패 실행의 이미지는 최종 결과에 포함하지 않는다.
- 변경 전에도 있던 Inori 의상의 삼각형 얼룩·변형과 머리카락의 가는 내부 Hull 선, LianLian 볼의 거친 색 영역은 여전히 보인다. 따라서 **이번 변경 항목의 육안 검토 완료**이며 캐릭터 전체 상용 품질 PASS를 뜻하지 않는다. 다음 시각 품질 작업은 이 부분과 얼굴/월드 광원 연동을 우선한다.
- 검토 요약: `Saved/ToonTest/shading_visual_review.json`. 기존 `gallery.json`은 3차 전신 캡처 기록이므로 이번 결과는 `shading_preview_after.json`을 사용한다.

## 5차 — 월드 조명·본 추종·실루엣 외곽선 및 런타임 검증 (2026-10-03)

### 적용 범위와 조사 근거

[Epic Shading Models](https://dev.epicgames.com/documentation/en-us/unreal-engine/shading-models-in-unreal-engine)의 Default Lit/Unlit 차이를 기준으로, 엔진을 포크하지 않는 Default Lit 변형을 추가했다. 3단 아트 명암을 Base Color에 적용하고 제한된 Emissive 채움으로 얼굴/암부 가독성을 보존한다. 실제 직접광·간접광·그림자는 엔진이 계산한다. **모든 월드 광원을 정확히 3단으로 양자화하는 커스텀 Shading Model은 아니다.**

[Guilty Gear Xrd 제작 발표](https://www.ggxrd.com/Motomura_Junya_GuiltyGearXrd.pdf)의 얼굴용 아트 제어와 [lilToon 그림자 제어](https://lilxyzw.github.io/lilToon/ja_JP/color/shadow.html)를 참고해, 머리 본 좌표계로 얼굴 명암을 제어한다. SDF 텍스처를 새로 만든 방식은 아니며, 원본 기하 노멀과 본의 정면·오른쪽 축으로 완만한 아트 노멀을 만든다. 엔진 조명에는 원본 기하 노멀을 유지한다.

[Epic Post Process Materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-materials-in-unreal-engine)와 설치된 UE 5.8 셰이더 소스를 대조해 CustomDepth/Stencil 기반 실루엣 외곽선을 구성했다. DOF/TSR 이전 패스에서 버퍼 UV와 texel 크기를 사용하고, 주변 픽셀과 중심 픽셀의 SceneDepth를 모두 검사한다. 따라서 전경 물체 뒤 캐릭터의 외곽선을 그리지 않는다. 내부 노멀/깊이 경계는 그리지 않아 헤어 내부 Hull 선을 제거한다.

### 구현과 사용

- 에셋: `/Game/Art/ToonTest/Advanced`, 원본 슬롯과 41개 대응 MI. 기존 메시·Skeleton·애니메이션·초기 갤러리는 유지한다. 새로운 마스터 4종과 화면 공간 외곽선, 테스트 바닥 재질을 별도로 저장한다.
- `WorldLightingInfluence`: 얼굴/디테일 0.55, 다른 투명 0.75, 의상·헤어 등 0.88. 1이면 발광 채움 없이 실제 조명에 의존한다. 0이면 월드 광원 반응이 사라지므로 장면 노출과 함께 조절한다.
- `FaceShading` 0.85와 `HeadForwardWS`/`HeadRightWS`로 얼굴 명암을 완화한다. `ShadowCast`는 마스크 재질의 그림자 패스에만 적용하며, 얼굴/디테일은 0으로 코·입 주변의 작은 투사 그림자를 생략한다. 헤어/다른 물체의 그림자 수신과 CustomDepth 실루엣은 유지한다. 얼굴 기하 자체의 바닥 투사도 생략되는 아트 선택이며, 필요하면 해당 MI 값을 1로 돌린다.
- 헤어 `HairAnisotropy` 0.8은 메시 탄젠트를 따라 펼쳐지는 제한된 하이라이트다. 원본 tangent/UV 방향 품질에 의존하며, 새 헤어 flow map이나 노멀 베이크는 아니다.
- `UPGToonPresentationComponent`: BeginPlay에 슬롯별 MID를 생성하고 지정한 광원 방향 및 애니메이션 머리 축을 PostPhysics에서 갱신한다. 매 프레임 액터 검색·MID 생성·포즈 복제를 하지 않는다. 재초기화/EndPlay 시 자신이 여전히 소유한 슬롯만 복구해 다른 시스템의 재질 교체를 보존한다. 광원이나 본이 없으면 명시적 폴백을 사용한다.
- 본마다 로컬 축이 다르므로 테스트 생성기가 Reference Pose로 `HeadForwardAxis`/`HeadRightAxis`를 보정해 저장한다. 실제 캐릭터에 붙일 때도 `KeyLight`, `HeadBone`, 이 두 축을 설정하고 메시 교체 후 `Initialize`를 호출한다. 에디터 맵을 단순히 연 상태에서는 저장된 MI를 사용하며 자동 MID 동기화는 PIE/BeginPlay에서 시작한다.
- `StateColor`, `StateGlow`, `DissolveAmount`는 안경·눈 오버레이를 포함해 유지한다. `Initialize(nullptr)`는 기존 MID를 해제하므로 풀 재사용 시 새 상태를 다시 지정한다.
- 프로젝트 `r.CustomDepth=3`, 본체 stencil 73, 후처리 재질 `M_PGToonScreenOutline`을 함께 사용한다. 새 테스트 맵에는 Hull 복제 액터가 없다. 기본 폭 1.15는 **TSR 이전 렌더 픽셀** 단위라 화면 비율 50%에서 출력 픽셀 기준으로 두꺼워질 수 있다. 실루엣 전용이므로 교차하는 두 캐릭터의 같은 stencil 경계선은 추가하지 않는다.

### 원본 비교로 바로잡은 진단

4차 기록의 Inori 의상 ‘삼각형 얼룩’은 `Tex_Inori_Costume_A.png`에 직접 그려진 원래 무늬였다. 원본 디자인을 삭제하지 않는다. 별도 Reference Pose 본체와 기존 idle의 쿼터뷰를 비교했으며, 뒤로 넓어지는 외투 실루엣은 셰이더/Hull을 꺼도 존재한다. 본 작업은 보조 Dress 본의 물리/의상 스키닝을 새로 제작하지 않는다. 공격 등에서 의상의 변형은 모션용 데이터의 별도 품질 항목으로 구분한다.

### 재현 순서

1. C++: `Build.bat UPlaygroundEditor Win64 Development -Project=<uproject> -Module=PGActor -WaitMutex -NoHotReloadFromIDE`.
2. NullRHI commandlet: `ConfigureToonLightingLab.py`.
3. 전체 에디터 SM6: `PreviewToonLightingLab.py` → `L_PGToon_LightingLab`, 4개 얼굴·외부 그림자·가림·무광원·측면광·기준 포즈·상태색·완전 소멸·쿼터뷰/전체 14구도.
4. 전체 에디터 SM6, `-csvGpuStats`: `ValidateToonLightingRuntime.py` → `L_PGToon_AdvancedMotionTest`, 6모션/화면 비율 비교, 실제 PIE 파라미터 및 수명주기 검사, 연속 카메라/광원 이동, 1→10→50→50(외곽선 없음)→50→1 CSV.
5. 새 NullRHI 프로세스: `ValidateToonLightingAssets.py` → 41개 MI/저장 맵과 원본 갤러리·모션 해시 회귀. 일반 Python의 `TestToonMaterialSource.py`도 실행한다.
6. 일반 Python: `SummarizeToonLightingRuntime.py` → 실제 1280×720 여부와 CSV 프레임/GPU 샘플을 검사해 `lighting_performance.json`을 생성한다.

위 스크립트는 `-DisablePlugins=RiderLink -EnablePlugins=PythonScriptPlugin -ddc=InstalledNoZenLocalFallback`을 사용한다. 렌더는 `-RenderOffscreen -windowed -ForceRes -ResX=1280 -ResY=720 -unattended -NoSound`을 사용한다. `Saved/ToonTest/lighting_*.json`이 최신 결과이며 실행별 이미지/백업/CSV 원본 경로를 기록한다. 작업 디렉터리의 다른 AI/전투 변경은 이 셰이딩 작업의 검증 대상이 아니다.

`BP_PGToonLabGameMode`는 테스트 화면에 불필요한 기본 Pawn/HUD를 생성하지 않는다. `APGToonPreviewActor`의 테스트용 정적 함수는 실제 Game World 액터 생성과 고정 렌더 타깃 크기를 제공한다. Python의 에디터 전용 액터 생성으로 PIE 검사를 대신하지 않는다. `SetPreviewViewportSize(1280,720)`은 에디터 패널 크기와 무관하게 적용하며 종료 시 0/0으로 해제한다. 초기 1272×390 측정은 최종 성능 결과에서 제외했다.

### 최종 기능 검증

- PGActor C++ 빌드: PASS (`Saved/ToonTest/pgactor_build.log`). 기존 엔진 API deprecation·순환 참조 경고가 있으나 새 코드 컴파일/링크 오류는 없다.
- 최종 14구도: `Saved/ToonTest/LightingPreview/20261003T110821977646Z/`. 얼굴 자체 그림자 감소, 기존 눈동자/투명 오버레이, 외부 그림자 수신, 무광원 반응, Hull 내부 선 제거, 전체 가림 시 외곽선 누출 없음, 상태색/완전 소멸과 그림자 제거를 확인했다. 원본 의상 무늬는 유지했다.
- 실제 PIE: `Saved/ToonTest/Runtime/20261003T112517983305Z/`. 6개 모션의 재생 시간 변화 및 현재 머리 본 축과 MID 값 일치, 움직인 광원의 방향 동기화, 20회 중복 재초기화·20회 생성/파괴, 다른 시스템의 재질 교체 보존, 누락 광원/본의 유효한 폴백을 모두 통과했다. 1280×720 렌더 크기를 각 계측 구간 시작에도 재검사했다.
- 저장 에셋: `lighting_validation.json`의 41개 MI, 4개 조명 실험 액터, 6개 모션 액터, 지속 가능한 MI 참조와 GameMode를 검사했다. 저장 맵에 transient MID나 Hull 복제는 없다. 원본 63개 파일 불변 및 기존 Inori 모션/초기 갤러리 검증도 PASS다.
- 알파 해석 회귀 4개: PASS. NullRHI 결과와 실제 SM6 렌더 결과를 구분해 기록한다.
- 연속 카메라/광원 이동 8초 중 4개 시점을 캡처했다. 검토한 시점에서 실루엣 끊김은 보이지 않는다. 이 표본은 긴 영상의 미세한 깜빡임 전체를 인증하지 않는다. 화면 비율 50/100 비교는 HighResShot의 캡처로, 실제 동적 해상도 전환 영상 검증과 구분한다.

### 남는 적용 한계

새 3개 모델은 재질 비교용 Reference Pose이고, Inori의 기존 6개 리타깃 클립을 동적 검증 대상으로 사용했다. 얼굴 SDF, 전용 헤어 flow map, 모프 기본값·보조 물리·의상 스키닝은 이번 셰이딩 단계에서 새로 제작하지 않았다. 실제 플레이어/전투 맵 채택 및 패키지 성능 인증까지 완료했다는 뜻은 아니다. 전투용 채택 시 아트 방향에 맞는 얼굴/헤어 그림자 강도와 쿼터뷰 LOD·투명 정렬을 실제 장면에서 결정해야 한다.

### 720p 성능 관측과 판정

RTX 3060 Ti / 드라이버 581.29, 실제 1280×720, ScreenPercentage 100, VSync/FPS 제한 없음, Shadow/PostProcess 품질 3. 각 구간 8초 준비 후 최소 12초 CSV를 기록했다. 백그라운드 에디터 CPU 절전은 해당 프로세스에서만 해제하고 종료 시 원래 값으로 복구한다.

| 조건 | 프레임 수 | GPU 평균 ms | 프레임 p95 ms |
|---|---:|---:|---:|
| 1개 + 외곽선 | 459 | 5.990 | 18.632 |
| 10개 + 외곽선 | 969 | 7.553 | 16.353 |
| 50개 + 외곽선 | 665 | 16.758 | 41.295 |
| 50개 외곽선 끔 | 287 | 39.028 | 62.685 |
| 50개 + 외곽선 반복 | 123 | 94.596 | 215.434 |
| 1개로 복귀 | 325 | 22.265 | 69.380 |

**판정: `UNSTABLE_PERFORMANCE_SAMPLE`.** 같은 50개 조건의 GPU 평균이 약 5.64배 달랐다. 측정 중 다른 UnrealEditor 프로세스도 관찰됐고, 시스템 부하·프레임 정지 영향을 분리하지 못했으므로 위 수치로 외곽선 자체 비용이나 60fps 달성을 주장하지 않는다. 원시 기록을 유지하고 비용 차이 필드는 null로 남긴다. 최초 720p 실행은 에디터 절전에 따른 20프레임 구간을 검출해 제외했고, 최종 실행은 모든 구간에서 최소 120프레임을 확보했지만 반복 안정성 기준을 통과하지 못했다. 기능/원본 회귀 PASS와 성능 판정은 별개다.

원본 CSV, GPU 패스별 평균/p95와 주의 사항은 `Saved/ToonTest/lighting_performance.json` 및 해당 Runtime 폴더의 `performance.json`에 있다. 이는 AI/전투가 없는 에디터 아트 실험이며 패키지 성능이나 장시간 메모리 누수를 인증하지 않는다.

## 6차 — 성능 검증·LOD 최적화 (2026-10-03)

이번 작업은 후속 순서 중 **1단계 성능 검증·최적화**다. 연속 영상/해상도 전환 품질, 실제 전투 맵·피격/디졸브 연동, 얼굴 SDF·헤어 방향 맵은 별도 후속 단계로 유지한다.

### 재현 도구와 판정 기준

- `RunToonPerformance.py`는 실행 중인 Unreal/game/compiler 프로세스가 있으면 시작하지 않는다. 실행 중에도 약 3초 간격의 프로세스 목록과 GPU 사용률·메모리·온도·클럭·전력을 저장한다. 기존 앱을 닫지 않는다. 다른 데스크톱 앱의 GPU 사용까지 완전히 배제하는 장치는 아니다.
- `ProbeToonPerformance.py`는 저장하지 않는 PIE 월드에서 1/10/50개 Inori를 렌더한다. 실제 출력 크기 1280×720, ScreenPercentage=100, 동적 해상도/VSync/FPS 제한 해제, 기존 Shadow/PostProcess 품질을 기록한다. 50개 모두 들어오는 고정 카메라를 사용한다.
- 일반 실행은 후처리만 제거 → Custom Depth까지 제거 → 투명 두 슬롯 제거 → 캐릭터 투사 그림자 제거를 기본 조건 반복 사이에 배치한다. 제거는 진단용이며 제품 프리셋으로 저장하지 않는다. 투명도를 0으로 설정하는 대신 실제 메시 섹션을 숨긴다.
- `--lod-candidate`는 원본 LOD0과 후보 LOD1/LOD2를 교대로 비교한다. 각 후보를 두 번, 원본을 다섯 번 측정한다. 기본 조건 최대/최소 GPU 평균 비율과 후보 반복 비율이 각각 1.15 이하여야 비용 차이를 제공한다. 불안정·동시 실행·누락 CSV·유효 샘플 부족은 성공으로 처리하지 않는다.
- 모든 측정 구간은 재생 시간 진행과 실제 손 본 위치 변화를 확인해야 한다. 비교 캡처는 CSV 종료 뒤 같은 포즈로 정지해 생성하며, 다음 구간에 다시 준비 시간을 둔다. 이미지는 연속 영상 품질 인증이 아니다.
- 최초 도구 실행 `20261003T114426941516Z`와 중단한 `20261003T115421699379Z`는 동적 생성 액터의 T 포즈가 캡처에서 확인되어 **무효**다. 각 폴더의 `invalidated.json`을 따른다. 재질/섹션 편집 이후 재생 데이터·런타임 재생·컴포넌트 틱을 초기화하고 본 움직임 검사를 추가했다. 이 두 실행의 수치를 움직이는 캐릭터 비용으로 사용하지 않는다.

### 최적화 후보

`ConfigureToonPerformanceLOD.py`는 원본을 복제한 `/Game/Art/ToonTest/Advanced/Performance/SK_Inori_ToonLOD`에 3개 LOD를 생성한다. 원본 LOD0 정점 수와 원본 파일 SHA-256을 검사한다. Skeleton·PhysicsAsset·머티리얼 슬롯 참조는 공유하며 기존 게임 캐릭터나 맵을 교체하지 않는다.

| LOD | 정점 수 | 용도 |
|---|---:|---|
| 원본/후보 LOD0 | 102,860 | 비교 기준 |
| 후보 LOD1 | 30,137 | 쿼터뷰 검토 후보 |
| 후보 LOD2 | 16,759 | 원거리 검토 후보 |

기존 얼굴/디테일 재질에서 이미 투사 그림자를 제거한 슬롯 0·2와 투명 오버레이 1·5는 후보 메시 섹션에서도 그림자를 끈다. 헤어·몸·의상 그림자, 화면 공간 외곽선, 투명 눈/안경은 유지한다. 감소 대상은 여러 패스에서 반복 처리하는 메시 비용이다. 투명 재질을 불투명으로 바꾸거나 눈·안경을 삭제하는 최적화는 적용하지 않는다.

별도 맵 `/Game/Art/ToonTest/Maps/L_PGToon_PerformanceMotionTest`는 기존 6개 모션을 후보 LOD1로 확인하기 위한 경로다. 강제 LOD 값은 UE의 1 기반 값 `2`다. 근접/원거리 전환 임계값을 제품 기준으로 확정한 상태는 아니다.

원본의 길이 0 법선 및 비다양체 경고(공유 과다 에지 약 0.75%)가 LOD 생성에서도 관찰된다. 축소 메시의 근접 얼굴/헤어·모프·연속 애니메이션·LOD 전환 검증은 남아 있다. 의상 물리나 리그를 새로 구현한 작업이 아니다.

### 실행 순서

엔진 경로는 `UPlayground.uproject`의 5.8을 따른다. 일반 Python 명령은 UE 번들 Python(`Engine/Binaries/ThirdParty/Python3/Win64/python.exe`)으로 실행한다.

1. 기존 에셋 생성 commandlet 방식으로 `ConfigureToonPerformanceLOD.py`를 실행한다. 재실행 시 후보 메시와 후보 맵을 실행별 폴더에 백업한다.
2. `RunToonPerformance.py --lod-candidate --warmup 12 --sample 20`으로 반복 비교한다.
3. `RunToonPerformance.py`로 각 기능의 제거 비교를 실행한다. 두 GPU 검증을 동시에 실행하지 않는다.
4. 새 NullRHI commandlet에서 `ValidateToonPerformanceLOD.py`로 저장된 LOD/섹션/맵과 기존 조명·갤러리 회귀를 검사한다.
5. `TestToonPerformance.py`는 오염·불안정·누락·GPU 0·정적 포즈를 잘못 채택하지 않는지 검사한다.

실행별 `Saved/ToonTest/Performance/<ID>/`에 명령, 옵션, 프로세스/GPU 기록, 원본 CSV, runtime/report JSON 및 후보 비교 이미지를 보존한다. `Saved/ToonTest/performance_latest.json`은 가장 최근 실행만 가리킨다. 후보 생성/저장 검사는 `performance_lod.json` / `performance_lod_validation.json`에 기록한다. `Content`와 `Saved`는 저장소 ignore 대상이므로 바이너리 에셋과 증거의 별도 보관이 필요하다.

### 정상 모션 50개 반복 비교 결과

실행 `Saved/ToonTest/Performance/20261003T115800313221Z/`, RTX 3060 Ti, 1280×720. 각 구간 12초 준비 뒤 20초 측정했다. 모든 구간에서 애니메이션 시간/본 움직임을 확인했으며, 측정 중 다른 Unreal/game/compiler 프로세스는 관찰되지 않았다.

| 조건 | GPU 평균 범위 ms | GPU 평균의 구간 평균 ms | 프레임 p95 범위 ms |
|---|---:|---:|---:|
| 원본 LOD0, 5회 | 24.693–25.513 | 25.027 | 134.540–152.734 |
| 후보 LOD1, 2회 | 11.856–11.967 | 11.911 | 33.543–35.451 |
| 후보 LOD2, 2회 | 10.799–10.834 | 10.817 | 25.393–26.218 |

기본 조건 최대/최소 비율 1.033, LOD1 1.009, LOD2 1.003으로 GPU 반복 안정성 기준을 통과했다(`CHARACTERIZED`). 전후 기본 조건을 이용한 GPU 절감은 LOD1 12.849–13.059ms, LOD2 14.097–14.472ms다. 단순 구간 평균 대비 약 52%/57% 감소다. LOD와 섹션 그림자 제외를 함께 적용한 후보의 비용이며, 두 조치의 개별 효과로 나누어 주장하지 않는다.

| GPU 패스 평균 ms | 원본 | LOD1 | LOD2 |
|---|---:|---:|---:|
| ShadowDepths | 8.595 | 1.827 | 1.490 |
| CustomDepth | 3.055 | 0.637 | 0.293 |
| RenderVelocities | 3.987 | 0.928 | 0.612 |
| Basepass | 4.776 | 1.256 | 0.788 |
| Translucency | 0.403 | 0.235 | 0.225 |

패스 시간은 겹칠 수 있으므로 합산해 전체 GPU 시간으로 해석하지 않는다. 투명 오버레이를 유지하면서 그림자·외곽선용 깊이·속도·기본 메시 패스 비용이 감소했다.

`full_a.png`, `lod1.png`, `lod2.png`의 동일 6모션 대표 포즈/50개 구도를 육안 확인했다. 모든 캐릭터가 화면에 들어오며 큰 실루엣 손실이나 눈에 띄는 재질 누락은 관찰하지 못했다. 근접 얼굴·헤어와 움직이는 얇은 외곽선 품질은 이 정지 이미지로 인증하지 않는다.

**GPU 개선은 확인했지만 60fps 인증은 미완료다.** 후보도 전체 프레임 p95가 16.7ms를 초과한다. 에디터 PIE의 프레임 지연과 실제 전투/AI·패키지 비용은 별도 계측이 필요하다. 다음 품질 단계의 기본 검토 후보는 LOD1이며 LOD2는 원거리 후보로 보존한다.

### 개별 기능 분리 측정의 제한

`20261003T120431631823Z`는 투명 섹션을 제거하려는 단계에서 UE 5.8의 PIE 중 에디터 전용 메시 조회 제한을 검출해 실패했다. 섹션 매핑을 PIE 시작 전에 캐시하도록 수정했다. 앞선 LOD 비교는 모든 슬롯을 표시하는 조건이었고, 조회 실패 로그는 측정 전 준비 구간에 발생했다. 실제 LOD·본 움직임·화면 출력과 반복 GPU 비교의 근거를 유지하되 이 진단 도구 오류를 숨기지 않는다.

수정 후 `20261003T120842869338Z`는 12개 구간의 동작/본 움직임 검사를 모두 통과했다. 투명 슬롯 제거 시 Translucency 패스도 감소했다. 그러나 프로세스 감시에서 다른 `UnrealEditor-Cmd` PID 27808·34044·368이 관찰됐고 일부가 측정 구간과 겹쳤다. 따라서 전체 성능 판정은 **FAIL(동시 실행 오염)**, 개별 기능 비용 차이는 모두 null이다. 해당 프로세스는 종료하지 않았다. 이 수치에서 외곽선 필터 자체·투명 슬롯·그림자 제거의 정확한 절감량을 확정하지 않는다.

즉, LOD 후보의 GPU 개선은 확인됐지만 **개별 기능 제거의 완전한 단독 재측정과 전체 프레임 지연 원인 분석은 1단계 잔여 작업**이다. 시스템의 다른 Unreal 작업이 없는 구간에 같은 도구를 다시 실행할 수 있다.

### 저장/회귀 검사 기록

- 최종 후보/6모션 맵 생성: `configure_performance_lod_final.log`, PASS. 재생성 후에도 정점 수 102,860 / 30,137 / 16,759 및 그림자 제외 슬롯이 일치한다. 이전 측정 시점의 후보 바이너리는 최종 생성 실행 폴더의 `previous_mesh.uasset`에 보존했다.
- 최초 새 프로세스 검사는 Python 진입 전에 게임 모듈을 찾지 못해 종료했다(`validate_performance_lod.log`). 같은 시각 프로젝트 게임 모듈의 외부 빌드 갱신이 관찰됐으며, 이 실행을 에셋 검사 성공으로 처리하지 않았다.
- 모듈 갱신 뒤 재실행: `validate_performance_lod_retry.log` 및 `performance_lod_validation.json`, **PASS**. 저장된 3개 LOD의 정점·섹션·그림자 설정, Skeleton/PhysicsAsset·머티리얼 참조, 6모션 후보 맵의 강제 LOD와 지속 가능한 MI/애니메이션 참조, 원본 메시 SHA-256 및 기존 조명·갤러리 회귀를 확인했다.
- 일반 Python 회귀: `TestToonPerformance.py` 6개와 `TestToonMaterialSource.py` 4개, 총 **10개 PASS**. 생성·계측·검증 스크립트의 Python 구문 검사도 통과했다. 이번 변경은 C++ 런타임을 수정하지 않아 별도 C++ 빌드를 수행하지 않았다.

## 7차 — 단독 재측정·LOD1 프레임 지연 분석 (2026-10-03)

**즉시 후속 작업인 단독 기능별 재측정과 LOD1 CPU/렌더/에디터 분석을 완료했다.** 원본과 LOD1의 GPU 반복 비교 모두 `CHARACTERIZED`다. 이번 짧은 LOD1 아트 테스트는 프레임 p95 16.7ms 이내지만, 간헐적 지연과 실제 전투·에디터 외 실행·패키지 검증이 남아 있어 1단계 전체 완료 또는 안정적인 60fps 인증으로 확대하지 않는다.

### 실행 환경과 증거

RTX 3060 Ti / 드라이버 581.29, UE 5.8, DX12 SM6, 실제 1280×720, ScreenPercentage 100, VSync 0, t.MaxFPS 0, Shadow/PostProcess 품질 3이다. 각 구간은 12초 준비 후 20초 측정한다. 에디터 자체의 틱 정책은 남아 있으므로 t.MaxFPS 0을 독립 게임의 무제한 실행과 동일하게 취급하지 않는다. 기존 프로젝트 설정 `mass.UseProcessingQueue=0`을 사용했으며 이 작업에서 설정·메시·머티리얼을 변경하지 않았다.

| 실행 폴더 (`Saved/ToonTest/Performance/` 아래) | 용도 | 구간 수 | 판정 |
|---|---|---:|---|
| `20261003T123949041469Z` | 원본 LOD0 기능 제거 비교 | 12 | CHARACTERIZED |
| `20261003T124736086097Z` | LOD1 고정 CPU 추적 | 3 | CHARACTERIZED, 추적 부하 포함 |
| `20261003T125133083390Z` | LOD1 기능 제거 비교, 추적 끔 | 12 | CHARACTERIZED |

모든 구간에서 실제 애니메이션 시간·본 움직임을 확인했다. 프로세스 감시에서 측정과 겹치는 다른 Unreal/game/compiler 실행은 발견되지 않았다. Insights 분석은 렌더 측정을 끝낸 뒤 실행했다. 다른 데스크톱 앱이나 OS 스케줄링까지 제거한 환경은 아니다. 각 폴더의 명령·소스 해시·프로세스/GPU 감시·runtime/report JSON·원본 CSV를 보존한다. 추적 및 LOD1 실행에는 프로젝트 설정과 PGActor 바이너리 해시도 기록했다. 수치 종합은 `Saved/ToonTest/performance_followup.json`이다.

### 기능별 GPU 비용

각 제거 구간의 앞뒤 기본 조건 GPU 평균을 평균낸 값에서 제거 구간 평균을 뺀 차이다. 외곽선 전체 제거는 후처리와 캐릭터 Custom Depth를 함께 끈다. 그림자 제거는 캐릭터의 투사 그림자만 끈다.

| 제거 항목 | 원본 LOD0 GPU 절감 ms | LOD1 GPU 절감 ms |
|---|---:|---:|
| 외곽선 후처리만 | 0.235 | 0.074 |
| 외곽선 후처리 + Custom Depth | 2.012 | 0.505 |
| 투명 두 슬롯 | −0.056 | −0.033 |
| 캐릭터 투사 그림자 | 4.323 | 0.724 |

기본 50개 조건 5회의 GPU 최대/최소 비율은 원본 1.017, LOD1 1.038이다. 1개 복귀 비율도 각각 1.000, 1.030으로 1.15 기준을 통과했다. 이 기준은 큰 환경 변동을 거르는 장치이며 작은 차이의 통계적 유의성을 보증하지 않는다. 특히 LOD1 후처리 절감 0.074ms는 앞뒤 기본 조건 차이 0.328ms보다 작아 정밀한 비용 확정으로 사용하지 않는다.

투명 제거의 음수 차이는 비용 증가가 입증됐다는 뜻이 아니라, 전체 GPU 절감을 분리하지 못했다는 뜻이다. 실제 Translucency 패스는 원본 약 0.279→0.052ms, LOD1 약 0.172→0.078ms로 감소해 섹션 제거 자체는 확인했다. 패스 시간과 제거 차이는 겹칠 수 있으므로 합산하지 않는다. 현재 근거로 눈/안경 투명도를 희생할 이유는 없다.

### 전체 프레임과 CPU/렌더 분리

| 모든 표현을 켠 50개 조건 | GPU 평균 범위 ms | 프레임 p95 범위 ms | 최악 프레임 ms |
|---|---:|---:|---:|
| 원본 LOD0, 5회 | 18.161–18.464 | 75.889–87.375 | 547.973 |
| LOD1, 5회, 추적 끔 | 8.591–8.919 | 11.923–13.837 | 39.841 |

원본 기본 조건 총 5,252프레임, LOD1 기본 조건 총 10,237프레임이다. 이전 6차와 이번 실행의 절대 수치 차이 전체를 새 최적화 효과로 주장하지 않는다. 이번 작업은 런타임 최적화 변경이 아니라 재측정·분석이며, 6차의 같은 실행 내 LOD 절감 근거도 별도로 유지한다. 원본의 큰 지연 전체에 대한 CPU 추적은 이번에 수집하지 않았으므로 원본의 모든 지연 원인을 외곽선으로 단정하지 않는다.

LOD1 추적은 `Trace.RegionBegin/End PGToon_<구간>`으로 CSV 측정 구간을 표시했다. `ExportToonCPU.py`는 준비/로딩을 제외한 GameThread, RenderThread, RHIThread, RHIInterruptThread, RHISubmissionThread, 작업 스레드 통계를 내보낸다. `cpu_summary.json` 및 `cpu_*_PGToon_*.csv`가 근거다. CPU 추적 3회의 프레임 p95는 12.053–12.393ms이며 추적 자체 부하가 포함돼 있다.

| 관찰 대상 | LOD1 추적에서 확인한 내용 | 해석 |
|---|---|---|
| 에디터 UI | `Slate::Tick (Time and Widgets)` 약 3.95–4.00ms/프레임 | 게임 HUD가 없는 테스트에도 에디터 창/위젯 처리 비용이 포함됨 |
| 에디터 Mass | `UMassEntityEditorSubsystem::Tick` 약 1.07–1.09ms/프레임 | 기존 큐 우회 설정 아래에서도 남는 에디터 처리 비용 |
| 캐릭터 표현 CPU | 50개 `ToonPresentation` 합계 약 0.219–0.226ms/프레임 | 머리/광원 MID 동기화가 이 캡처의 주된 CPU 비용은 아님 |
| 렌더 스레드 | 첫 20초의 `SceneRenderBuilder_Render` inclusive 9.462초, `WaitForTasks` exclusive 7.561초 | 렌더 CPU 작업과 작업 대기가 모두 존재함. 두 범위를 합산하지 않음 |
| RHI 스레드 | 첫 20초의 `WaitForTasks` 14.678초 | 대기는 CPU 연산 사용률과 구분. GPU/제출/작업 의존 관계를 함께 봐야 함 |

CSV `RenderThreadTime`이 거의 0이어도 렌더 스레드가 공짜라는 뜻은 아니다. 예를 들어 첫 구간의 렌더 스레드에는 RHI fence 대기 최대 20.97ms도 존재한다. 위 inclusive 시간은 하위 범위를 포함하고 작업 스레드는 병렬이므로, 표를 더해 전체 프레임 시간을 만들거나 모든 대기를 GPU 병목으로 단정하지 않는다.

긴 프레임을 실제 타임라인으로 대조한 결과:

- **48.005ms 프레임:** 내부 `FDirectoryWatcherWindows::Tick` 34.623ms. 에디터 파일 감시가 이 지연의 큰 부분을 차지했다.
- **41.825ms 프레임:** `UWorld_Tick` 30.581ms, 그 안의 PrePhysics/EndPhysics 작업 완료 처리가 각각 약 13.5/15.9ms였다. 부모·자식 범위는 중복이다. 월드 틱의 병렬 작업 완료 지연도 남아 있어 모든 피크가 UI/파일 감시 때문은 아니다.
- **39.473ms 프레임:** 렌더 동기화 18.384ms와 EndPhysics 작업 완료 처리 약 10.7ms가 관찰됐다. 여러 스레드의 진행을 기다리는 지연이며 순수 셰이더 비용과 구분한다.

Insights 이벤트 내보내기는 구간 경계와 겹치는 프레임도 반환한다. 종료 경계를 넘는 64.72ms 프레임은 CSV 종료 이후 작업을 포함하므로 최장 프레임 분석에서 제외했다. 분석기는 시작/종료 시각이 모두 측정 구간 안에 있는 프레임만 원인 대조에 사용하며, 이를 확인하는 회귀 테스트를 추가했다. 원시 이벤트는 그대로 보존한다.

### 도구 재현과 검증

```powershell
$pythonExe = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
& $pythonExe Tools/Validation/RunToonPerformance.py --warmup 12 --sample 20
& $pythonExe Tools/Validation/RunToonPerformance.py --fixed-lod 1 --repeats-only --trace --warmup 12 --sample 20
& $pythonExe Tools/Validation/ExportToonCPU.py Saved/ToonTest/Performance/<추적실행ID>
& $pythonExe Tools/Validation/RunToonPerformance.py --fixed-lod 1 --warmup 12 --sample 20
& $pythonExe -m unittest discover -s Tools/Validation -p TestToonPerformance.py
```

각 명령은 순차 실행한다. `--fixed-lod`는 후보 메시의 지정 LOD로 기능 제거 비교를 수행하고, `--repeats-only`는 동일 50개 조건 3회만 실행한다. 새 CSV 요약은 p99/최대 및 비교 기준 양 끝 값을 함께 보존하며, 렌더 스레드 카운터가 거의 0일 때 CPU 추적 필요성을 경고한다. 회귀 **7개 PASS**, 변경 Python 4개 구문 검사 PASS, 실제 27개 계측 구간 및 CPU 내보내기 PASS다. C++/에셋 변경이 없어 C++ 재빌드와 아트 에셋 회귀 재실행은 하지 않았다.

### 다음 검증의 범위

LOD1을 품질 검토 후보로 유지한다. 다음 성능 검증은 같은 캐릭터 조건의 에디터 외 실행과 실제 전투·Development 패키지에서 진행해, 에디터 UI/파일 감시가 빠진 뒤의 월드 틱·렌더 동기화·프레임 p99/최대를 확인한다. 이번 짧은 p95 통과만으로 안정적인 60fps를 확정하지 않는다. 연속 영상의 헤어/그림자/얇은 외곽선, 해상도·LOD 전환, 피격/사망/장비 교체 연동은 아직 수행하지 않은 후속 항목이다. 모든 광원/그림자의 일정한 셀 단계화와 의상 물리·스키닝 개선도 기존 별도 작업으로 유지한다.


## 2026-10-03 Bokusei 직접 리타게팅: 최초 6모션

사용자가 지정한 대상은 **Bokusei**다. Anime Katana 원본 FBX 리그에서 `SK_Bokusei_ToonTest`로 직접 리타게팅했으며 ElfSelena는 소스·중간·대상으로 사용하지 않았다.

| 표시명 | In_Place 원본 | 길이(초) |
|---|---|---:|
| Idle | AS_Anime_KC_Idle | 3.167 |
| Run | AS_Anime_KC_Run | 0.500 |
| Attack | AS_Anime_KC_Combo_V1 | 3.633 |
| Dodge | AS_Anime_KC_Dodge_V1 | 1.033 |
| Hit | AS_Anime_KC_Get_Hit_V1 | 0.867 |
| Death | AS_Anime_KC_Death | 3.167 |

소스 메시와 클립은 `/Game/Art/AnimationTests/AnimeKatana`, 결과는 `/Game/Art/ToonTest/Bokusei/Animation/PGBokusei_AS_Anime_KC_*`에 있다. 원본 리그 92본과 Bokusei 302본을 21개 체인으로 연결하고 pelvis 이동과 FK를 사용한다. 설정은 `IK_PGAnimeKatana_Source`, `IK_PGBokusei_Target`, `RTG_PGAnimeKatana_Bokusei`다.

`L_PGToon_Bokusei_MotionTest`는 6개 본체와 Leader Pose 외곽선으로 구성된 반복 재생 맵이다. 명세는 `Tools/Art/ToonTest/Bokusei/motion_manifest.json`, 생성·렌더·저장 검증은 `Configure/Preview/ValidateBokuseiMotionTest.py`, 순차 실행은 `RunBokuseiMotionTest.py`가 담당한다.

### 최초 검증 근거

- 생성 PASS: `Saved/BokuseiMotion/20261003T131016846324Z/configure.json`. 클립별 31개 시점, 원본 FBX와 Bokusei 메시·Skeleton·PhysicsAsset 해시를 검사했다.
- 최종 SM6 이미지 9장: `Saved/BokuseiMotion/Preview/20261003T132221030514Z`. 실행 기록은 `Runs/20261003T132208610474Z`다. 6개 근접 포즈, 공격의 추가 시점 2개, 전체 쿼터뷰를 확인했다.
- 정지 에디터에서 `set_position`만으로는 본이 갱신되지 않아 초기 공격 이미지가 동일했던 문제를 수정했다. `set_position`과 `override_animation_data`를 함께 사용한 최종 15%·35%·70% 공격 포즈는 서로 다르며, 소켓과 AnimPose 비교 최대 오차는 0.004222cm다. `20261003T131524489207Z` 이미지는 최종 시점 변화의 근거로 사용하지 않는다.
- PIE 본체 6개 시간 진행과 실제 본 움직임, 외곽선 6개 연결 PASS. 육안 확인 기록은 `Saved/BokuseiMotion/visual_review.json`이다.
- 새 프로세스 저장 검증 PASS: `Saved/BokuseiMotion/Runs/20261003T132402383352Z/validation.json`. 클립별 21개 시점·8개 사지 구간의 길이 변화는 약 1e-13cm 수준이었다.

이 검증은 FK 기반 모션 적용의 기준선이다. 무기 그립·발 고정 IK·의상/머리카락 물리·판정 및 Notify·게임플레이 연결은 포함하지 않는다.

## 2026-10-03 Bokusei 카타나 모션 60개 확장

사용자가 확인한 6개를 보존하고 같은 Anime Katana 팩의 나머지 **54개를 추가**했다. 최종 구성은 In_Place 40개와 Root_Motion 20개다. 방어/방어 피격, 차지 공격, 공중/하강/지상 콤보, 점프 공격, 대시/롤/돌진, 달리기 공격, 연속 베기·찌르기 등을 포함한다. 전체 파일 목록은 `Tools/Art/ToonTest/Bokusei/motion_library_manifest.json`을 따른다.

### 에셋과 확인 방법

| 용도 | 경로 |
|---|---|
| 제자리 40개 | `/Game/Art/ToonTest/Bokusei/Animation/PGBokusei_AS_Anime_KC_*` |
| 이동 변형 20개 | `/Game/Art/ToonTest/Bokusei/Animation/RootMotion` |
| 이동 변환 설정 | `Animation/LibrarySetup/RTG_PGAnimeKatana_Bokusei_RootMotion` |
| 확인용 맵 11개 | `/Game/Art/ToonTest/Maps/KatanaLibrary` |
| 원본 소스 클립 | `/Game/Art/AnimationTests/AnimeKatana/Animations` 및 `RootMotion` 하위 폴더 |

`L_PGBokusei_Katana_IP_01`부터 `IP_07`, `L_PGBokusei_Katana_RM_01`부터 `RM_04`를 열어 Play하면 각 페이지의 최대 6개 모션을 반복 재생한다. 총 본체 60개와 외곽선 60개가 11개 맵에 분산되어 있다. 마지막 IP 페이지는 4개, 마지막 RM 페이지는 2개다.

### 이동값 변환과 소스 의존성 복구

원본 RM 20개는 `SK_Mannequin`과 `root`가 정지해 있고 **pelvis에 이동값이 들어 있다**. 단순 root 복사로는 이동이 사라지므로 별도 Root Motion Op에서 `GenerateFromTargetPelvis`·`SnapToGround`를 사용한다. 골반의 XY 이동을 Bokusei 최상위 `Armature`로 옮기고 수직 움직임과 회전은 `Hips`에 보존한다. 루트 모션 추출을 켜고 잠금 기준을 첫 프레임으로 저장한다. 이는 수평 이동용 변환이며, 공중 스킬의 캡슐 수직 이동이나 회전 구동까지 연결한 것은 아니다.

초기 소스 메시가 참조하던 `SK_PGAnimeKatana_Skeleton` 파일 누락도 복구했다. 같은 원본 모델을 별도 `AnimationTests/AnimeKatana/LibrarySetup` 복구 경로로 읽어 Skeleton을 생성하고 기존 참조 경로에 저장했다. 기존 소스 메시·승인된 6개 클립·3개 리그·원래 모션 맵은 바이트 단위로 유지했다. 공용 FBX 임포트 함수는 자동 생성 Skeleton을 명시적으로 저장하도록 수정했다.

### 검증 결과

- 생성 **PASS**: `Saved/BokuseiMotionLibrary/20261003T135712771707Z/configure.json`. 60개 대상 Skeleton·길이·유한 좌표, 31개 포즈 및 21개 사지 길이 샘플 검사. 최대 사지 길이 변화 1.71e-13cm.
- 이동 변형 20개: 원본 pelvis XY 대비 배율 약 **0.915733**, 최대 궤적 오차 **0.0000765cm**. 생성된 루트의 시작점 대비 최대 이동은 클립별 약 **91.62–1028.58cm**다. 누적 이동거리가 아니다.
- SM6 렌더 및 PIE **PASS**: `Saved/BokuseiMotionLibrary/Preview/20261003T135959054244Z`에 1280×720 이미지 11장. 각 맵의 실제 반복 재생 시간 진행과 Leader Pose 연결을 검사했다. 캡처 소켓과 AnimPose의 최대 오차는 **0.006681cm**다.
- 새 프로세스 저장 검증 **PASS**: `Saved/BokuseiMotionLibrary/Runs/20261003T140317826845Z/validation.json`. 모션 60개, 맵 11개, 원본/대상 Skeleton 연결, 재생 플래그, 외곽선 연결, 루트 추출 설정과 저장 궤적을 검사했다. 저장 전후 루트 궤적 오차는 0cm다.
- 원본 FBX·승인된 기존 에셋의 SHA-256 보존 PASS. 육안으로 11개 쿼터뷰 샘플을 확인했다. 큰 점프/회전은 페이지 셀을 넘어 라벨과 겹치거나 화면 상단 여유가 좁아질 수 있다. 전체 프레임의 의상 간섭·접지·무기 그립 품질을 인증한 것은 아니다.

최초 렌더 시 맵 생성 중 Slate 콜백이 재진입해 실패한 실행은 `Runs/20261003T135820739090Z`에 보존했다. 재진입 방지 후 완료된 위 최종 실행만 렌더/PIE 근거로 사용한다.

### 재실행

```powershell
$pythonExe = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
& $pythonExe Tools/Validation/RunBokuseiMotionLibrary.py
# 단계별 재실행
& $pythonExe Tools/Validation/RunBokuseiMotionLibrary.py --step configure
& $pythonExe Tools/Validation/RunBokuseiMotionLibrary.py --step preview
& $pythonExe Tools/Validation/RunBokuseiMotionLibrary.py --step validate
```

실행 중인 Unreal 프로세스와 겹치면 중지한다. 다른 검증의 정상 종료를 기다릴 때만 `--wait-editor-seconds 600`을 사용할 수 있다. 원본 이동 구조를 읽기 전용으로 확인하는 단계는 `--step inspect`다. 반복 생성 시 추가 에셋과 생성 맵은 Saved의 해당 실행 폴더에 백업한다. 전투 Montage/GAS·판정/Notify 연결과 접지·무기·보조 물리 폴리싱은 후속 작업이다.

## 2026-10-04 Bokusei 추가 3팩

사용자가 지정한 Frank Slash, Grruzam Powerful Sword, RPG Animations를 원본 인체 리그에서 Bokusei로 직접 변환한다. ElfSelena는 이 작업의 소스·중간·대상에 포함하지 않는다. 기존 카타나 60개와 Bokusei 기본 에셋은 SHA-256으로 보호한다.

| Unity 원본 폴더 | 작성된 인체 클립 | Bokusei 결과 폴더 |
|---|---:|---|
| `Frank_Slash_Pack` | 1,291 | `Animation/FrankSlash` |
| `Grruzam Powerful Sword Animation(Great Sword, Katana)` | 959 | `Animation/GrruzamSword` |
| `RPG_Animations_Pack` | 6,336 | `Animation/RPGAnimations` |
| 합계 | 8,586 | `/Game/Art/ToonTest/Bokusei` 하위 |

원본 FBX 8,326개 중 모델·소품 29개와 인체 골반이 없는 무기 전용 애니메이션 1개는 모션 변환 대상에서 제외한다. 하나의 FBX에 Unity 클립 구간이 여러 개 지정된 Sword2·Whip 등이 있어 출력 클립 수가 FBX 수보다 많다. `three_packs_manifest.json`은 FBX·Unity 메타 해시, 구간·Take 이름, 소스/대상 경로와 제외 사유를 보존한다. 일반적인 `Take 001` 표시명은 FBX 파일명으로 바꾸되 원래 Unity 이름은 별도로 기록한다.

### 변환 구성

- Frank 10종, Grruzam 1종, RPG 1종의 소스 메시·IK Rig·Retargeter는 `/Game/Art/AnimationTests/<팩>/Setup`에 있다. 기존 Bokusei 21개 체인에 pelvis·FK를 직접 연결한다.
- 원본 구간을 30fps로 임포트하고 결과는 팩/종류/`InPlace`·`RootMotion`·`Authored` 폴더로 분리한다. `Authored`는 원본 폴더에 이동 방식 표시가 없다는 뜻이다. 실제 루트 이동/회전이 있으면 Root Motion Op로 Bokusei `Armature`에 복사하고 루트 추출을 켠다. 명시된 RM 클립에서 root가 정지하고 pelvis만 이동하면 pelvis XY를 사용한다.
- Unity 구간의 끝점이 소수 프레임인 8개 클립은 Unreal FBX 임포터의 정수 구간에 맞춰 가장 가까운 원본 프레임으로 반올림하고 30fps 경계에 맞춘다. 명세에는 반올림 전 값을 보존한다.
- Unreal AssetTools가 이름 끝의 큰 숫자를 자산 번호로 해석하고 INT32_MAX로 제한하는 충돌을 피하도록 숫자로만 된 식별자의 끝에 `h`를 붙인다. 파일/클립의 영구 ID는 유지한다. 체크포인트가 완료된 애셋은 해시가 일치할 때 재사용한다.
- `Frank_RPG_Warrior_Unequip.FBX`의 `Frank_RPG_Warrior_Putup` 클립은 Unity 메타에 5940~6000 프레임이 남아 있지만, 제공된 FBX Take와 키는 60fps의 0~60 프레임(1초)이다. 이 파일에 한해 실제 FBX 구간을 사용한다. 명세에 보정 사유와 원래 구간을 함께 저장하고 입력 해시가 달라지면 재검토하도록 제한했다. 근거는 `Saved/BokuseiPacks/warrior_range_diagnostic.json`이다.
- Frank Whip의 원본 FBX 11개에는 `Sword_Blade10`~`Sword_Blade14` 회전 곡선의 비유한 값이 있다. Unreal은 해당 값을 기본값으로 보정하며 진단을 남긴다. 이 무기 본들은 인체 리타게팅 체인에 포함되지 않는다. 원본을 수정하지 않고 모든 Whip 결과의 302개 본을 추가 검사한다. 상세 원본 진단은 `Saved/BokuseiPacks/whip_fbx_diagnostics.json`이다.
- 대표 모션 확인용 맵은 `/Game/Art/ToonTest/Maps/PackLibrary`에 생성한다. 각 종류의 최대 6개 클립을 보여 주며 전체 8,586개를 전부 전시하는 맵은 아니다.

### 검증 기록

- 전체 생성 **PASS**: `Saved/BokuseiPacks/Runs/20261003T160539150628Z/configure.json`. 8,586개 ID를 빠짐없이 변환했다. Body 7,380개, 소스 루트 복사 1,170개, pelvis XY 추출 36개이며 의도된 정지 포즈 29개를 포함한다. 9개 시점의 주요 본·8개 사지 구간 검사에서 최대 길이 변화는 **0.000002624cm**, 원본 대비 루트 궤적 최대 오차는 **0.00003868cm**였다.
- 새 프로세스 전체 저장 검증 **PASS**: `Saved/BokuseiPacks/Runs/20261003T170241291361Z/validation.json`. 전체 클립의 Skeleton·길이·유한 포즈·루트 설정과 26개 맵의 참조·반복 재생·Leader Pose를 검사했다. 저장 전후 루트 궤적 오차는 **0cm**다. Whip 114개는 각각 31개 시점에서 Bokusei의 302개 본 위치·회전·스케일까지 모두 유한함을 확인했다.
- SM6 대표 렌더 및 PIE 검사: 26종×6개, **156개 모션**. 첫 전체 실행 `Runs/20261003T163418345793Z`는 정상 종료했다. 이후 화면 배치를 보완한 이미지 26장은 `Saved/BokuseiPacks/Preview/20261003T171006928221Z`에 있다. 실제 소켓과 AnimPose 최대 오차는 **0.010608cm**, 26개 맵 모두 재생 시간 진행과 외곽선 연결 PASS다.
- 정지 캡처는 골반 XY를 셀 중앙으로 옮기고 최저 주요 관절 높이를 맞춘 **표시 위치**를 사용한다. 포즈 회전·스케일 및 애니메이션 데이터를 변경하지 않으며, PIE 전 원래 Actor 위치를 복구한다. 저장 맵과 실제 재생에서는 원본 이동을 유지하므로 일부 Authored 이동/점프가 셀이나 카메라 범위를 벗어날 수 있다. 정지 이미지는 실제 이동 거리·점프 높이·접지 품질의 근거가 아니다.
- 보완 렌더 실행 `Runs/20261003T170949693523Z`는 26개 캡처/PIE 보고서를 모두 저장하고 로그에 `Exiting`과 파일 닫힘까지 기록한 뒤 종료 코드 **0xC0000005**를 반환했다. 원인은 확정하지 않았으며 실행기 결과는 FAIL로 보존했다. 캡처 완료와 프로세스 정상 종료를 구분한다.
- 최종 맵·파일 무결성 재검증 **PASS**: `Saved/BokuseiPacks/Runs/20261003T172104955400Z/validation_maps.json`. 보완 후 저장된 맵 26개를 다시 열었고, 이미 전체 포즈 검증을 통과한 애니메이션 8,586개의 SHA-256이 동일함을 확인했다. Unity FBX·메타 및 보호한 기존 Bokusei·카타나 에셋도 불변이다.
- 최종 26개 이미지를 육안으로 확인했다. 기록은 해당 Preview 폴더의 `visual_review.json`이다. 전체 클립의 전 프레임 의상 간섭·무기 그립·접지를 인증한 것은 아니다. C++/GAS 변경이 없어 C++ 재빌드는 수행하지 않았다.

### 재실행

```powershell
$pythonExe = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
& $pythonExe Tools/Validation/AuditBokuseiPacks.py
& $pythonExe Tools/Validation/BuildBokuseiPacksManifest.py
& $pythonExe Tools/Validation/RunBokuseiPacks.py
# 완료된 단계가 있으면 configure / preview / validate만 개별 실행 가능
# 전체 검증 후 갤러리만 바뀐 경우: 모든 애니메이션 해시와 맵을 검사
& $pythonExe Tools/Validation/RunBokuseiPacks.py --step validate --maps-only
```

순차 실행이 기본이며 `--pilot`은 대표 60개 변환 확인용이다. 전체 검증은 반드시 명세의 8,586개 ID와 일치해야 통과한다. 체크포인트와 단계별 결과는 `Saved/BokuseiPacks`에 저장한다. 이 작업은 애니메이션 라이브러리와 Bokusei 적용/재생 확인이며, 무기 리그·그립·접지 IK·의상 물리, 전투 Montage/GAS·판정/Notify 연결은 포함하지 않는다.
