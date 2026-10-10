# 헤어 툰 셰이딩 부드러움 개선

## 구현

- `ToonShading.hlsl`: 헤어에 한해 반사 로브를 연속적으로 유지하고, 밝은 밴드로 반사를 다시 잘라내는 정도를 줄인다. 반사색을 머리색과 혼합해 검은 머리에 회백색 띠가 뜨는 현상을 완화한다. `HairSoftness=0`은 기존 반사 계산이다.
- `ConfigureToonCharacterTest.py`: 머리 본 중심과 머리 축으로 만든 타원체 노멀을 원래 노멀과 혼합한다. 툰 명암과 엔진 수광에 적용하며, 엔진의 양면 노멀 반전을 별도 출력에서 보정한다. 유효한 머리 본이 없거나 `HairNormalBlend=0`이면 원래 노멀로 동작한다.
- `PGToonPresentationComponent`: 해당 헤어 재질을 머리 축 갱신 대상에 포함하고, 회전이 그대로여도 머리 중심의 이동을 전달한다. 기존 원거리 15Hz 정책을 따른다.
- `shading_profiles.json`: 헤어 명암 경계 0.12/0.18, 반사 강도 0.032, 반사 지수 18, 림 강도 0.035, 볼륨 노멀 혼합 0.65, 머리 중심 오프셋 6cm를 사용한다. 얼굴·의상의 신규 파라미터 기본값은 0이다.
- 헤어 월드 조명 비중은 `profiles.hair.world_lighting_influence=0.25`다. 머리카락의 실그림자를 부분적으로 유지하면서 강한 카드 그림자 대비를 줄인다. 기존 0.65 후보보다 정면·측광에서 덩어리 내부의 명암이 부드러워 최종 선택했다. LightingLab·캐릭터 배치·P09·Bokusei 비교 생성 코드도 이 값을 따른다.
- 기존 헤어 인스턴스 54개에 `ConfigureHairSoftness.py`로 이관했다. 비교 단계의 의도적인 반사·림 0을 보존하고 기본 셀 단계는 기존 cloth 명암을 유지한다. 메시·텍스처 원본과 맵은 수정하지 않는다.

## 실행과 백업

프로젝트 루트에서 다음 순서로 실행한다. 각 `--run`에는 아직 사용하지 않은 디렉터리를 지정한다.

```powershell
$env:PG_TOON_SURFACE_ONLY='1'
python Tools/Validation/RunToonImprovement.py --step apply --run Saved/HairSoftness/new_masters
python Tools/Validation/RunToonImprovement.py --step apply --hair-softness --run Saved/HairSoftness/new_instances
$env:PG_HAIR_SOFTNESS_PREVIEW='1'
python Tools/Validation/RunToonImprovement.py --step render --run Saved/HairSoftness/new_render
```

- 최초 변경 전 에셋 백업: `Saved/HairSoftness/masters/backup`, `Saved/HairSoftness/instances/backup`.
- 최종 양면 보정 마스터 저장·재로드: `Saved/HairSoftness/two_sided_retry`.
- 볼륨 노멀 파라미터 저장·재로드: `Saved/HairSoftness/volume_instances`.
- 최종 조명 비중을 포함한 인스턴스 저장·재로드: `Saved/HairSoftness/final_instances`.
- 근접 전후 비교는 `PG_HAIR_CLOSE_COMPARE=1`, 근접만 촬영할 때는 `PG_HAIR_CLOSE_ONLY=1`을 함께 설정한다. 전후 비교의 원래 값은 최초 `Saved/HairSoftness/instances/hair.json`을 사용한다.
- 다른 Unreal 프리뷰가 같은 에셋을 로드한 상태에서는 파일 잠금으로 저장과 롤백이 모두 실패할 수 있다. 이번 실행의 `volume_masters`, `two_sided_masters`는 해당 실패 기록이며, 이후 별도 실행에서 표면 마스터 전체를 재적용·재로드했다. 실패 디렉터리는 성공 근거로 사용하지 않는다.

## 검증 범위

- 최종 완료 근거는 `Saved/HairSoftness/final_saved/run.json`의 PASS다. 저장된 최종 재질로 12장(툰/월드 조명, 광원 방위 0/60/120°, 원거리 화면 비율 50/100%, 근접 원래 값/개선 값)을 촬영하고, 머리 중심 이동 추적·SM6 셰이더 컴파일·프로세스 정상 종료를 확인했다. 정면·측광 근접 및 원거리·역광 이미지를 직접 검수했다.
- UE 5.8 Development Editor 빌드 성공. 실제 PIE에서 신규 머리 중심·유효 프레임 파라미터 전달 확인.
- 최종 표면 마스터 17개 저장·새 프로세스 재로드 통과. 인스턴스/메시 보존 해시 검사 포함.
- 헤어 인스턴스 54개 저장·새 프로세스 파라미터 재조회 통과.
- `PG.Rendering.Toon` 2개 통과(1개 경고 포함): `Saved/HairSoftness/automation`.
- `fill_compare`: 동일 카메라·포즈에서 정면/측광 × 원래 값/노멀 보정/조명 비중 감소의 6장, 머리 중심 17cm 이동 추적, SM6 셰이더 컴파일·정상 종료 통과. 조명 비중 감소 후보를 채택했다.
- `final_close`는 촬영·이동 검사 결과가 PASS였지만 에디터 종료 후 프로세스 반환 코드가 0xC0000005여서 실행 전체는 FAIL로 기록했다. 로그에 종료 전 셰이더 실패는 없었고, 후속 `fill_compare`는 정상 종료했다. 해당 실패를 전체 통과로 간주하지 않는다.
- `final_isolated`는 양면 보정 전 후보의 렌더 기록이다. `volume_after`는 다른 전시 모델에 가려 시각 검수 근거에서 제외한다. `two_sided_verified`의 이동 검사는 근접 카메라 전환 직후 원거리 갱신 주기에서 실행되어 실패했으며, 최종 검사는 카메라 전환 후 대기하도록 수정했다.
- 모델 자체의 각진 실루엣, 헤어 카드 겹침/투명 정렬, 텍스처에 그려진 줄무늬는 셰이딩만으로 모두 제거되지 않는다. 다른 외형 전체의 근접 미술 수용과 장시간 전투 성능은 별도 범위다.
