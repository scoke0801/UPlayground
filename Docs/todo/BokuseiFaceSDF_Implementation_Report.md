# Bokusei 모델 기반 얼굴 SDF 적용 · 2026-10-06

## 적용 결과와 확인 방법

실제 `DA_Bokusei`가 사용하는 `SK_Bokusei_ToonTest`의 얼굴 슬롯에 전용 SDF 명암을 적용했다. `MI_PGToon_Bokusei_Mat_Bokusei_Face`의 부모를 `M_PGBokuseiFaceSDF`로 교체하고 텍스처·조절값을 저장했다. 게임에서 Bokusei를 선택하면 같은 얼굴 재질을 사용한다. 외형 DataAsset, 메시, 나머지 9개 슬롯은 적용 전 SHA-256과 일치한다. 기존 텍스처의 눈·입·머리카락 표현을 유지한다.

`/Game/Art/ToonTest/Maps/L_PGToon_Bokusei_ShadingComparison`을 열고 재생한다.

- `5 → F`: 적용 전 얼굴. 원본 얼굴 MI를 복제한 Baseline을 사용한다.
- `7 → F`: 실제 적용한 Unlit SDF 얼굴과 같은 설정.
- `8 → F`: 비교 전용 Default Lit SDF + 월드 수광·헤어 그림자.
- `C`: 쿼터뷰. `0/R`: 전체 보기. `J`: 6/8단계 헤어 그림자. `H`: 가림막 그림자.

좌우 조명 비교 시트는 `Saved/BokuseiFaceSDF/Comparison.png`다. 주광원 방향을 바꾸면 PIE에서 머리 본 방향과 함께 명암 경계가 갱신된다. 8단계의 물리적 헤어 그림자 변형은 게임 얼굴에 적용하지 않았다.

## 제작 기준

UE 5.8.2의 실제 imported LOD0에서 `Mat_Bokusei_Face` 6,671개 정점·9,760개 삼각형의 위치·노멀·UV0를 추출했다. `Head` 본 기준 좌표와 `DA_Bokusei`의 HeadForwardAxis/HeadRightAxis를 사용한다. 별도 얼굴 이미지나 다른 모델의 범용 마스크를 투영하지 않는다.

512×512 UV 래스터의 가장 큰 피부 섬은 124,046 texel이며 좌우 충돌은 0이다. 전체 얼굴 재질에는 눈·입·귀 주변의 다른 UV 섬 및 중첩이 있으므로 피부 섬만 SDF 영향 대상으로 삼는다. 뒤쪽과 아래쪽 경계는 기존 기하 명암을 사용한다.

실제 얼굴 폭 기반 방향 78%와 노멀 방향 22%를 혼합해 코 주변의 작은 노멀 변화가 얼굴을 잘게 나누지 않도록 했다. 각 측면의 0–180° 조명을 19단계로 샘플링하고, 이진 명암 마스크마다 정확한 2D Euclidean signed distance를 구한다. 인접 각도의 거리장 영점 사이를 보간해 조명 전환 임계값을 저장한다. 텍스처 패딩은 12 texel이다.

최종 텍스처는 **SDF로 생성한 각도 임계값 맵**이다. 런타임 3D 거리장을 추적하는 방식은 아니다. UE Mesh Distance Fields와 별개의 얼굴 전용 기법이다.

| 채널 | 데이터 |
|---|---|
| R | +HeadRight 측 광원에 대한 전환 각도 / π |
| G | -HeadRight 측 광원에 대한 전환 각도 / π |
| B | 앞쪽 피부의 SDF 영향도 |

`T_PGBokusei_FaceSDF`는 sRGB 꺼짐, 손실 압축 없는 `TC_VECTOR_DISPLACEMENTMAP`, 평균 mip, Clamp 샘플링이다. 셰이더는 머리 평면의 광원 각도와 R/G 임계값을 비교하며 fwidth 기반 경계 보정을 적용한다. 머리 평면에서 광원 투영 길이가 거의 0인 천정·바닥 방향은 기존 명암으로 부드럽게 돌아간다.

## 파일과 조절값

- 제작 원본: `Tools/Art/ToonTest/BokuseiFaceSDF`의 baker, `settings.json`, `FaceSDF.hlsl`, PNG, `bake.json`.
- 추출: `PGEditorProbeTools::ExportSkeletalMaterialGeometry`, `InspectBokuseiFaceSDF.py`. FBX export의 엔진 재질 베이크 assertion을 우회하는 읽기 전용 편집기 도구다.
- 에셋: `/Game/Art/ToonTest/BokuseiFaceSDF`의 텍스처 1개, 마스터 2개, Baseline/SDF/SDFWorld MI 3개.
- 적용: `ConfigureBokuseiFaceSDF.py`. shared master builder의 선택적인 `face_sdf_texture` 분기를 사용한다. 기존 마스터 생성 호출은 유지한다.
- 런타임: `PGToonPresentationComponent`는 FaceShading 또는 FaceSDFEnabled가 활성화된 MID에 HeadForwardWS/HeadRightWS를 전달한다. PostPhysics의 기존 캐시·본 갱신 경로를 사용하며 매 프레임 MID 생성·월드 광원 탐색은 없다.

| 파라미터 | 적용값 | 조절 대상 |
|---|---:|---|
| FaceSDFEnabled | 1 | 0이면 기존 기하 명암 사용 |
| FaceSDFStrength | 0.85 | 얼굴 그림자 강도 |
| FaceSDFBias | 0.035 | 그림자가 시작되는 광원 각도 |
| FaceSDFSoftness | 0.012 | 명암 경계 부드러움 |
| FaceSDFDebug | 0 | 1: 맵 RGB, 2: 명암 마스크 |

설정 원본은 `settings.json`이다. 게임 얼굴 MI에서도 수치를 조절할 수 있으며 재생성하면 settings 값으로 갱신한다.

## 재현·백업

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunBokuseiFaceSDF.py --apply
```

모델 추출 → Blender 5.2 번들 numpy Python으로 베이크 → UE 에셋 생성·얼굴 적용 → 8단계 비교 맵 생성 → 별도 SM6 렌더/PIE 검사 순서다. `--step inspect|bake|configure|preview`로 개별 실행한다. `--step configure --apply`는 베이크 결과를 적용하고 비교 맵을 갱신한다. `--step preview --input-only`는 각도 캡처를 반복하지 않고 재로드·애니메이션·입력만 검사한다. `--apply` 없이 생성하면 실제 얼굴 MI는 유지한다.

매 생성 실행의 `Saved/BokuseiFaceSDF/Runs/<시각>/backup/original_face.uasset`은 원래 경로의 얼굴 패키지 백업이다. SDF 후보 폴더도 함께 보관한다. 적용 프로세스가 실패하면 runner가 해당 얼굴 패키지를 복구한다. 수동 복원은 에디터 종료 후 실제 적용 전 실행의 `original_face.uasset`을 원래 얼굴 MI 경로로 복사한다. Baseline MI의 패키지를 원래 파일명으로 바꾸는 방식은 사용하지 않는다.

원본 Bokusei 임포트/재질 재생성 작업이 얼굴 부모를 초기화하면 이 적용 명령을 다시 실행한다. Content는 별도 저장소이므로 얼굴 패키지·새 SDF 폴더·비교 맵을 소스 변경과 함께 관리한다.

## 검증 근거

- UE 5.8 Editor Development 빌드 PASS: imported geometry 추출과 FaceSDF MID 등록, 비교 7/8 키 경로를 포함한다.
- 베이크 PASS: 피부 UV 중첩 0, 알려진 거리장의 exact distance, 유한 값/0–1 범위, 정면/후면 전환 끝점 검사. 설정 파일로 다시 베이크했을 때 PNG SHA-256 `a16968941041d9be6d4dc232aa15faf1fd32e6c9d3fa1829ed97fce999deafb9` 재현.
- 실제 적용·8단계 재생성 PASS: `Saved/BokuseiFaceSDF/Runs/20261006T133627736828Z/run.json`. 원본 DA/메시/9개 다른 MI 해시 보존.
- SM6 캡처 19개: `Saved/BokuseiShadingComparison/Preview/20261006T132611484588Z`. 기존/SDF 쌍의 -60/0/+60/180° 조명, 천정/바닥 광원, 얼굴·쿼터뷰를 시각 확인. 이 실행은 캡처 후 PIE의 오차 기준에서 실패한 기록을 보존한다.
- 최종 새 프로세스 재로드·PIE PASS: `Saved/BokuseiFaceSDF/Runs/20261006T133721053186Z/run.json`, `Saved/BokuseiShadingComparison/Preview/20261006T133741539858Z/preview.json`. 실제 얼굴의 부모·SDF 텍스처·5개 파라미터 저장값 확인, 8개 모델·35개 본 비교 최대 오차 약 `5.68e-14 cm`, 애니메이션 중 SDF 머리 방향, 자유 카메라/그림자 토글/7·8 얼굴·쿼터뷰 입력 33개 PASS.
- 초기 검증은 캐시의 성분별 오차 0.0001을 벡터 거리 0.0001로 검사해 불필요하게 실패했다. 런타임과 같은 성분별 기준으로 수정했다. 전체 보기 직후 7단계를 얼굴 시점으로 잘못 기대하던 테스트도 정면 선택→F 순서로 수정했다.
- 첫 재적용의 텍스처 import-save/명시적 save 연속 저장은 Windows 파일 핸들 충돌로 종료 코드 1을 반환했다. 얼굴 백업 복구를 확인했고, import는 메모리에서만 수행한 후 숫자 텍스처 설정을 마치고 한 번 저장하도록 수정했다. 다음 적용·재로드는 PASS.
- 실제 RogueArena Bokusei의 GAS 공격 8종·회피·장비 해제/재장착 11개 모션 회귀 PASS: `Saved/PlayableCharacters/Runs/20261006T133911550317Z_grip-preview/result.json`. 898개 표본에서 무기 상대 위치 변화 0, 상대 각도 최대 약 0.000004°. 이 도구의 카메라는 손 근접용이므로 얼굴 렌더 수용 근거는 위 별도의 SM6 얼굴/쿼터뷰 캡처다.
- PG 자동 테스트 50개 PASS: `Saved/PlayableCharacters/Runs/20261006T133831_automation`. 41개 Success, 9개 SuccessWithWarnings, 실패 0.

## 남은 폴리싱

얼굴 폭과 실제 노멀에서 만든 초기 모델 전용 결과다. 코 그림자의 수작업 스타일 가이드나 표정별 별도 맵은 추가하지 않았다. 8단계의 헤어 그림자는 기존 물리적 투사체 방식이라 일부 각도의 카드 경계가 남으며 별도 폴리싱 대상이다. 다중 광원·포인트라이트별 얼굴 SDF, GPU 비용 측정과 패키지 장시간 플레이는 이번 확인 범위에 포함하지 않는다.
