# UPlayground 프로젝트 분석 문서

## 2026-10-10 모션 확장 AI 선택 진단

- `pg.AI.DebugDecisions 1`로 스킬 후보 탈락·선택 우선순위·선택 유지·공격권 대기·GAS 실행·이동 요청 실패를 기록한다. `pg.AI.DebugEnemyID`로 적 ID를 필터링하며 기본 출력은 꺼짐이다.
- 기존 선택 정책·전투 수치와 Art 후보/보스 사용 확정 목록은 유지한다. `RunCombatBT.py --debug-decisions`가 기존 6역할 검증과 진단 근거 수집을 연결한다. 범위·한계·검증 결과는 [AI 개선 기록](Docs/todo/MotionExpansion_AI_Implementation_Report.md)을 따른다.

## 2026-10-10 Dark Knight 보스와 Art 모션 목록

- Dark Knight 본체와 오른손 전용 검에 BossyEnemy 모션 11개 및 모델 사망 모션을 연결한 보스 15601을 추가했다. 공격 4종과 HP 50%의 2페이즈를 기존 GAS/역할 AI로 실행한다. 6스테이지 자동 스폰은 Dark Knight 1마리이며 같은 편성을 절차 생성 던전 보스방도 사용한다.
- `Tools/PlayDarkKnightBoss.ps1` 또는 PIE의 `PGBossEncounter 15601`로 실행한다. 에셋 경로, 검증 근거 및 한계는 [보스 제작 기록](Docs/todo/DarkKnightBoss_Implementation_Report.md)을 따른다.
- Art 전체 애니메이션 관련 17,376개를 [사용 후보 목록](Docs/todo/ArtAnimation_UsageCatalog.md)과 CSV에 수집했다. 직접 참조 없음은 미사용 확정이나 삭제 허가를 뜻하지 않는다.

## 2026-10-10 절차 생성 던전 P2 환경·탐험 통합

- 보물방에 기존 저장 트랜잭션과 고유 키를 사용하는 장비 보상 1개씩을 추가했다. 기본 104마리·16웨이브·필수 강화 7회는 유지한다. 미니맵에 발견한 보물·획득 상태·카메라 방향과 범례를 연결했다.
- 제작 방 데이터 8종과 실제 PCG 환경 그래프를 적용한다. PCG 완료·출력 검증 후 이동 경로를 검사한다. 실행 객체는 저장·PIE 복제에서 제외하고 취소·재생성 시 정리한다. 외부 원본은 유지하며 던전 사본에만 환경 가림 재질을 적용한다.
- 보스 문은 금빛 봉인·승강·사운드로 표현한다. 경계벽·장식은 두 카메라에서 플레이어·바닥을 가릴 때 인스턴스별로 디더링하며 충돌은 유지한다.
- P2 최종 빌드·자동 테스트 60개·100시드 경로·3시드 완주·복구·렌더 검사를 통과했다. 720p/1080p 화면 11장을 확인했고 최종 렌더는 종료 코드 0이다. 기존 종료 접근 위반의 원인 해결을 확정한 것은 아니며 전체 수용·저장 복원·패키지 성능·직접 플레이는 P3에 남는다.
- 도구는 `Tools/Validation/RunDungeonExploration.py`와 `Tools/Art/ProceduralDungeon/ConfigureDungeonP2.py`다. 검증 근거와 P3 범위는 [던전 구현 기록](Docs/todo/ProceduralDungeon_Implementation_Report.md)의 최신 P2 항목을 따른다. 아래 항목은 이전 이력이다.

## 2026-10-10 절차 생성 던전 P2 발견형 미니맵

- 방문한 방·확인한 연결·입구·발견한 목표와 보스·플레이어 위치를 좌측 상단 지도에 표시한다. 미탐험 출입구는 방향만 표시하며 미발견 방의 역할·보상을 노출하지 않는다.
- `PGDungeonDiscoverySubsystem`이 월드별 발견 상태와 제한된 UI 스냅샷을 소유한다. 생성기에서 준비 완료·위치·목표를 전달하고 PGUI가 변경 이벤트를 구독한다. 전투·탐험 모드에서 동작하며 취소·재생성 시 초기화한다. 저장 복원은 아직 지원하지 않는다.
- 검증 도구는 `Tools/Validation/RunDungeonExploration.py --step <build|automation|runtime|render>`다. 검증 근거와 남은 P2/P3 범위는 [던전 구현 기록](Docs/todo/ProceduralDungeon_Implementation_Report.md)의 P2 첫 단계 항목을 따른다.

## 2026-10-10 절차 생성 던전 P1 전투 연결

- 실제 이동 검증 후 StageManager에 목표방 5개와 보스방을 전달한다. 방 진입으로 기존 16웨이브·적 104마리·강화 7회를 실행하고 보스 진행 게이트·봉쇄·실패 시 해제를 연결했다. 일반방은 개방 상태로 유지한다.
- `PGStageManagerDungeon.cpp`가 공간 진입과 도달 불가 감시를 맡고 기존 GAS 전투·강화·드랍 저장을 재사용한다. 생성기는 구조와 문을 소유한다. 스폰 실패·비정상 적 소멸·사망·재생성을 처리하고 정상 소환체 정리를 보존한다.
- `Tools/PlayProceduralDungeon.ps1 -Seed 101026`은 격리 전투 프로필의 새 런을 시작한다. `-Preview`는 이전 공간 검증 모드다. 던전 시드에서 전투·진행 난수를 분리하며 체크포인트 복원은 아직 지원하지 않는다.
- 검증은 `Tools/Validation/RunDungeonCombat.py --render`다. 결과와 P1/P2/P3 잔여 범위는 [던전 구현 기록](Docs/todo/ProceduralDungeon_Implementation_Report.md)의 P1 항목을 따른다. 아래 P0 항목은 이전 단계의 기록이다.
- 최종 빌드·PG 자동 테스트 59개·3시드 자동 완주(16웨이브/104마리/강화 7회)·실패 복구·P0 실제 경로 100시드는 통과했다. 한국어 화면 3장은 확인했으나 렌더 종료 후 `0xC0000005`가 재현되어 전체 검증은 FAIL로 유지한다. 직접 조작 완주와 저장 복원·P2 환경/탐험 통합은 남아 있다.

## 2026-10-10 ExternalAssets 공격 VFX 연결

- 플레이어 공격·스킬 8종의 접점과 이동 검기에 `MixedVFX` 베기, `SlashTrail_SoftTofu` 순간 섬광, `Niagara/GroundRocks` 파편을 연결한다. 프로젝트 조정본은 `/Game/Art/PlayerCombatFX/External`이며 원본은 유지한다.
- `PGPlayerSkillProfile.ExternalVFX`가 접점 형상별 시스템·변환·재생 시간을 소유한다. 기존 형상·빌드별 머티리얼을 함께 사용하며 피해 판정·모션 시각을 보존한다. 논리 시계·히트스톱·취소·검기 수명과 풀 반환 경로에 통합한다.
- `Tools/Art/PlayerCombatVFX/RunExternalVFX.py`로 검사·빌드·적용·재로드·렌더를 재현한다. 백업, 에셋 선정, 검증 결과와 미검증 범위는 [VFX 구현 기록](Docs/todo/PlayerCombatVFX_Implementation_Report.md)을 따른다.

## 2026-10-10 절차 생성 던전 P0 기반

- 별도 `/Game/Maps/L_PG_ProceduralDungeon`에서 시드에 따라 8~12개 방과 통로를 생성한다. 입구·필수 목표 5개·보스 역할과 깊이 1~2의 선택 분기를 연결한다. 현재는 탐험 검증용이며 전투·보상 진행은 연결하지 않았다.
- PGShared의 레이아웃 값 타입, PGData의 정의·유한 구조 생성기, UPlayground의 공간 조립·준비 게이트로 책임을 분리한다. 최대 3회 생성과 기본 배치, 요청 취소, 완전 내비게이션 경로 및 실제 캐릭터 캡슐 검사를 제공한다.
- 기존 숲 폐성소의 조명·석재·이끼·나무 스타일을 재사용하고 `ExternalAssets/LevelDesign/RuinedCrypt` 소품을 던전 전용 사본에 연결한다. 기존 고정 맵과 외부 원본은 보존한다. PCG·제작 방 변형·미니맵은 후속 범위다.
- 실행은 `Tools/PlayProceduralDungeon.ps1 -Seed 101026`, 재현 검증은 `Tools/Validation/RunProceduralDungeon.py --build-map --runtime --render`다. UE 5.8 에디터 빌드·1,000개 구조 시드·100개 실제 월드 경로와 재생성·취소 후 복구 검사를 통과했다. 상세 근거와 직접 플레이·전투·저장 통합의 잔여 범위는 [구현 기록](Docs/todo/ProceduralDungeon_Implementation_Report.md)을 따른다.
- 최종 화면 3장은 확인했으나 렌더 에디터가 종료 로그 이후 `0xC0000005`로 반환해 전체 검증은 FAIL로 남긴다. 캡처 완료와 정상 종료 통과를 구분한다.

## 2026-10-10 PCG 전 폐성소 환경 구성

- 현재 기본 테스트 맵 `/Game/Maps/L_PG_ForestRuins`에 `ExternalAssets/LevelDesign/RuinedCrypt`의 예배당·기둥·묘비·석재 잔해와 `Dungeon_Pack`의 바닥 텍스처·봉헌 소품을 적용한다. 석재 광장과 흙·이끼가 연결되는 참배길, 외곽의 지붕 없는 예배당과 묘역을 구성했다.
- 지면 전용 재질은 `/Game/Environment/ForestRuins/Sanctuary/M_PG_SanctuaryGround`다. 기존 지면·경계·전투 장애물의 충돌, PlayerStart와 내비게이션 액터 Transform을 유지하고 장식은 비충돌로 배치한다. 예배당·보관함은 환경 표현이며 입장·보상 기능은 없다.
- 재현은 `Tools/Art/ForestRuins/RunLevelDressing.py`에 `InspectLevelDressing.py`, `DressForestSanctuary.py`, `PreviewLevelDressing.py --render`를 차례로 전달한다. 실행마다 기존 맵을 `Saved/LevelDressing/Backups`에 보관하며, 전용 태그의 장식만 교체한다. 과거 `BuildForestRuins.py`로 기본 숲을 다시 만들었다면 환경 구성을 재적용해야 한다.
- 요청에 따라 빌드·패키징·전투 회귀 테스트는 실행하지 않는다. 검수 범위는 저장된 맵의 에디터 렌더와 쿼터뷰 화면이며, PCG 및 던전 진행 로직은 아직 구현하지 않았다. 배치 명세와 캡처 위치는 `Saved/LevelDressing/dressing.json`, `latest_preview.json`에서 확인한다.
- 최종 맵을 새 에디터 프로세스에서 로드해 5개 시점의 캡처를 확인했다. 맵 저장 프로세스는 정상 종료했으나 최종 렌더 프리뷰 2회는 캡처와 종료 로그 기록 후 `0xC0000005`로 반환됐다. 카메라 참조 정리 후에도 재현되어 원인은 미확정이며, 정상 종료 검증 통과로 보지 않는다.

## 2026-10-10 그리핀·식물 전투 AI와 스폰 연결

- 몬스터 15501 그리핀(공중형), 15502 식물 본체·15503 뿌리(고정형), 15504 이동형 식물(지상형)을 `/Game/DataCenter/CreatureCombat`에 등록했다. 기존 역할 BT·GAS와 전용 메시·모션·스탯·피격·사망을 연결한다. 기존 검성 15401은 유지한다.
- `PGEnemyDataRow.Mobility`가 지상·저공 비행·고정 이동을 구분한다. 본체의 15523 소환 스킬은 예고 후 이동형 2마리, 쿨다운 12초, 생존 상한 4마리를 사용한다. NavMesh·충돌을 검사하고 기존 스테이지 스폰 메시지·집계 정책에 연결하며 소환체 드랍과 본체 제거 후 잔존을 차단한다.
- 기존 웨이브 총수·보상·보스를 유지하며 1~5구간 일부 적을 신규 타입으로 교체했다. 이동형 15504는 일반 웨이브에도 독립 등록된다. 원본은 `Tools/Validation/Data/CreatureCombat.json`, 백업·적용·재로드·실전 렌더 검사는 `RunCreatureCombat.py --apply --render`다.
- Development 빌드·PG 자동 테스트 56개·공통 에셋 검사 schema 5·9스킬 실행·공중/고정/지상 이동·소환 상한/취소/정리·1~4구간 신규 타입의 실제 스폰을 확인했다. 근거와 저공 이동·전용 연출의 후속 범위는 [전투 통합 기록](Docs/todo/CreatureCombat_Implementation_Report.md)을 따른다.

## 2026-10-10 그리핀·식충식물 에셋 이전

- Unity의 두 캐릭터 팩을 `/Game/Art/CreatureModels`에 이전했다. 그리핀·식물 본체·이동형 식물·뿌리의 4메시, 원본 모션 42개, 몽타주 25개, 색상별 배치 BP 11종과 PBR 재질을 제공한다.
- `/Game/Art/CreatureModels/Maps/L_PG_CreatureModels`에서 외형을 확인한다. `Tools/Art/CreatureModels/RunCreatureModels.py`가 임포트·재질 구성·재로드·렌더를 재현한다.
- 새 프로세스의 참조·모션·본 포즈 검사와 16개 렌더를 확인했다. 기존 전투 데이터는 유지하며 AI/스폰/비행 이동 연결은 별도다. 사용법·근거·한계는 [에셋 이전 기록](Docs/todo/CreatureModels_Import_Report.md)을 따른다.

## 2026-10-10 플레이어 기본 무기 카타나 01

- `BP_PlayerWeapon_Sword`의 표시 메시를 CombatGirls School Katana Girl의 `Weapon_Katana01` 칼날로 교체한다. 기존 무기 태그·어빌리티·스탯·충돌 박스와 캐릭터별 그립을 유지한다.
- `/Game/Art/PlayerKatana`에 칼날·칼집·원본 합본, 텍스처와 카메라 디더링(CPD 7) 재질을 저장한다. 실제 손 장착에는 칼날을 사용하고, 칼집·합본은 별도 에셋으로 제공한다.
- 원본 보관·FBX 준비는 `Tools/Art/PlayerKatana/PrepareKatana.py`, 적용·재로드·그립 검사는 `Tools/Validation/RunPlayerKatana.py`다. 상세 범위는 [카타나 교체 기록](Docs/todo/PlayerKatana_Implementation_Report.md)을 따른다.

## 2026-10-10 액션 카메라 근접 디더링

- 플레이어·카메라 근처 몬스터·각 부착 장비에 거리 기반 `DitherTemporalAA` 페이드를 적용한다. 실제 최종 시점과 대상별 캡슐 표면 거리를 사용하며 `CameraBodyClearance` 45cm에서 완전 숨김, 그 바깥 `CameraBodyFadeDistance` 100cm에서 점진적으로 복원한다. 3D 액션에만 적용하고 쿼터뷰 전환 시 전체 복원한다. 일반 재질 몬스터도 완전 숨김 거리에서는 내부 노출을 차단한다.
- CPD 슬롯 7을 사용해 MID를 교체하지 않고 본체·장비를 함께 처리한다. 페이드 중 화면 공간 외곽선은 억제하며 원거리·시점/모드 변경·장비 분리·종료 시 복원한다. 기존 출구 히스테리시스 대신 연속 거리 페이드를 사용한다.
- 툰/Hull 생성기와 저장된 마스터 20개에 연결했고, 현재 장착 검에는 원본 셰이딩을 보존한 전용 마스터를 추가했다. 재현 도구와 검증 범위는 [근접 디더링 기록](Docs/todo/CameraDitherFade_Implementation_Report.md)을 따른다.
- 검 전용 변형에서 함수 출력 이름 누락으로 Base Colour가 마스크에 연결된 소실 문제를 수정했다. 원래 Opaque인 검은 디더링 전 커버리지 1을 사용하고, 공용 연결기는 원래 출력 핀 이름을 보존한다. 저장 재로드 검사에 불투명 검의 커버리지 검증을 추가했다.

## 2026-10-09 액션 카메라 캐릭터 관통·몬스터 충돌 보완

- `EnemyCharacter` 프로필에서 Camera를 무시하고, 몬스터·장비의 `PostInitializeComponents`에서 기존 BP의 충돌 응답도 보정한다. 이동·공격·Visibility 판정은 유지한다.
- 3D 액션에서 지형 충돌로 시점이 몸에 가까워지면 `PGPlayerController.UpdateHiddenComponents`가 해당 시점의 플레이어 메시·부착 장비를 숨긴다. 최종 시점과 캡슐 거리를 사용하며 `PGQuarterViewData.CameraBodyClearance`(45cm), `CameraBodyHideHysteresis`(15cm)로 조절한다. 지형 충돌은 유지하고, 카메라가 멀어지거나 시점·모드가 바뀌면 자동 복원한다.
- 검증 결과와 한계는 [카메라 충돌 기록](Docs/todo/ActionCameraCollision_Implementation_Report.md)의 후속 수정 항목을 따른다.

## 2026-10-09 헤어 툰 셰이딩 부드러움

- 헤어의 반사·명암 경계를 완화하고, 머리 본을 따라가는 타원체 노멀을 원래 노멀과 혼합한다. 양면 카드의 엔진 노멀 반전을 보정하며, 헤어 월드 조명 비중을 0.25로 낮춰 카드 그림자의 강한 대비를 줄인다.
- 공용 헤어 프로필과 기존 인스턴스를 함께 갱신한다. `HairSoftness`, `HairNormalBlend`, `HairCenterOffset`으로 조절하며 얼굴·의상은 비활성 기본값을 사용한다. 재생성·백업·렌더 근거와 메시 자체의 잔여 한계는 [헤어 개선 기록](Docs/todo/HairToonSoftness_Implementation_Report.md)을 따른다.

## 2026-10-09 액션 카메라 상하 입력·지형 충돌 수정

- 3D 액션 모드의 세로 마우스 입력 부호를 반전한다. Pitch 제한·수평 회전·쿼터뷰 입력은 유지한다.
- 숲 맵의 바닥·경계·장애물 6개가 `Camera` 채널을 무시하던 원인을 수정했다. 기존 맵과 생성 코드를 함께 수정하며, 카메라는 24cm 구체 검사로 충돌 시 거리를 줄인다. 블루프린트 기본값 이후에도 충돌을 켜고 카메라를 검사된 소켓 위치에 둔다.
- 회전 지연을 끄고 이동 추종의 서브스텝·최대 지연 거리 60cm를 적용한다. `PGQuarterViewData`의 Collision 설정에서 반경과 최대 지연 거리를 조절한다.
- UE 5.8 Development 빌드, 카메라 자동 검사 3개, 저장 맵 새 프로세스 재로드·바닥 스윕, 실제 플레이어의 최대 상향 65° 렌더 검사를 통과했다. 지면 Z=0에서 카메라 중심 Z=24cm를 확인했다. 근거와 검증 한계는 [수정 기록](Docs/todo/ActionCameraCollision_Implementation_Report.md)을 따른다.

## 2026-10-09 툰 셰이딩 P0–P2 개선

- `PGToonPresentationComponent`가 명시적 `KeyLight`/`PGToonKeyLight` 태그와 안정적인 기존 맵 대체 선택을 사용한다. World Lit 재질의 확산 수광과 림·반사·상태 발광을 분리하고, 출력 해상도 기준 외곽선과 동일 스텐실 겹침선을 추가했다.
- 10종 외형에 모델별 얼굴 SDF를 적용하고 거리별 림·반사 감쇠와 원거리 머리 방향 갱신을 연결했다. 별도 개선 비교 맵의 6/8단계에는 224삼각형 머리 부착 그림자 프록시와 거리/토글 제어를 채택했다. 게임의 Unlit 얼굴은 SDF 경로를 유지한다.
- UE 5.8 공식 Toon BSDF를 격리 설정에서 비교했다. 현재 후보가 기존 얼굴 표현보다 낫다는 근거가 없어 전면 전환하지 않고 비교용으로 보존한다. 기본 렌더 설정은 유지한다.
- 적용·새 프로세스 재로드·60조건 렌더·PG 45개·세 외형 이동 전투·Development 패키지의 두 카메라 숲 전투를 검사했다. 성능 측정 조건, 중단 원인 수정과 최종 수용 범위는 [툰 개선 기록](Docs/todo/ToonShading_Improvement_Implementation_Report.md)을 따른다. 개선 맵 실행은 `Tools/PlayToonImprovedComparison.ps1`이다.
- 재개 후 성능 검사기의 준비 판정·PIE 종료 순서를 수정해 12단계 측정과 정상 종료를 확인했다. RTX 3060 Ti/1080p 패키지 숲 전투의 프레임 p95는 쿼터뷰 9.70ms·3D 액션 9.63ms다. 50캐릭터 에디터 장면은 GPU 평균 11.4ms지만 프레임 p95 25–31ms로 60fps 수용에 미달한다. 측정 완료와 전체 성능 수용을 구분한다.

## 2026-10-09 비인간형 몬스터 모션 점검

- 골렘 `BS_Golem`의 표본은 존재하지만 보간 캐시가 비어 있는 오류를 수정했다. 생성 시 `ResampleData`를 호출하고 6개 속도의 실제 표본 선택을 검사한다. 기존 공용 모션 선택은 유지된 상태이며, 골렘 모델 폴더에 전용 모션이 없어 전용 모션 교체는 원본 경로 확인 대기다.
- 새끼 거미·스파이더 퀸·리치·엔트의 대기/이동 및 공격 원본은 각 모델의 `Animations` 폴더를 사용한다. 폴더 출처와 이동 보간 검사를 추가했다. 스파이더 퀸의 두 공격 실행·렌더 및 골렘의 두 공격·본 포즈 회귀는 통과했다. 연속 이동의 시각적 이상 원인은 아직 확정하지 않았다. 근거는 [몬스터 모션 점검 기록](Docs/todo/CreatureMotions_Inspection_Report.md)을 따른다.

## 2026-10-09 플레이어 Idle · 방향 전환 폴리싱

- 공통 플레이어 Idle을 Sword2 `Unequip_Idle`의 편안한 대기 자세로 교체한다. `BS_PlayerSword`의 속도 0인 9개 표본을 교체하며 적의 Idle은 유지한다. 전체 재생성 설정은 `HumanoidLocomotion.json.player_idle_prefix`다.
- 60° 미만 변경과 달리기 중 반전은 방향별 이동을 이어간다. 제자리 턴은 정지 출발에서만 사용하며, 감속·벽 접촉 중 재진입하지 않는다. 턴 중 이동 재개를 65%에서 20%로 앞당기고, 가속 재개와 함께 턴 포즈를 지상 이동으로 블렌딩한다. 입력 방향 변경 취소 기준은 65°에서 20°로 낮춘다.
- 적용·백업·새 프로세스 검증은 `RunPlayerLocomotionPolish.py`, 입력·렌더 검사는 `RunPlayerTurns.py --step preview`다. 검증 결과와 한계는 [턴 모션 기록](Docs/todo/PlayerTurns_Implementation_Report.md)을 따른다. 아래 초기 턴 설정 설명보다 이 항목이 우선한다.
- Development 빌드·에셋 재로드·PG 53개와 두 외형의 입력 56조건/448장 검사가 통과했다. 60Hz 검사에서 큰 각도 정지 출발의 이동 감지는 117ms, 45° 출발은 17ms였다. 장시간 직접 조작감·경사면·모든 의상 검수는 별도다.

## 2026-10-09 설정에서 전환하는 카메라 모드

- HUD의 설정 버튼 또는 Esc로 카메라 설정을 연다. 기본은 기존 쿼터뷰이며, 3D 액션은 마우스로 Pitch/Yaw를 회전하고 휠로 거리를 조절한다. Alt를 누르면 UI 커서를 표시한다.
- 3D 액션은 카메라 기준 WASD와 화면 중앙 조준점을 사용한다. 지면을 조준할 수 없는 방향에서는 카메라의 수평 방향을 사용하며 이전 지면 목표를 지운다. 각 모드의 줌 거리와 회전 상태는 전환 중 유지한다.
- 선택은 캐릭터/런 저장과 별개인 UPGCameraSettings의 GameUserSettings 설정에 저장한다. 리스폰·맵 변경에서도 선택한 모드로 시작한다. 설정·인벤토리·보상 창에서 복귀할 때 커서/카메라 입력을 복원한다.
- PGQuarterViewData의 Action3D 설정에서 기본 거리·줌 한계·Pitch 한계·주시 높이를 조정한다. 기존 쿼터뷰의 근접 줌과 가운데 버튼 Pitch 조절은 유지한다.
- 최종 UE 5.8 Development 빌드와 PG 자동 검사 53개가 통과했다(경고 포함 성공 9개). 결과는 `Saved/QA/CameraModes/FinalAutomation`이다. `Tools/Validation/PreviewCameraModes.py`로 720p 설정 창과 쿼터뷰/3D 액션 시작 구도를 렌더 확인했다. `Saved/QA/CameraModes/Presentation`에 결과와 PNG가 있으며, 이 프리뷰는 설정 선택을 디스크에 저장하지 않는다. 실제 마우스 연속 회전·Alt 전환·장애물 충돌의 조작감은 직접 플레이 검수 범위다.


## 2026-10-09 전투 측면·후방 이동

- 공격 중과 공격 종료 후 `CombatStrafeSeconds`(기본 1.5초) 동안 몸 방향을 유지해 기존 Sword2 좌우·후방·대각선 걷기/달리기를 사용한다. 실제 속도의 캐릭터 로컬 방향이 `BS_PlayerSword`를 구동한다.
- 유지 시간이 끝나면 기존 이동 방향 회전과 Turn으로 복귀한다. 커서만 움직일 때의 평상시 회전 정책은 유지한다. 설정은 `DA_PlayerLocomotion`과 `Tools/Validation/Data/PlayerTurns.json`에 있다.
- `RunPlayerTurns.py --step preview`에 걷기·달리기 각 8방향 검사를 추가했다. 검증 결과는 [턴 모션 기록](Docs/todo/PlayerTurns_Implementation_Report.md)을 따른다.

## 2026-10-09 공격 형태·빌드별 VFX 재제작

- 플레이어 공격 8종에 새 머티리얼 9개와 접점별 6개 형상을 연결했다. `PGPlayerSkillProfile.SwingShapes`가 피해 판정과 독립적으로 횡베기·찌르기·세로 베기·회전·충격·검기를 선택한다. 원본 손 궤적에 맞춰 원월참 2번째와 공중 연격 마지막 접점의 찌르기를 분리했다.
- 출혈·충격파·격분의 실제 보유 강화와 활성 격분을 시전 시 복사해 붉은 잔광·푸른 압력선·금색 추가 궤적을 합성한다. 혼합 빌드와 발사된 검기의 시전 당시 표현을 유지한다. 에디터 재질 준비를 로드아웃 단계에 완료해 첫 시전 누락을 보완했다.
- 원본/적용은 `Tools/Art/PlayerCombatVFX`, 에셋은 `/Game/Art/PlayerCombatFX/Authored`다. 피해·접점 시각·쿨다운·이동을 보존하고, 저장 재로드·PG 51개 테스트·24접점×5빌드의 120개 렌더와 첫 시전/빌드 색상 픽셀 검사를 통과했다. 재현·백업·직접 플레이/패키지 성능 검증 한계는 [VFX 구현 기록](Docs/todo/PlayerCombatVFX_Implementation_Report.md)을 따른다.

## 2026-10-09 플레이어 턴 모션 연결

- Sword2 좌·우 45°/90°/180° 턴을 공통 플레이어 AnimBP의 지상 이동에 연결한다. 정지 출발·이동 중 급반전은 원본 회전 곡선과 발 디딤을 사용하고, 작은 방향 변경은 기존 8방향 이동을 사용한다.
- `PGPlayerLocomotionData`가 선택 각도·재생률·이동 재개·블렌드와 모션을 소유한다. 공격·회피·키 해제는 턴을 취소하며 공중·IK·공격 슬롯은 유지한다. 리타겟 골반의 중복 회전을 제거한다.
- 재생성·백업·새 프로세스 재로드·실제 입력 렌더 검사는 `RunPlayerTurns.py`다. Development 빌드·PG 51개·두 외형의 턴 22조건/176장·이동 공격/콤보/대시 연결 회귀가 통과했다. 최종 근거와 튜닝, 직접 플레이 검수 범위는 [턴 모션 기록](Docs/todo/PlayerTurns_Implementation_Report.md)을 따른다.

## 2026-10-09 근접 줌 · Pitch 전용 카메라

- 플레이어 카메라는 월드 Yaw를 고정한다. 휠 줌은 부드럽게 보간하며 기존 `MinDistance`보다 가까운 구간에서 `CloseUpDistance`(기본 220)·`CloseUpPitch`(-10°)로 전환하고 주시 높이를 25cm 올린다. 원거리에서는 기존 쿼터뷰 구도를 유지한다.
- 가운데 마우스 버튼을 누른 채 세로로 드래그하면 Pitch만 -75°~ -5° 범위에서 조절한다. 일반 커서 이동은 카메라를 회전시키지 않으며 UI 위에서는 카메라 입력을 막는다. 정면 얼굴을 보려면 캐릭터를 카메라 쪽으로 이동시킨 뒤 멈춘다.
- UE 5.8 Development 에디터 빌드와 기존 `PG.Combat.` 자동 검사 7개가 통과했다(1개 경고 포함). 검사 결과는 `Saved/QA/CameraPitch/Automation`이며 근접 카메라의 실제 화면·마우스 조작은 별도 검수 대상이다.
- 거리·근접 각도·주시 높이·보간 속도·Pitch 제한은 `PGQuarterViewData`의 CloseUp/Pitch 설정에서 조정한다. 캐릭터 외형별 얼굴 프레이밍과 근접 장애물 가림은 직접 플레이 검수가 필요하다.

## 2026-10-09 플레이어 이동 방향 회전

- 일반 이동은 뒤쪽·대각선을 포함해 입력한 이동 방향으로 회전하고, 정지하면 마지막 방향을 유지한다. `PGCharacterPlayer.MovementFacingInterpSpeed`로 회전 보간 속도를 조정한다.
- 커서 지면 조준은 계속 갱신하되 평상시 몸 방향을 덮어쓰지 않는다. 공격 시작 조준과 공격·대시·피격 몽타주의 회전 처리는 유지한다.

## 2026-10-09 휴머노이드 이동 모션 교체

- 플레이어 공통 지상 이동을 검 전투용 8방향 걷기·달리기로 교체했다. 9종 외형에 적용하며 공중 상태·발 IK·상체/전신 공격 슬롯을 유지한다.
- P09 남녀 6종과 템플릿 4종을 스켈레톤 이동에서 분리하고, 검/방패 장비에 맞는 방향별 모션을 연결했다. 실제 이동 속도와 원본 이동 거리로 재생률을 맞추며 스켈레톤 몬스터와 기존 독립 보스·비인간형 이동은 유지한다.
- `RunHumanoidLocomotion.py`가 백업·51개 모션 생성·보간 데이터 재구축·새 프로세스 검증·격리 이동 렌더를 담당한다. Development/DebugGame 빌드·PG 50개·이동 공격 15회·몬스터 시전 19회·대시 회귀와 검증 범위는 [이동 모션 기록](Docs/todo/HumanoidLocomotion_Implementation_Report.md)을 따른다.

## 2026-10-09 드랍 아이템 라벨 이동 갱신

- `PGUILootOverlay`의 월드 아이템 탐색은 0.1초 주기로 캐시하고, 카메라 투영·거리/획득 대상·겹침 배치는 매 프레임 계산한다. 이동 중 라벨이 10Hz로 계단식 이동하던 원인을 제거했다.
- 캐시는 약한 액터 참조를 사용해 획득/파괴된 드랍을 즉시 제외하며, 이름·안내·아이콘은 표시 아이템이나 획득 대상 상태가 바뀔 때만 갱신한다.
- UE 5.8 Development 에디터 빌드·`PG.UI.LootLabelLayout`·실제 렌더링의 `loot/loot100/lootFar` 배치 및 획득 회귀가 통과했다. 결과는 `Saved/QA/LootLabelMotion/Automation`, `Saved/QA/20261009T084929Z_e055ecf7_reward_loot`에 있다. 이동 중 연속 화면의 직접 플레이 검수는 별도다.

## 2026-10-09 스킬 아이콘 축소 품질 · 빌드 HUD 제거

- 스킬 아이콘 8종은 원본을 유지하고 power-of-two 리사이즈·평균 밉맵·trilinear 필터를 적용한다. 최대 256px와 상주 로딩으로 작은 HUD 슬롯의 얇은 선이 거칠게 샘플링되는 문제를 보완한다. 기존 에셋 이관/재로드 확인은 `ConfigureSkillIconSampling.py`, 재임포트는 `ConfigureSkillIcons.py`다.
- 메인 HUD의 빌드 계열·강화 단계·발동/대상 상태 문구와 스킬 위 발동 표시를 제거한다. 빌드 패널용 54px 예약 영역도 제거하며 생명력·격분 자원, 스킬/회복약·쿨다운은 유지한다. 아래 전투 HUD 기록의 빌드 표시 설명보다 이 항목이 우선한다.

## 2026-10-09 체력·격분 액체 셰이더

- `SPGResourceOrb`에 UI 머티리얼 기반 수면·내부 흐름과 자원 증감 시 감쇠하는 출렁임을 연결했다. `PGUIStyleSettings.CombatOrbLiquid`와 `/Game/UI/Combat/M_PGResourceLiquid`를 사용하며 기존 금속 프레임은 유지한다.
- 후속 폴리싱으로 내부 얼룩의 대비를 줄이고 수면 반사·유리 하이라이트·깊이감을 다듬었다. 생명력/격분 라벨과 격분 시간 문구를 제거하고 현재/최대 수치를 구슬 정중앙에 배치했다. 기존 전투 바 위치는 유지한다.
- 원본 HLSL·머티리얼 생성·실행 검증은 `Tools/Art/CombatHUD/*ResourceLiquid*`에 있다. 최신 빌드·셰이더·화면 검증 상태는 [구현 기록](Docs/todo/ResourceLiquid_Implementation_Report.md)을 따른다.

## 2026-10-09 bOKUSEI 비교 맵 추가 모델

- 아린·화련(`DA_Hwarin`)·Lianlian에도 모델별 8단계 비교 무대를 제공한다. 일반 조명→셀 명암→부위별 명암→림→외곽선→월드 그림자→얼굴 SDF→얼굴 SDF·월드 그림자를 비교 전용 재질로 구성하며 원본 외형 에셋은 유지한다.
- `Tab`으로 Bokusei/아린/화련/Lianlian을 순환하고 `1–8`로 단계를 선택한다. 모델 전환 시 단계·얼굴(`F`)·쿼터뷰(`C`)를 유지하며 `0/R`은 선택 모델의 전체 단계를 보여준다. `H/J`와 조명 조작은 모든 비교 모델에 적용한다.
- `BokuseiGuestModels.py`가 배치, `BokuseiGuestStages.py`가 재질을 소유한다. `RunBokuseiShadingComparison.py --step guests`로 백업 후 기존 맵을 갱신하고 `--step guests-preview`로 새 프로세스 재로드·입력·렌더를 검증한다. 전체 맵 재생성에도 포함된다.
- 추가 모델의 대기 포즈는 비교 맵에서 사용하는 Bokusei 대기 모션을 직접 리타겟한다. 비교 전용 리그·모션으로 팔·다리·손가락 자세를 맞추고 시작 시점 1.25초와 재생률을 공유한다. 원본 플레이어용 리타겟 설정은 유지한다.

## 2026-10-09 핵앤슬래시 전투 HUD 재제작

- 핵앤슬래시의 하단 중앙 전투 배치와 서브컬처 게임풍의 셀 명암·차콜/샴페인 골드 장식을 결합한다. ImageGen 테두리·스킬바·회복약을 연결하고 붉은 생명력·호박색 격분 구슬을 사용한다. 우측 상단에 시련 목표·시작·장비를 묶고, 획득한 강화/발동 상태만 전투 바 위에 표시한다.
- `PGCombatHUDStyle`과 `SPGResourceOrb`는 전투 HUD 표현을 소유한다. 원본 금속 아트·임포트·해상도별 실제 실행/캡처는 `Tools/Art/CombatHUD`, 텍스처 교체 설정은 `PGUIStyleSettings.CombatOrbFrame`이다. 기존 인벤토리/보상 테마와 GAS·입력·진행 데이터는 유지한다.
- UE 5.8 Development 빌드와 720p·900p·1080p 회복약/레이아웃 검사를 통과했다. 최종 렌더·재실행 방법·검증 범위는 [전투 HUD 기록](Docs/todo/CombatHUD_Implementation_Report.md)을 따른다. 아래 9월 네이비 HUD·10월 달빛 프레임 기록보다 이번 전투 HUD 변경이 우선한다.

## 2026-10-09 숲속 폐허 바닥 반복감 수정

- 비늘처럼 겹쳐 보이던 포석을 230개에서 12개로 줄이고 중앙에 흙·이끼 지면을 드러냈다. 지면 노멀 강도를 낮추고 다른 식생·폐허 배치의 난수 순서를 보존했다. 현재 2,338 인스턴스이며 실제 플레이 시점 렌더·저장 재로드가 통과했다. 아래 2,556개 구성은 수정 전 기록이다.

## 2026-10-09 숲속 폐허 고디테일 환경 교체

- 보유 환경 팩을 재조사하고 `DreamscapeEastLands` 개별 모델 28종·텍스처 75개를 임포트했다. 원본 LOD0를 분리하고 나무껍질·잎 알파·석재 노멀/ORM·이끼 레이어를 Unreal 재질로 연결했다. Unity 씬·프리팹 배치는 사용하지 않는다.
- `/Game/Maps/L_PG_ForestRuins`를 석조 아치·수호상·회색 포석·이끼 지면·다층 식생과 따뜻한 등불로 교체했다. 2,556 인스턴스/25 HISM 그룹, Actor 22개와 기존 충돌 Actor 6개를 사용한다. 기존 전투·보상 데이터와 기본 맵은 유지한다.
- `DetailedForestLayout.py`·`layout.json`이 배치, `ImportDetailedResources.py`가 재질을 소유한다. `RunForestRuins.py --step preview --art-only`는 빠른 미술 프리뷰이며 일반 `preview`의 전투 검증과 구분한다. 현재 렌더·실행 근거·수정 방법은 [환경 구현 기록](Docs/todo/ForestRuins_Implementation_Report.md)의 고디테일 교체 절을 따른다. 아래 LowPoly 항목은 교체 전 이력이다.
- 최종 재로드·17개 경로·네 방향 경계·보조 16웨이브/7보상과 실제 렌더 8장 검사가 통과했다. 셰이더 컴파일 오류도 실패로 검사한다. 수동 난이도·장시간 GPU 성능·패키징은 별도다.

## 2026-10-09 개별 환경 리소스로 구성한 숲속 폐허 전투장

- Unity `Environment/LowPolyFantasyArena2`의 개별 FBX 40개·텍스처 4개로 `/Game/Maps/L_PG_ForestRuins`를 새 빈 레벨에서 구성했다. Unity 씬·프리팹 배치와 조립된 Arena 메시는 사용하지 않았다. 약 47×39m의 전투장에 중앙 석재 마당·네 곳의 낮은 잔해·외곽 숲을 배치하고 연속 지면·경계 충돌을 새로 제작했다.
- 기존 Stage GameMode와 6개 시련 데이터를 연결한다. 953개 장식은 24 HISM 그룹으로 저장하며 배치·조명 수정과 재생성은 `Tools/Art/ForestRuins/layout.json`, 바로 실행은 `Tools/PlayForestRuins.ps1`이다. 기본 시작 맵과 전투·성장 데이터는 유지한다.
- Development 빌드·저장 재로드·17곳 경로·네 방향 경계 이동·실제 적 스폰/추격과 보조 16웨이브/7보상 완주를 검사했다. 실행 근거·렌더·재생성 방법과 수동 밸런스/성능 검수 범위는 [숲속 폐허 기록](Docs/todo/ForestRuins_Implementation_Report.md)을 따른다.

## 2026-10-08 bOKUSEI 비교 맵 · 재생 중 조명 조절

- 기존 8단계 비교 맵의 공통 주광원을 방향키로 좌우·높이 각도 조절하고, Z/X/V 정면·측면·역광 프리셋, L 자동 회전, Backspace 시작 조명 복원을 제공한다. 방향광은 좌표 이동 대신 방향을 조절하며 카메라·단계 선택과 독립적으로 동작한다.
- `PGShadingComparisonPawn`은 각 모델의 `PGToonPresentationComponent.KeyLight`를 공유하고, 기존 컴포넌트가 툰 명암·얼굴 SDF 방향을 동기화한다. 한국어 안내와 현재 방향·높이·자동 회전 상태를 표시하며 속도는 Pawn 설정으로 노출한다. 바로 실행은 `Tools/PlayBokuseiShadingComparison.ps1`이다.
- Development 에디터 빌드·새 프로세스 재로드·35개 본 포즈·55개 실제 입력(조명 19개)·70개 MID 방향 동기화와 기존 H/J 그림자 픽셀이 통과했다. 정면·측면·역광 렌더와 초기 연결 안내도 확인했다. 재현 근거·사용법·재생 중 적용 범위는 [비교 맵 기록](Docs/todo/BokuseiShadingComparison_Implementation_Report.md)을 따른다.

## 2026-10-07 bOKUSEI 머리카락 그림자 품질 개선 · 중단 작업 재개

- 미적용 후보와 마지막 진단을 확인하고 비교 맵의 공통 주광원 Source Angle을 20°로 조절했다. 헤어 투사체의 컷오프 0.35·양면·안쪽 0.08cm, 보이는 반투명 헤어와 실제 게임의 얼굴 SDF는 유지한다. H 가림막은 모든 비교 단계에서 같은 130×50×14cm를 사용한다.
- `hair_shadow_settings.json`을 생성 도구에 연결하고 설정 해시·저장 재로드 검사, 고정 PIE/TSR 캡처와 피부 그림자 경계 측정을 추가했다. 6/8단계 정면·비스듬한 시점의 그림자 기울기 RMS가 28–31% 감소했으며 실제 얼굴 투사를 유지했다.
- 최종 생성·저장 재로드·8개 모델·35개 본·입력 33개·H/J 픽셀 검증 PASS. 최종 각도 렌더 14장과 실패 기록·백업·재현 방법은 [비교 맵 기록](Docs/todo/BokuseiShadingComparison_Implementation_Report.md)의 2026-10-07 품질 개선 항목을 따른다. 일부 측면 카드 경계와 GPU 비용·패키지 장시간 품질은 추가 폴리싱 범위다.

## 2026-10-06 이동 공격 · 대시 연결

- 기본 3타는 WASD 이동 100%와 상체 공격, 관통검기는 이동 90%와 상체 발사, 원월참은 전신 회전을 유지하며 이동 85%를 사용한다. 공격 시작·종료·콤보 전환·히트스톱에서 Walk 속도를 지우지 않으며 이동 방향은 실제 속도를 캐릭터 기준으로 계산한다. 피해·모션 접점·쿨다운·취소 시각은 유지한다.
- 대시 중 입력 예약을 0.45초로 분리하고, 이동 키를 누른 상태의 종료 속도를 보행 속도로 제한해 다음 공격과 연결한다. 정지 입력·취소는 즉시 정지하며 기존 벽 충돌·적 통과·잔상 정리를 유지한다.
- 원본은 `HackSlashP0.json` / `HackSlashP1.json`, 적용·백업·재로드는 `ConfigureMobileCombat.py`, 실행 검증은 `RunMobileCombat.py`다. Development·DebugGame 빌드, PG 50개·8종 공간 판정·콤보·세 외형의 15회 이동 공격과 별도 대시 회귀가 통과했다. Development의 이동 공격 실행도 별도로 확인했다. 일반 플레이는 `Tools/PlayMobileCombat.ps1`로 실행한다. 검증 근거와 직접 플레이 폴리싱 범위는 [이동 공격 기록](Docs/todo/MobileCombat_Implementation_Report.md)을 따른다.

## 2026-10-06 Bokusei 모델 기반 얼굴 SDF 적용

- 실제 Bokusei LOD0 얼굴 위치·노멀·UV0와 Head 본 좌표를 추출해 512×512 좌우 얼굴 명암 전환 맵을 제작했다. 19단계 조명 마스크의 2D signed distance 영점을 보간하며 피부 UV 섬만 적용한다.
- 게임에서 사용하는 얼굴 MI의 부모를 전용 Unlit SDF 마스터로 교체했다. DA·메시·다른 9개 슬롯의 해시는 유지하며 기존 얼굴은 Baseline으로 보관한다. 비교 맵 7단계는 적용한 얼굴, 8단계는 비교 전용 월드 수광·헤어 그림자 변형이다.
- `RunBokuseiFaceSDF.py --apply`가 추출·베이크·백업·적용·8단계 비교 맵 생성·검증을 재현한다. 에디터 빌드, 좌우/정면/후면·천정/바닥·쿼터뷰 캡처, 저장 재로드, 애니메이션 중 머리 방향 및 입력 33개 검사를 통과했다. 설정·복원·확인 범위는 [얼굴 SDF 기록](Docs/todo/BokuseiFaceSDF_Implementation_Report.md)을 따른다.

## 2026-10-06 bOKUSEI 머리카락 그림자

- 비교 맵의 6단계와 얼굴 SDF가 있는 8단계에 반투명 헤어를 유지하는 그림자 전용 Masked 메시를 추가했다. 두 헤어 슬롯만 투사하며 같은 메시·LOD·Leader Pose를 공유한다. `J`로 헤어 그림자를 전환하고 기본 가림막은 꺼 두어 `6 → F → J`로 얼굴의 차이를 볼 수 있다.
- C++ 빌드, 정면·비스듬한 얼굴·PIE의 피부 수광 픽셀, 헤어 투사체 2개의 포즈 일치, 현재 8개 모델·35개 본 비교·입력 33개 검증이 통과했다. 최종 렌더와 입력 재검증의 별도 실행 근거는 [비교 맵 기록](Docs/todo/BokuseiShadingComparison_Implementation_Report.md)의 머리카락 그림자 항목을 따른다.

## 2026-10-06 bOKUSEI 단계별 셰이딩 비교 · 자유 카메라 · 그림자

- 기존 `/Game/Art/ToonTest/Maps/L_PGToon_Bokusei_ShadingComparison`을 일반 Lit → 공통 셀 명암 → 피부·얼굴·헤어별 명암 → 림·하이라이트 → 외곽선 → 월드 그림자의 6단계로 확장했다. 10개 슬롯의 텍스처·색상·투명도, 메시·LOD0·Leader Pose를 공유하며 5단계는 현재 Bokusei 표현, 6단계는 비교 전용 Default Lit 툰 수광 변형이다.
- 비교 전용 `PGShadingComparisonPawn`으로 재생 중 WASD/QE 이동·우클릭 회전·Shift 가속, 1–6 단계 선택·F 얼굴·C 쿼터뷰·0/R 전체 보기·H 가림막 그림자 토글을 제공한다. 5/6단계에 같은 가림막을 배치해 Unlit/월드 수광을 비교하며 한국어 조작 안내를 표시한다. 게임 기본 맵과 원본 외형은 보존한다.
- 생성·새 프로세스 저장 재로드·SM6 렌더·PIE 포즈와 실제 입력 경로 검증은 기존 `RunBokuseiShadingComparison.py`를 사용한다. 에디터 빌드·6단계 저장/렌더·25개 본 포즈 비교·입력 24개 및 실제 그림자 수광 픽셀 검증이 통과했다. 단계별 차이·사용법·검증 근거는 [비교 맵 기록](Docs/todo/BokuseiShadingComparison_Implementation_Report.md)을 따른다.

## 2026-10-05 bOKUSEI 셰이딩 비교 맵

- `/Game/Art/ToonTest/Maps/L_PGToon_Bokusei_ShadingComparison`에서 왼쪽 일반 Default Lit과 오른쪽 현재 `DA_Bokusei`의 Unlit 툰 재질·화면 공간 외곽선을 비교한다. 동일 메시·텍스처·색상·투명도·LOD0와 Leader Pose로 맞춘 대기 포즈를 사용한다.
- 정면·쿼터뷰·얼굴 카메라, 공통 광원·고정 노출과 한국어 안내판을 제공한다. 재생하면 정면 카메라에서 대기 모션이 반복된다. 원본 외형·재질·모션과 기존 게임 맵은 수정하지 않는다.
- 재생성은 `RunBokuseiShadingComparison.py`, 저장 재로드·10개 슬롯의 색상/알파 일치·SM6 렌더·PIE 동기 포즈 검증은 `PreviewBokuseiShadingComparison.py`다. 열기·카메라 사용법과 검증 근거는 [비교 맵 기록](Docs/todo/BokuseiShadingComparison_Implementation_Report.md)을 따른다.

## 2026-10-05 휴머노이드 보스 · 월식의 검성

- `15401` 월식의 검성을 6구간에 연결했다. 기존 황혼의 기사 데이터와 일반 15웨이브는 보존한다. P09 Armor007 남성 외형·검 그립과 보유 카타나 모션을 별도 전투 Skeleton으로 변환해 사용한다.
- `PGEnemyAttackProfile`이 2타/3타 연참의 접점·형상·피해·모션 구간과 검막 유지/지연 반격을 소유한다. 찌르기·돌진·고리 파동, HP 50% 전환과 패턴 종료 뒤 최소 대기, 취소·사망 정리와 한국어 보스 HUD를 연결한다.
- 원본은 `Tools/Validation/Data/HumanoidBoss.json`, 적용·백업·새 프로세스 재로드·실제 GAS 전투·접점 근접 캡처는 `RunHumanoidBoss.py`다. 검증 결과와 직접 플레이·연출 수용 범위는 [보스 구현 보고서](Docs/todo/HumanoidBoss_Implementation_Report.md)를 따른다.

## 2026-10-05 몬스터 배리에이션 · P09 등급 장비

- 보유 리소스로 새끼 거미·거미 여왕·리치·엔트·골렘(`15301–15305`)을 시련에 추가하고, P09 남녀를 일반·강화·정예 6종(`15201–15206`)으로 구성했다. 등급별 Armor003/007/012, Sword001/003/005와 정예 방패·방어·보장 드랍을 연결한다.
- `MonsterVariations.json`이 능력치·전투 패턴·장비·15웨이브 편성을 소유한다. 일반/강화/정예 P09는 1/3/4구간부터 등장하며 기존 웨이브 총수와 6구간 보스는 유지한다. 이전 P09 전용 편성 기록보다 이 구성이 우선한다.
- `PGCharacterAppearance.Attachments`는 표시 메시 본의 고정 장비를 관리하며 골렘은 `PGCreatureAnimInstance`로 기존 Fantasy Pack 모션을 재생한다. 적용·백업·저장 재로드·11종 실전 검사는 `RunMonsterVariations.py`이며 검증 결과와 범위는 [몬스터 배리에이션 보고서](Docs/todo/MonsterVariations_Implementation_Report.md)를 따른다.
- P09 그립은 성별 손바닥 접촉점과 검·방패 방향을 보정하고 정예 방패에 왼손 15개 본의 쥐기 포즈를 적용한다. 모듈형 표시 리더의 현재 본 위치로 전체 몸 경계를 계산해, 기본 자세의 팔 경계 때문에 손·전완이 잘못 컬링되는 문제를 수정했다. `RunMonsterVariations.py --apply --grips-only`는 외형 6개만 저장하며 `RunP09MonsterGrip.py`가 손잡이 접촉 오차·팔 경계·근접 시점을 검사한다.
- 중단 작업 재개 후 최신 빌드의 근접 표본·PNG 150개와 14개 검토 시트, 새 프로세스 저장 재로드, 11종 전투·9종 플레이어 외형 회귀 및 PG 자동 테스트 50개가 통과했다. 초기 그립 불합격 기록 이후의 최신 결과와 수용 범위는 [그립 보정 완료 기록](Docs/todo/MonsterVariations_Implementation_Report.md#p09-그립-보정중단-작업-재개-완료--2026-10-05)을 따른다.

## 2026-10-05 월빛 회복약 · HUD 크기 수정

- Q 또는 체력바 옆 버튼으로 최대 체력 40%를 즉시 회복한다. 3개 휴대·8초 대기시간이며 일반 구간 정비에서 체력/회복약을 보충한다. 공격·대시·피격 경직은 유지하고, 같은 구간의 웨이브와 프로필 저장은 재보급하지 않는다.
- `PGConsumableData`와 `DA_PGProgression.HealingPotion`이 수치·보급 정책·표현을 소유한다. 원본은 `Tools/Validation/Data/Consumables.json`, 에셋 적용·새 프로세스 재로드·GAS/실제 입력 검증은 `RunConsumables.py`다.
- 사용 안내가 나타날 때 HUD가 커지는 문제를 고정 크기/안내 영역과 축소 전용 스케일로 수정했다. PG 테스트 48개 및 1280×720 실행의 버튼 작동·회복·대기시간·소진·정비 상태 크기 검사가 통과했다. 상세 근거와 난이도 플레이테스트 범위는 [회복약 구현 보고서](Docs/todo/Consumables_Implementation_Report.md)를 따른다.

## 2026-10-05 적중 반동 보완

- 확정 피해 피드백에 `PGEnemyPresentationComponent`의 방향성 외형 반동을 추가했다. 교체 외형과 기존 메시를 지원하고 공격 몽타주 중에도 동작한다. 충돌 캡슐·피해·공격 상태는 유지한다.
- `PGCombatFeedbackData`의 각 타격 프리셋에서 거리·지속시간을 조절한다. 연속 적중 누적 방지와 사망 정리를 포함하며, 피격 플래시·음향 및 실제 체감 검수 범위는 [타격감 보완 기록](Docs/todo/CombatFeel_Implementation_Report.md)을 따른다.

## 2026-10-05 P09 몬스터 웨이브 편성

- P09 남녀 기본형(`15201/15202`)을 1~2스테이지부터, Armor007(`15203/15204`)을 3~5스테이지에 혼합했다. 기존 근접 추격자 20마리를 대체하며 웨이브별 총수·궁수/수호자/정예 수·6스테이지 보스는 유지한다.
- 편성 원본은 `Tools/Validation/P09WaveRoster.py`, 백업·적용·저장 재로드·실제 스폰 확인은 `RunP09Waves.py --apply`다. 콘텐츠 재생성도 P09 등록 시 같은 편성을 사용한다. 기존 추격자 AI/GAS와 외형 에셋 연결을 유지한다.
- 저장 재로드·전체 편성 보존 검사와 1~6스테이지 첫 웨이브 36마리의 실제 스폰 검사를 통과했다. P09 4종의 컨트롤러·표시 메시를 확인했으며, 밀집 전투 성능은 별도다. 실행 근거는 [플레이어 선택·P09 보고서](Docs/todo/PlayableToonCharacters_Implementation_Report.md)의 웨이브 편성 기록을 따른다.

## 2026-10-05 플레이어 8종 검 그립 확장

- LianLian·Honoka·Yura(저장 ID Hichi)·Siuha·Lili·Nenmir·Hwarin·Arin에 손잡이 접촉 프레임과 오른손 15개 본의 쥐기 포즈를 적용했다. bOKUSEI의 기존 값은 유지하며, 리그의 100배 단위 차이를 보정해 검의 월드 크기를 유지한다.
- 원본은 `PlayableCharacterGrip_Players.json`과 `PlayableCharacterPolish.json`이다. `RunPlayableCharacterPolish.py --step configure-grips`는 바뀐 외형 그립만 백업·저장하고 새 프로세스에서 재로드 검증한다.
- 저장된 8종의 공격·대시·장비 해제/재장착 6,908개 표본과 근접 캡처 560장, 9종/P09 런타임 및 PG 자동 테스트 46개가 통과했다. LianLian은 원본 긴 소매가 손을 덮으므로 접촉면 시각 확인에 한계가 있다. 전투 비교와 재현 근거는 [그립 확장 기록](Docs/todo/PlayableToonCharacters_Polishing_Implementation_Report.md#2026-10-05-플레이어-8종-검-그립-확장)을 따른다.

## 2026-10-05 시련 · 전투 준비 UI 개선

- 실패·보상·승리 창, HUD 시련 안내·준비/시작·강화 공명과 전투 준비 창에 ImageGen 달빛 프레임을 연결했다. `PGUIStyleSettings`에서 각 표현용 텍스처를 교체한다.
- 불필요한 영어와 닫기 키 안내를 지우고, 검술은 공격 방식·피해·재사용 시간, 강화는 획득 효과와 다음 선택 중심으로 표시한다. 실패 사유의 개발 원문은 로그에 보존하고 화면에서는 한국어로 안내한다.
- 원본·프롬프트·가져오기는 `Tools/Art/TrialUI`, 검증은 `RunRewardLootPresentation.py`와 `RunInventoryPresentation.py`다. 구현·검증 범위는 [시련 UI 보고서](Docs/todo/TrialUI_Implementation_Report.md)를 따른다.

## 2026-10-05 플레이어 대시

- 기존 회피 슬롯 10000을 Shift 대시로 바꾸고, 플레이어 스켈레톤의 전진 회피 모션을 FullBody 몽타주로 연결했다. 원본 모션과 기존 슬롯·강화 연계는 유지한다.
- `PGPlayerDashComponent`가 선택 외형의 포즈 잔상을 최대 8개 재사용하며, GAS의 RootMotionSource가 이동·벽 충돌을 처리한다. 기본 450cm/0.36초와 잔상 색·간격·수명은 컴포넌트에서 편집한다.
- 대시 중에는 캡슐의 Enemy 채널 차단을 Overlap으로 바꿔 적을 통과한다. 종료·취소·사망·EndPlay에서 원래 응답을 복구하며 벽 충돌과 피격 판정은 유지한다.
- 대시 중에는 캡슐의 Enemy 채널 차단을 Overlap으로 바꿔 적을 통과한다. 종료·취소·사망·EndPlay에서 원래 응답을 복구하며 벽 충돌과 피격 판정은 유지한다.
- 적용·검증 도구는 `ConfigurePlayerDash.py` / `RunPlayerDash.py`이며 범위와 실행 근거는 [대시 보고서](Docs/todo/PlayerDash_Implementation_Report.md)를 따른다.

## 2026-10-05 Hwarin · Arin · Yura 모델 이전

- Unity의 `PlayerModel_Hwarin`, `PlayerModel_Arin`, `PlayerModel_Yura` 프리팹에서 활성 부위·재질 변형·기본 체형/헤어 모프를 추출해 `/Game/Art/PlayerModels`에 스켈레탈 메시 3종을 저장했다. Arin의 SaltLine 의상과 Yura의 Twin Bun Braids 헤어를 포함한다.
- Hwarin·Arin을 플레이어 목록에 추가하고 Hichi의 표시 모델·이름을 Yura로 교체했다. `I → 캐릭터`에서 총 9종을 선택하며, 기존 저장 ID `Hichi`는 유지해 이전 선택 기록도 Yura로 이어진다. 세 합성 리그에 전용 리타게팅과 스케일 보정·무기 본 매핑을 연결했다.
- 일러스트는 사용자가 지정한 `AdditionalPortraits_20261005/T_Yura_v2.png`, `T_Hwarin.png`, `T_Arin.png`를 사용한다. `PlayableCharacterCatalog.py`가 재생성 시에도 같은 모델 ID와 초상화 경로를 유지한다.
- 원본·합성 FBX·재현 도구는 `Tools/Art/PlayerModels`에 있다. 연결은 `RunConnection.py`, 저장 원본은 `PlayableCharacterPolish.json`이며 배치용 블루프린트·확인 맵·검증 근거는 [모델 이전 보고서](Docs/todo/PlayerModels_Transfer_Report.md)를 따른다.

## 2026-10-05 모델 기반 초상화 검증·비율 수정

- 실제 선택 메시의 정면·얼굴 14장을 기준으로 제작한 투명 초상화 7종을 `PGCharacterAppearance.Portrait`에 연결했다. 원본·프롬프트는 `Tools/Art/MoonlitUI/ModelPortraits.json`에 보존하며 PNG 해시와 별도 UE 프로세스의 저장 참조 검사를 통과했다.
- 텍스처 준비 중 임시 정사각형 크기가 캐시되어 초상화가 늘어나던 문제를 `GetImportedSize()`로 수정했다. 카드는 상단 기준으로 자르고 상세는 전체 원본 비율을 유지한다. `RunInventoryPresentation.py --portrait-previews`의 실제 갤러리 버튼·배치 비율 검증과 실행 근거는 [달빛 성소 UI 보고서](Docs/todo/SubcultureUI_Implementation_Report.md#2026-10-05-모델-기반-초상화-재개-및-표시-비율-수정)를 따른다.

## 2026-10-05 Bokusei 검 그립 보정

- `DA_Bokusei`의 `Weapon.Sword / RightWeaponSocket`에 실제 손잡이 접촉점과 오른손 15개 손가락 본의 쥐기 포즈를 적용했다. 원본은 `PlayableCharacterGrip_Bokusei.json`과 `PlayableCharacterPolish.json`이며 다른 외형의 그립 값은 유지한다.
- 표시 AnimInstance는 리타게팅 뒤에 장착 상태에 맞춰 손가락 회전만 보간한다. 장비 해제 시 원래 포즈로 돌아가며, 손 밖 본·중복 본은 검증에서 거부한다. 원본 전투 애니메이션과 손목/팔은 변경하지 않는다.
- `RunGripPreview.py --motion --calibration Tools/Validation/Data/PlayableCharacterGrip_Bokusei.json`의 근접 렌더·1,000개 정렬 표본, 실제 GAS 8공격 비교, 7종/P09 런타임과 PG 테스트 45개가 통과했다. 플레이어 납도는 기존 정책대로 비활성이므로 검사는 격리 프로필의 부착 해제/재부착으로 수행한다. 상세 근거와 잔여 범위는 [폴리싱 기록](Docs/todo/PlayableToonCharacters_Polishing_Implementation_Report.md#2026-10-05-bokusei-실제-검-그립-적용)을 따른다.

## 2026-10-05 무기 휘두름 FX 누락 보완

- 후속 요청으로 큰 검기와 피해를 `ResolveHitPhases`의 동일한 모션 접점 목록에 연결했다. 기본 3타·액티브 5종의 24타가 각각 독립 피해를 주며, 기존 1타 계수를 유지해 추가 타격만큼 총 피해가 증가한다. 스킬 설명도 실제 타수·총 피해를 표시한다. PG 회귀 46개와 24타 적중/24타 헛스윙의 체력·FX 검증, 양쪽 에디터 빌드가 통과했다. 최신 동작과 피해 배율은 [검기/피해 보고서](Docs/todo/PlayerSlashFX_Implementation_Report.md)의 모든 베기·찌르기 피해 연결 기록을 따른다.
- 관통검기(114)의 발사 순간에 `NS_PGPlayerCastSwing`을 별도로 연결한다. `ProjectileSwingVFX/Radius`는 캐릭터 베기, 기존 `SlashVFX`는 투사체 표현을 담당하며 공격 판정과 모션 시간은 유지한다.
- uncooked Niagara의 첫 활성화 지연 컴파일로 짧은 베기가 시뮬레이션되지 않는 경우를 재현해, 장착 준비에서 컴파일을 완료하도록 수정했다. 장착 갱신 없이 주입된 프로필도 공격 시계 시작 전에 준비한다.
- Niagara 생성 도구의 113/114 오분류를 `HitPhases.Shape` 기반으로 수정했다. 후속 조사에서 피해 12페이즈와 원본 모션의 공격 24접점이 다른 것을 확인했다. `SwingNotifyName=P_HitPoint`로 원본 접점을 논리 시각에 매핑해 큰 검기를 각 베기·찌르기에 연결하며, 피해·모션 수치는 유지한다. `RunPlayerSlashFX.py --niagara --all-phases`는 24접점의 적중·헛스윙을 검사한다. 실행 결과와 범위는 [검기 FX 보고서](Docs/todo/PlayerSlashFX_Implementation_Report.md)의 원본 모션별 연결 기록을 따른다.

## 2026-10-05 기본 액티브 스킬 4종 장착

- 기본 장착은 질풍연참(111)·원월참(112)·낙성참(110)·관통검기(114)이며 기존 입력 1·2·3·4로 사용한다. `DA_PGProgression.DefaultActiveSkills`가 순서를 소유하고 검술 창에서 5종 중 서로 다른 4종을 배치·저장한다.
- 기존 v1 저장의 2종 선택과 프리셋 순서를 보존하고 나머지 슬롯은 기본 데이터에서 중복 없이 채운다. 저장 실패 시 장착 유지, 스킬 ID별 쿨다운 유지, 안전 단계의 변경 제한을 유지한다.
- `ConfigureDefaultSkills.py -PGApplyDefaultSkills`는 원본 에셋 백업 후 기본 장착만 수정한다. `RunHackSlashP1.py`에 기본 4슬롯의 실제 GAS 시전·피해 검사를 추가했다. 근거와 화면 검증 범위는 [P1 보고서](Docs/todo/HackSlashP1_Implementation_Report.md#2026-10-05-기본-액티브-스킬-4종-장착)를 따른다.

## 2026-10-05 달빛 성소 UI / UX

- 전투 준비 창에 달빛 성소 배경과 캐릭터 초상화 7종, 장비 / 검술·강화 / 전리품 / 캐릭터 탭을 적용했다. 캐릭터는 미리보기와 저장 확정을 분리하고, 선택 기록이 없는 프로필도 상세 미리보기를 표시한다.
- 검술은 목록·상세·고정 장착 슬롯의 세 영역으로 구성한다. 현재 장착과 적용 예정 구성을 구분하며 중복 스킬 배치는 슬롯을 교환한다. 미적용 변경은 적용 후 닫기·계속 편집·취소 후 닫기로 처리하고 저장 실패 시 유지한다.
- HUD의 빌드 요약을 하단 모서리로 옮기고 상태가 없는 설명은 숨긴다. 아트 원본·가져오기는 `Tools/Art/MoonlitUI`, 격리 프로필 화면·입력 검사는 `RunInventoryPresentation.py`다. 검증 근거와 후속 범위는 [달빛 성소 UI 보고서](Docs/todo/SubcultureUI_Implementation_Report.md)를 따른다.

## 2026-10-05 캐릭터 폴리싱 재생성·그립 기반

- P1 후속은 `RunPlayableCharacterGrip.py`의 7종 × 8공격 부착 전후 전투 비교와 접촉 좌표 기반 `GripOffset` 후보 계산을 제공한다. 고정 DLL 사본의 112회 전투·기존 캐릭터 런타임·PG 테스트 45개가 통과했다. 당시에는 실제 오프셋을 적용하지 않았으며 이후 Bokusei 적용은 위 기록을 따른다. 레거시 무기 충돌 경로의 수용은 별도다.
- `PlayableCharacterPolish.json`이 11종의 외형·리그·포즈·Op 원본을 보존한다. 생성은 수동 변경 감지, 전체 패키지 백업, 새 프로세스 의미 비교와 실패 복구를 거친다. `polish-export/validate/check` 경로와 단계별 범위는 [폴리싱 1차 기록](Docs/todo/PlayableToonCharacters_Polishing_Implementation_Report.md)을 따른다.
- `GripProfiles`는 기존 무기 태그·원본 소켓별 표시 부착점과 로컬 오프셋을 제공하며 캐시도 이 두 키를 사용한다. Bokusei 검 프로필에는 손가락 포즈까지 적용했고 나머지는 기존 표시를 유지한다. 다른 캐릭터의 그립 튜닝·발 IK·물리는 후속 수용 대상이다.

## 2026-10-05 툰 플레이어 선택 및 P09 몬스터 확장

- `I → 캐릭터`에서 Bokusei / LianLian / Honoka / Hichi / Siuha / Lili / Nenmir를 선택한다. 준비·정비 단계에서 변경하며 기존 검술·장비·GAS를 공유하고 선택 ID는 저장·새 도전에도 유지한다.
- `PGCharacterAppearance`와 표시 전용 리타게팅 컴포넌트가 외형, 툰 머리 축, 무기 본 매핑, 모듈러 부위를 데이터로 연결한다. P09 남녀 기본형·Armor007의 적 템플릿 4종은 `15201–15204`이며 기존 추격자 AI/GAS를 계승한다.
- 에셋은 `/Game/DataCenter/Characters`, 목록은 `DA_PGProgression.PlayableCharacters`에 있다. 재생성·검증은 `RunPlayableCharacters.py`, 사용법과 검증 범위는 [플레이어 선택·P09 보고서](Docs/todo/PlayableToonCharacters_Implementation_Report.md)를 따른다.

## 2026-10-05 툰 플레이어 선택 및 P09 몬스터 확장

- `I → 캐릭터`에서 Bokusei / LianLian / Honoka / Hichi / Siuha / Lili / Nenmir를 선택한다. 준비·정비 단계에서 변경하며 기존 검술·장비·GAS를 공유하고 선택 ID는 저장·새 도전에도 유지한다.
- `PGCharacterAppearance`와 표시 전용 리타게팅 컴포넌트가 외형, 툰 머리 축, 무기 본 매핑, 모듈러 부위를 데이터로 연결한다. P09 남녀 기본형·Armor007의 적 템플릿 4종은 `15201–15204`이며 기존 추격자 AI/GAS를 계승한다.
- 에셋은 `/Game/DataCenter/Characters`, 목록은 `DA_PGProgression.PlayableCharacters`에 있다. 재생성·검증은 `RunPlayableCharacters.py`, 사용법과 검증 범위는 [플레이어 선택·P09 보고서](Docs/todo/PlayableToonCharacters_Implementation_Report.md)를 따른다.

## 2026-10-05 플레이어 검기 Niagara 전환

- `/Game/Art/PlayerCombatFX/NS_PGPlayerSlash`와 `NS_PGPlayerBlade`를 8개 공격의 `SlashVFX`에 연결했다. 기존 MixedVFX 검기 원본을 복제해 검기 메시·스파크·연무의 로컬 시뮬레이션과 색/투명도 사용자 입력을 구성한다. 원본과 피해·판정·모션 데이터는 유지한다.
- 근접은 공격 논리 시계, 발사형은 투사체 수명으로 Niagara를 재생한다. 회전기는 반대 방향 원호 2개를 사용하며 종료 시 수동 풀 반환으로 정리한다. 기존 Plane 표현은 Niagara 참조가 없는 프로필의 대체 경로다.
- `RunPlayerSlashFX.py --niagara --apply`가 백업·생성·재로드·회귀·8종 실제 Niagara 재생/정리/화면 검사를 수행한다. 편집 항목과 검증 범위는 [검기 FX 보고서](Docs/todo/PlayerSlashFX_Implementation_Report.md)를 따른다.

## 2026-10-05 플레이어 검기 FX 개선

- 기본 3타와 액티브 5종의 고정 원호 재질을 진행 방향·밝은 칼날·폭 변화·분리 잔상·소멸을 갖는 절차적 검기로 교체했다. 회전기는 두 원호, 발사형 검기는 수명 끝 소멸을 사용한다.
- `PGPlayerSkillProfile.Presentation`에서 색·지속시간·폭·밝기·높이·방향을 조정한다. 근접 표현은 논리 시계와 히트스톱을 따르며 장착 시 메시/재질 렌더 준비를 시작한다. 피해·판정·모션 수치는 유지한다.
- `ConfigurePlayerSlashFX.py`가 백업 후 표현만 저장하고 `RunPlayerSlashFX.py`가 저장 재로드·회귀·8종 SM6 캡처를 검사한다. 셰이더 원본, 실행 근거와 직접 플레이/무기 리본의 후속 범위는 [검기 FX 보고서](Docs/todo/PlayerSlashFX_Implementation_Report.md)를 따른다.

## 2026-10-05 액티브 스킬 재생 속도 조정

- 기본 콤보와 별개로 남아 있던 액티브 110~114의 포즈 시간 압축을 완화했다. 논리 시간·포즈 키·타격 창·이동·조준 및 취소 시각을 1.5배로 늘려 기존 대비 재생 속도를 2/3로 낮췄다. 기본 3타, 피해량·이동 거리·쿨다운·투사체 속도는 유지한다.
- `ConfigureAttackMotion.py -PGActiveSkillTempo`는 기존 액티브 프로필 5개만 백업 후 갱신한다. JSON 원본과 재생성 도구도 함께 반영하고, 보간 중 순간 배속과 실제 런타임 포즈 진행에 속도 상한 검사를 추가했다. 수치·검증 근거·직접 플레이 확인 범위는 [P1 보고서](Docs/todo/HackSlashP1_Implementation_Report.md#2026-10-05-액티브-스킬-속도-재조정)를 따른다.

## 2026-10-04 툰 캐릭터 5종 추가

- Unity의 Nenmir·Spi_Reien·Suiha·lili·Hichi 기본 FBX를 `/Game/Art/ToonCharacters`에 스켈레탈 메시 5개·툰 재질 56개·텍스처 38개로 이전했다. 기존 월드 조명 툰 마스터와 부위별 프로필, 머리 본/광원 동기화, Stencil 73 외곽선을 사용한다.
- 각 폴더의 `BP_PG_<이름>_Toon`과 `/Game/Art/ToonCharacters/Maps/L_PG_ToonCharacters`에서 확인한다. `Prepare/Configure/Preview/Validate/RunToonCharacterBatch.py`가 소스 매핑·임포트·렌더·재로드 검사를 담당한다.
- Suiha의 원본에서 누락된 금속/렌즈 텍스처 2종은 원본 색상/불투명도로 대체하고, Suiha·Hichi의 깊이 기록형 반투명 헤어는 컷아웃으로 보정했다. SM6 16장 렌더와 새 프로세스의 저장 참조/MID 초기화/원본 해시 검증 PASS다. 기본 포즈 외형 이전이며 애니메이션·의상 물리·플레이어/GAS 연결은 별도다. 검증 결과와 한계는 [캐릭터 5종 이전 보고서](Docs/todo/ToonCharacters_Transfer_Report.md)를 따른다.

## 2026-10-04 스킬 속도·콤보 전환 후속 수정

- 액티브 5종의 과도한 모션 압축을 완화하고 8개 몽타주에 0.16초 Cubic 블렌딩을 적용했다. 프로필 시작 포즈를 처음부터 적용하고, 다음 공격 전환 중 이전 모션을 원본 배속으로 재개하지 않도록 수정했다.
- Development·DebugGame 빌드, DebugGame 에셋 재로드·자동 테스트 45개·8개 공격 공간 검사가 통과했다. 렌더링된 실제 유지 입력 콤보도 순서·교차 블렌딩·이전 포즈 유지·해제 후 종료 검사 PASS다. 직접 플레이의 체감 평가는 별도이며, 근거와 한계는 [P1 보고서](Docs/todo/HackSlashP1_Implementation_Report.md#2026-10-04-스킬-속도콤보-전환-후속-수정)를 따른다.

## 2026-10-04 기본 공격 속도 조정

- 기본 3타의 포즈·타격·이동 시간을 1.5배로 늘려 재생 시간을 0.72/0.75/1.02초로 조정했다. 다음 공격 허용은 0.48/0.51/0.72초이며, 기본 속도의 누름 연계 한 바퀴는 이론상 1.71초다. 피해량·총 전진 거리·액티브 스킬은 유지한다.
- `HackSlashP0.json`을 원본으로 `ConfigureNormalAttackTempo.py`가 기존 100/101/102 프로필의 시간만 백업 후 저장한다. 검증과 직접 플레이 조정 범위는 [P0 보고서](Docs/todo/HackSlashP0_Implementation_Report.md#2026-10-04-기본-공격-속도-조정)를 따른다.

## 2026-10-04 공격 모션 끊김 대응

- 히트스톱의 0초 애니메이션 갱신에서 이동 속도가 NaN/Inf가 되는 경로를 막고, 이동 → 공격 포즈 → 메시 평가 순서를 명시해 포즈의 한 프레임 지연을 줄였다. 장착 시 콤보 몽타주를 미리 불러온다.
- 프로필 포즈 매핑을 단조 Hermite 보간으로 바꿔 타격 키·게임플레이 시간을 유지하면서 구간 경계의 급격한 속도 변화를 완화했다. 전용 회귀 2개와 `RunHackSlashMotion.py`의 실제 뼈 포즈 기록을 추가했다. 최종 검증 및 직접 조작 검수 한계는 [P1 보고서의 모션 수정 기록](Docs/todo/HackSlashP1_Implementation_Report.md#2026-10-04-공격-모션-끊김-수정)을 따른다.
- 수정 후 자동 테스트 45개와 새 Development 패키지의 P0/P1 공간 검사가 통과했다. 패키지 8개 공격의 논리 시계 진행 샘플 298개에서 포즈 정체 0개를 기록했다. 직접 플레이의 자연스러움과 장시간 성능 수용은 별도다.

## 2026-10-04 핵 앤 슬래시 P0 전투 비교 도구

- P0 기본 3타·질풍연참·원월참 구현에 이어 `PGSkillScenario`와 `RunHackSlashComparison.py/.ps1`로 M10/M15/RING/E1의 격리 전투를 재현한다. 초기 배치를 시드로 고정하고 이관 전 스킬 행을 런타임에만 복원해 전후 비교한다.
- 피해·피격·처치·스킬 사용과 사망/시간 초과/중단 기록을 보존한다. 4개 시나리오 × 5시드 × 전후 2종의 직접 비교 목록을 만들며, 무입력 점검은 이 목록의 완료로 집계하지 않는다.
- UE 5.8 빌드, 기존 P0 회귀 및 8개 전후 배치 점검을 확인했다. 직접 조작·연속 모션·강화별 비교·패키지 성능 수용은 남아 있다. 범위와 재현은 [P0 구현 보고서](Docs/todo/HackSlashP0_Implementation_Report.md)의 전투 비교 도구 기록을 따른다.

## 2026-10-03 플레이어 콤보 복구·공격 구분

- 플레이어 공격 Ability 7개의 자기 공격 태그 차단을 제거해 실제 취소 창에서 기본 1→2→3타 연계를 복구했다. 몽타주 `RateScale`을 콤보 유지 시간에 반영하고 종료·취소 시 무기 판정을 정리한다.
- `DT_Skill`의 공격 재생 배율·근접 피해 배율·강타 표현을 기본 3타와 기존 스킬에 연결했다. 전진·회전·집중 연격은 각각 2·2·3개의 타격 창을 사용한다. 원본과 이관은 `PlayerAttacks.json` / `ConfigurePlayerAttacks.py`다.
- `RunPlayerAttacks.py`는 격리 프로필의 실제 GAS·몽타주 연계와 타격 창을 검사한다. 수치·변경 에셋·검증 근거·직접 플레이 잔여 항목은 [플레이어 공격 보고서](Docs/todo/PlayerAttacks_Implementation_Report.md)를 따른다.

## 2026-10-03 몬스터 BT 전체 역할 이관

- 기존 EQS 태스크의 비동기 요청 상태를 개체별로 분리하고 요청 취소·늦은 결과를 처리한다. 스킬 태스크는 실행한 GAS Ability의 종료까지 기다리며, 중단 시 해당 실행만 취소한다. 공격 직전 거리·시야를 재검사하고 정상 완료·취소를 구분한다.
- 아레나 6종 모두 `DT_Enemy.CombatBehaviorTree`의 편집 가능한 공통 BT/BB를 사용한다. 참조가 없으면 월드별 공유 런타임 트리로 복구한다. 기존 GAS 패턴·공격권 FIFO·보스 순서를 유지한다.
- `Positioning` 데이터로 빈 위치 이동과 공격 대기 간격을 조정한다. 최대 8개 후보·2개 경로 요청을 낮은 주기로 평가하며, 지원 대상 검색도 주변 Pawn 충돌 검색으로 바꿨다. `pg.AI.DebugPositions`와 `pg.AI.UseBehaviorTree`로 시각화·타이머 비교가 가능하다.
- `ConfigureCombatBT.py`는 백업 후 자산을 생성/연결하고 기존 편집을 보존한다. `RunCombatBT.py`는 전체 역할 자율 전투·보스 전환·중단/재개·타겟 이동·실제 위치 이동을 검사한다. 빌드, 35개 회귀 테스트 및 전체 진행 검사 결과는 [몬스터 BT 보고서](Docs/todo/MonsterBT_Implementation_Report.md)를 따른다.

## 2026-10-03 수호자 밀집 전투 CPU 최적화

- UE 5.8의 사용하지 않는 Mass 에디터 처리 큐에서 관찰한 긴 대기를 `mass.UseProcessingQueue=0`의 기존 작업 그래프 경로로 우회한다. 엔진 소스는 변경하지 않는다.
- 역할형 적의 피해·이동 충돌은 기존 캡슐을 사용한다. `PGEnemyDataRow.bUseSkeletalMeshCollision`이 꺼진 역할형 적은 스켈레탈 메시의 충돌·오버랩을 중지해 프레임마다 물리 뼈를 갱신하지 않는다. Legacy 및 물리 시뮬레이션 중인 메시와 명시적 선택은 기존 설정을 유지한다.
- `RunGuardianSoak.py --trace --no-images`와 `ExportGuardianCPU.py`는 CPU 추적을, `PGGuardianBenchmark` / `RunGuardianBenchmark.py`는 PIE 밖과 Development 패키지의 5→50→5마리 계측을 담당한다. 프레임 제한 대기를 분리하려면 `--fps-cap 0`을 사용한다. 직접 입력·자유 내비게이션·빌드 밸런스 검사와 구분하며, 검증 근거는 [수호자 보고서](Docs/todo/GuardianPresentation_Implementation_Report.md)의 CPU 최적화 기록을 따른다.

## 2026-10-04 P09 모듈러 캐릭터 툰 연결

- Unity `P09_Modular_Humanoid`를 `/Game/Art/P09Modular`에 개별 메시 193개·툰 인스턴스 103개·텍스처 82개로 이전했다. 기존 `M_PGToonWorld_multi`와 부위별 프로필을 재사용하며, FBX 메타데이터의 재질 이름→GUID 매핑으로 피부/의상 슬롯을 연결한다.
- `PrepareP09Modular.py`와 `Configure/Preview/ValidateP09Modular.py`, `RunP09Modular.py`가 분리 익스포트·연결·프리셋 생성·렌더·저장 검증을 담당한다. 기본 남성/여성과 Armor007 변형 4종을 `/Game/Art/P09Modular/Maps/L_PG_P09Modular_Toon`에서 확인한다.
- 기본 포즈의 외형 확인용이며 애니메이션 동기화·헤어 부착/물리·장비 UI/GAS 연결은 포함하지 않는다. `Hair_10`의 스킨 연결 예외, 사용법과 검증 근거는 [P09 툰 연결 보고서](Docs/todo/P09Modular_Toon_Transfer_Report.md)를 따른다.

## 2026-10-03 Unity lilToon 캐릭터 이전 테스트

- Bokusei에 Frank Slash **1,291개**, Grruzam Sword **959개**, RPG Animations **6,336개**, 합계 **8,586개**를 추가했다. `Animation/FrankSlash`, `Animation/GrruzamSword`, `Animation/RPGAnimations`에 저장하고 `/Game/Art/ToonTest/Maps/PackLibrary`의 26개 맵에서 대표 156개를 확인한다. `RunBokuseiPacks.py`가 소스 12종 직접 리타게팅·체크포인트 복구·렌더·저장 검증을 담당하며 ElfSelena는 사용하지 않는다. 전체 에셋 포즈/저장 및 최종 맵/파일 무결성 검증 PASS다. 보완 캡처의 종료 코드 오류와 표시 위치 보정, 원본 구간 1건 보정, Whip 무기 본 진단 및 전투 연결 범위는 [이전 보고서의 추가 3팩 기록](Docs/todo/ToonRendering_Transfer_Test_Report.md)을 따른다.
- Inori LOD 화면 검증은 `RunToonLODQuality.py` / `CaptureToonLODQuality.py` / `ReviewToonLODQuality.py`로 6모션·720p/1080p·자동 LOD 왕복의 원본 PNG, 프레임별 본/LOD 기록과 비교 영상을 만든다. 기존 Inori 리타게팅 설정으로 검사용 회피만 추가했다. 큰 LOD1 형상 팝, 얇은 헤어 선, 그림자 캐시 잔상을 구분하며 권장 설정·검증 한계·실행 근거는 [LOD 화면 품질 보고서](Docs/todo/ToonLODQuality_Validation_Report.md)를 따른다. 전투 맵 및 패키지 성능 인증은 별도다.
- Bokusei 카타나 모션을 기존 6개에서 **60개(제자리 40·이동 변형 20)**로 확장했다. `Configure/Preview/ValidateBokuseiMotionLibrary.py`와 `RunBokuseiMotionLibrary.py`가 원본 소스 Skeleton 의존성 복구·리타게팅·11개 확인용 맵·저장 재검증을 담당한다. 결과는 `Bokusei/Animation`과 `Animation/RootMotion`, 맵은 `/Game/Art/ToonTest/Maps/KatanaLibrary`다. RM 원본은 pelvis에 이동이 있어 XY를 Armature로 추출하며 수직/회전은 Hips에 남긴다. 60개 에셋·11개 맵의 렌더/PIE·저장 검증 PASS이며 게임플레이 이동/GAS 연결은 후속이다. 상세 근거는 이전 보고서의 60개 확장 기록을 따른다.
- Bokusei 모션 검증은 `ConfigureBokuseiMotionTest.py`로 Unity Anime Katana 팩의 대기·달리기·공격·회피·피격·사망 6개 FBX를 원본 리그에서 Bokusei로 직접 리타게팅한다. `/Game/Art/ToonTest/Bokusei/Animation`의 21개 체인 설정과 `/Game/Art/ToonTest/Maps/L_PGToon_Bokusei_MotionTest`의 반복 재생을 제공한다. `RunBokuseiMotionTest.py`가 생성·SM6 렌더/PIE·저장 재검증을 순차 실행한다. 기존 플레이어와 전투 데이터는 변경하지 않았으며, FK 기반의 모션 적용 검증으로 무기 그립·발 고정 IK·보조 의상 물리는 후속 항목이다. 상세 근거는 이전 보고서의 Bokusei 직접 리타게팅 기록을 따른다.
- 1단계 성능 후속 작업은 `RunToonPerformance.py` / `ProbeToonPerformance.py`로 동시 Unreal 실행 감시·본 움직임·반복 GPU 비교·CSV 저장 완료를 검사한다. 마지막 CSV 저장 전 종료 문제를 수정한 최신 50개/720p PIE LOD1 재측정(60초×3회)은 GPU 8.77–9.02ms, 프레임 p95 13.18–14.53ms, 합산 p99 16.40ms·최대 58.50ms다. 원본 CPU 추적의 최악 977.59ms 프레임에서는 GPU 가림 쿼리 결과 대기 970.71ms를 확인했다. `ExportToonCPU.py`가 측정 영역별 CPU/렌더/RHI 이벤트를 내보낸다. 이전 실행의 마지막 CSV 누락과 전체 완료 판정 정정, 근거 및 실제 전투·패키지 60fps 인증 한계는 이전 보고서 8차 기록을 따른다.
- `ConfigureToonPerformanceLOD.py`는 LOD 후보와 별도 `L_PGToon_PerformanceMotionTest`를, `ValidateToonPerformanceLOD.py`는 저장/기존 갤러리 회귀를 담당한다. 앞선 LOD 비교는 6차 기록에 보존하되 마지막 구간 누락에 관한 8차 정정을 함께 적용한다. 연속 영상과 이 테스트 카메라의 전환 거리 검증은 LOD 화면 품질 보고서에 추가했으며, 실제 전투 맵 설정 확정·채택은 후속 작업이다.
- 월드 조명 확장은 `ConfigureToonLightingLab.py`의 `/Game/Art/ToonTest/Advanced` 변형을 사용한다. Default Lit에 셀 명암·제한된 발광 채움을 결합해 실제 광원/그림자를 받고, `UPGToonPresentationComponent`가 지정 DirectionalLight와 애니메이션 머리 본을 MID에 동기화한다. 얼굴 투사 그림자 제어, 헤어 이방성 하이라이트, CustomStencil 73 기반의 가림을 고려하는 화면 공간 실루엣 외곽선을 제공한다. 기존 Unlit 갤러리와 별도로 `L_PGToon_LightingLab` 및 6모션 `L_PGToon_AdvancedMotionTest`를 저장한다. `ValidateToonLightingRuntime.py`는 PIE 본/광원 추적·수명주기·1/10/50개 렌더 계측을, `ValidateToonLightingAssets.py`는 저장 참조와 기존 갤러리 회귀를 검사한다. 검증 결과·한계는 이전 보고서 5차 기록을 따른다.
- 초기 셰이딩 개선은 `ConfigureToonShading.py`가 담당한다. `shading_profiles.json`의 부위별 명암·림·하이라이트, 미분 기반 셀 경계 완화, 거리 제한형 Hull 두께를 기존 테스트 에셋에 적용한다. Unity 셰이더 GUID로 불투명/컷아웃/투명을 구분해 Honoka 눈동자 누락·눈 하이라이트 덮임을 수정했다. `PreviewToonShading.py`로 근접/쿼터뷰/측면광/역광을 비교한다. 이 초기 갤러리는 Unlit 비교 기준으로 유지한다.
- Unity 프로젝트의 `Inori` 캐릭터를 `/Game/Art/ToonTest/Inori`에 스켈레탈 메시·텍스처·머티리얼 인스턴스로 이전했다. lilToon 셰이더 자체가 아니라 Unreal용 3단 셀 명암·림라이트·외곽선 마스터 머티리얼로 변환한다.
- `ConfigureToonCharacterTest.py`는 격리된 테스트 경로를 재생성하고 9개 활성 머티리얼 슬롯을 검증한다. `PreviewToonCharacterTest.py`는 저장하지 않는 프리뷰 월드에서 전면 3/4 렌더를 생성한다.
- 2차 테스트는 `ConfigureToonRetargetTest.py`로 ElfSelena의 대기·걷기·달리기·공격·피격·사망 6개 모션을 Inori Skeleton에 오프라인 FK 변환한다. `PreviewToonMotionTest.py`는 `/Game/Art/ToonTest/Maps/L_PGToon_Inori_MotionTest`에 쿼터뷰 모션 갤러리를 저장하며, `ValidateToonMotionTest.py`가 소스 불변·관절 길이·저장된 반복 재생/Leader Pose 연결을 검사한다. 일반 플레이어 Blueprint와 게임 맵은 변경하지 않았다.
- 추가로 Bokusei·Honoka·LianLian의 기본 FBX를 각 `/Game/Art/ToonTest/<이름>`에 이전했다. `ConfigureToonAdditionalCharacters.py`는 Unity GUID 기반 텍스처·알파 마스크를 32개 슬롯에 연결하고, `PreviewToonCharacterGallery.py`는 Inori 포함 4개 모델 비교 맵 `/Game/Art/ToonTest/Maps/L_PGToon_CharacterGallery`를 저장한다. 새 3개 모델은 아직 Reference Pose이며, `ValidateToonCharacterGallery.py`가 저장 참조·원본 파일 불변·기존 Inori 모션 회귀를 검사한다.
- 최종 SM6 렌더는 통과했지만 원본 FBX에는 유효하지 않은 Bind Pose, 누락된 스무딩 그룹, 과도한 스킨 영향도 경고가 있다. 실제 플레이어 채택 전 애니메이션 변형 QA와 FBX 재익스포트가 필요하다. 상세 내용은 [카툰 렌더링 이전 테스트 보고서](Docs/todo/ToonRendering_Transfer_Test_Report.md)를 따른다.

## 2026-10-03 Blender 유틸리티 모델 교체

- 개발 맵 5종(`DummyDevMap`, `EmptyDevMap`, `EnemyDevMap`, `FeatureDevMap`, `StageDevMap`)의 LevelPrototyping/Engine 기본 큐브 65개를 Blender 제작 `SM_PG_DevBlock_Corner`·`SM_PG_DevBlock_Centered`로 교체했다. 기존 두 메시의 서로 다른 피벗과 100cm 바운드, 액터 Transform 및 충돌 지오메트리를 보존한다.
- 전리품 희귀도 빔의 Engine 기본 Cylinder를 Blender 제작 `SM_PG_LootBeam`으로 교체했다. 기존 6cm 폭과 데이터 기반 높이, 동적 희귀도 머티리얼 경로는 유지한다.
- Blender 원본·FBX·명세는 `Tools/Art/UtilityModels`, UE 에셋은 `/Game/Art/UtilityModels`에 있다. `ReplaceTemporaryModels.py`가 임포트·백업·맵 교체·정적 검증을 수행하며, 저장된 맵의 교체 65개와 `APGLootDrop` CDO 연결을 새 에디터 프로세스에서 검증했다.

## 2026-10-03 몬스터 스킬 다양화

- 아레나 6종에 찌르기·삼연 사격·고리 공격·근접 내려찍기 7개를 추가해 총 15개 공격을 구성했다. `PGSkillDataRow`의 최소 거리·선택 가중치·압박 비용과 기존 페이즈·쿨다운을 사용한다. 기존 BT도 공통 공격·소환 Ability에 정확한 SkillID를 전달한다.
- `PGAI/PGCombatDirectorSubsystem`은 대상별 동시 공격 비용과 예고 시작 간격을 제한하며 대기 순서를 유지한다. `PGEnemyAttackPattern`은 공유 범위로 예고·판정을 계산하고, 삼연 사격은 대상당 한 번만 적중한다. 보스 2페이즈에 안쪽이 안전한 고리와 긴 반격 시간을 추가했다.
- 원본·이관은 `CombatVariety.json` / `ConfigureCombatVariety.py`, 에셋 검사는 기존 RunQA의 content schema 3, 렌더링 검사는 `RunCombatVarietyPresentation.py`다. 설계 자료, 적용 범위, 검증 결과 및 후속 폴리싱은 [몬스터 다양화 보고서](Docs/todo/CombatVariety_Implementation_Report.md)를 따른다.

## 2026-10-03 전투 입력·콤보

- 기본 공격을 누르는 동안 기존 취소 창에서 연계하며, 예약된 스킬·회피가 우선한다. `IA_Skill_Normal`의 단발 트리거 제거와 저장을 완료했다. UI·포커스·사망 시 누름 상태를 지운다. 회피는 현재 이동 입력 방향을 우선하고 정지 시 커서 방향을 사용한다.
- 콤보는 슬롯·기본 스킬·월드 시간으로 제한한다. 몽타주 길이·재생 속도와 `DT_Skill.ComboResetSeconds`로 연계 시간을 조정한다. 조작 설정 및 검증 범위는 [전투 조작 보고서](Docs/todo/CombatControls_Implementation_Report.md)를 따른다.

## 2026-10-03 UI 전용 SFX

- 보상 열기·선택·승리의 전용 SoundWave 3종을 `PGUIStyleSettings`에 연결했다. `/Game/DataCenter/Audio/UI`에서 UI 볼륨·버스·동시 발음을 관리한다. 저장 대기와 일반 실패에는 보상/승리음을 재생하지 않는다.
- 원본 합성 및 미리듣기는 `Tools/Art/UISFX`, 에셋 가져오기·검증은 `ConfigureUISFX.py` / `ValidateUISFX.py` / `RunUISFX.py`다. 사양과 검증 근거는 [SFX 구현 보고서](Docs/todo/UISFX_Implementation_Report.md)에 기록한다.

## 2026-10-03 보상·결과·전리품 UI

- 보상 카드는 공통 테마·가변 화면 크기·고정 선택 버튼을 사용하며 데이터의 빌드 계열과 실제 유효 특성에 따른 연관 표시를 제공한다. 결과 화면은 전리품 카드·누적 강화 내역·저장 대기/완료를 구분한다. `FPGRunResultView`는 표시 전용이며 저장 및 지급 권한은 기존 Profile/Stage에 유지한다.
- `PGUILootOverlay`는 라벨을 중앙에서 제한 배치하며 E 입력과 같은 획득 대상에만 안내를 붙인다. 기존 월드 드랍의 개별 위젯 생성을 중지하고 등장 궤적·희귀도 빔은 유지한다. 레이아웃·표현 설정과 검증 범위는 [메인 UI 보고서](Docs/todo/MainUI_Implementation_Report.md)의 보상·결과·전리품 기록을 따른다.

## 2026-10-03 철갑 수호자 표현·전용 모션

- 적 `15103`은 `PGEnemyPresentationData`와 `PGEnemyPresentationComponent`로 전용 방패·견갑, 방어/회복 자세, 예고 발광과 짧은 소리를 사용한다. 기존 GAS 피해·방어·회복 수치와 드랍/웨이브 데이터는 유지한다.
- 스킬별 몽타주 동기화와 이펙트 크기·강타 피드백 선택을 추가했다. 부착물은 충돌 없이 동작하며 취소·사망 시 표현을 정리한다.
- 후속 작업으로 전용 대기·걷기·달리기·강타 시퀀스와 `ABP_PGGuardian`을 15103에 연결했다. 기존 발동작 위에 방패 팔과 강타 상체 자세를 제작 시 계산해 저장하며 런타임 IK는 추가하지 않는다. `GuardianMotion.json`·`ConfigureGuardianMotion.py`로 재생성하고 기존 에셋 검사에서 연결·발 접지·타격 자세를 확인한다.
- `RunGuardianSoak.py`는 격리 프로필의 기본/밀집/복귀 조합에서 20분 렌더링 부하와 마스터 오디오를 계측한다. 직접 조작·내비게이션·패키지 성능 검증과 구분한다. 이 검사에서 발견한 무기 잔존 문제를 `PGPawnCombatComponent::EndPlay`의 보유 무기 정리로 수정하고 회귀 검사를 추가했다. 최종 성능·음량 판정과 측정 한계는 아래 보고서를 따른다.
- 원본 아트는 `Tools/Art/Guardian`, 런타임 에셋은 `/Game/Art/Guardian` 및 `/Game/DataCenter/Guardian`에 있다. 적용 범위, 재생성 도구와 검증은 [수호자 보고서](Docs/todo/GuardianPresentation_Implementation_Report.md)를 따른다.

## 2026-10-03 장비 UI 고도화 1차

- `UPGUIInventory`는 장착 정보·가방 그리드·아이템 비교를 제공하고 강화/검술·근처 전리품을 탭으로 분리한다. 선택과 장착은 별도 동작이며 가변 화면 크기와 영역별 스크롤을 사용한다.
- `PGUI/Style`의 공통 테마를 메인 HUD와 장비 화면이 공유한다. 장비/스테이지 화면의 입력·일시정지·포커스는 `UPGUIManager`에서 함께 관리한다. 프로필 저장 확정 이벤트로 열린 장비 화면을 갱신한다.
- 구현 범위와 검증 도구는 [메인 UI 보고서](Docs/todo/MainUI_Implementation_Report.md)의 2026-10-03 기록을 따른다.

## 2026-09-21 웨이브 스테이지

- `APGStageManager`는 `FPGStageDataRow.Waves`를 순서대로 진행한다. 각 웨이브의 스폰과 적 처리가 모두 끝나면 다음 웨이브로 넘어가고, 마지막 웨이브 완료가 스테이지 클리어다.
- 클리어 직후 보상 선택과 `BuildDuration` 카운트다운을 시작한다. 보상 선택 후에도 빌드 시간을 유지하고, 종료 시 다음 스테이지를 시작한다. 빌드 중 장비 창은 타이머를 멈추지 않는다.
- 데이터 호환, 이관 도구, HUD 및 검증은 [웨이브 스테이지 문서](Docs/todo/StageWaves_Implementation_Report.md)를 따른다.

## 2026-10-01 콘텐츠 3단계

- 핵심 강화 `BleedRecast / ShockFracture / FrenzyAfterimage`와 효과별 `PGCombatTuningData` 수치를 추가했다. 직접/추가 피해 원인을 구분하고 기존 GAS 경로로 피해를 확정한다.
- 보상 조건은 기존 RequiredPerk와 복수 AND/OR 조건을 함께 읽으며 장착 효과를 포함한다. 4·5구간의 데이터 정책으로 기존 세 카드 중 한 자리를 유효 핵심 강화에 배정한다. 총 21개 강화·7회 선택이다.
- 빌드 HUD, 튜닝 기반 카드/장비 창 설명, 테스트 프로필 전용 고정 비교 시나리오를 추가했다. 구현·검증·직접 조작 잔여 기준은 [콘텐츠 계획 3단계 기록](Docs/todo/17_content_implementation_plan.md#2026-10-01-3단계-빌드-완성-강화-구현)을 따른다.

## 2026-10-03 콘텐츠 4단계: 드랍 기반

- `PGProgressionData.DropPools`와 적 행의 `DropPoolId`로 일반 역할 3종·정예 2종·보스의 풀을 연결한다. 정예·보스는 일반 확률을 대체하는 장비 1개를 확정하며, 빈 풀 ID는 기존 전역 드랍 설정을 사용한다.
- 스테이지 드랍 수치는 런 시드·구간·웨이브·적 ID·해당 적의 스폰 순번으로 결정한다. 저장된 `RunId`와 획득 GUID 기록으로 체크포인트 재실행/버리기 후 중복 획득을 막는다. 소환 적은 드랍하지 않는다.
- 보스 전리품은 가방과 별도의 런 결과 슬롯에 승리와 함께 저장한다. 새 프로세스에서도 결과를 먼저 표시하고 사용자가 새 도전을 시작할 때 초기화한다. 저장 실패 시 결과 화면에서 저장을 재시도한다.
- 이관은 `ConfigureLootPools.py`, 에셋 검사는 기존 RunQA의 `ValidateLootPools.py` 경로다. 적용 범위·검증과 4단계 잔여 작업은 [콘텐츠 구현 계획](Docs/todo/17_content_implementation_plan.md)의 2026-10-03 기록을 따른다.

## 1. 프로젝트 개요

**프로젝트명**: UPlayground  
**엔진 버전**: Unreal Engine 5.8  
**프로그래밍 언어**: C++20  
**프로젝트 타입**: 쿼터뷰 액션 파밍 ARPG (Gameplay Ability System 기반, 파밍 루프 개발 중)

---

## 2. 핵심 플러그인

### 엔진 플러그인
- **GameplayAbilities**: GAS(Gameplay Ability System) 기반 어빌리티 시스템
- **EnhancedInput**: 향상된 입력 시스템
- **CommonUI**: UI 시스템
- **GameFeatures**: 게임 기능 모듈화
- **ModularGameplay**: 모듈화된 게임플레이
- **MotionWarping**: 모션 워핑 기능
- **AnimationWarping**: 애니메이션 워핑
- **AnimationLocomotionLibrary**: 이동 애니메이션 라이브러리
- **SkeletalMeshModelingTools**: 스켈레탈 메시 모델링 도구

### 커스텀 플러그인
- **PGBlueprintUtil**: 블루프린트 유틸리티

---

## 3. 모듈 구조

프로젝트는 8개의 주요 모듈로 구성되어 있습니다.

```
UPlayground (메인 모듈)
├── PGAbilitySystem (어빌리티 시스템)
├── PGActor (액터 및 캐릭터)
├── PGAI (인공지능)
├── PGData (데이터 관리)
├── PGMessage (메시지 시스템)
├── PGShared (공유 리소스)
└── PGUI (사용자 인터페이스)
```

---

## 4. 모듈 상세 설명

### 4.1 UPlayground (메인 모듈)

**역할**: 프로젝트의 메인 모듈로 게임 로직의 진입점

**주요 디렉토리**:
- `AnimInstances`: 애니메이션 인스턴스
- `AnimNotify`: 애니메이션 노티파이
- `Cheat`: 치트 기능
- `GameMode`: 게임 모드
- `Utils`: 유틸리티 함수

**의존성**:
```cpp
PublicDependencyModuleNames: 
- Core, CoreUObject, Engine, InputCore
- EnhancedInput, GameplayTags, DeveloperSettings
- UMG, Niagara, CommonUI

PrivateDependencyModuleNames:
- GameplayTasks, GameFeatures, GameplayAbilities
- ModularGameplay, AnimGraphRuntime, AIModule
- PGData, PGAbilitySystem, PGActor, PGShared
```

---

### 4.2 PGAbilitySystem (어빌리티 시스템)

**역할**: Gameplay Ability System 기반의 스킬 및 능력치 관리

**핵심 클래스**:
- `UPGAbilitySystemComponent`: 커스텀 어빌리티 시스템 컴포넌트
- `UPGAtrributeSet`: 캐릭터 능력치(HP, MP 등) 관리

**주요 디렉토리**:
- `Abilities`: 게임 어빌리티 정의

**특징**:
- GAS를 활용한 스킬 시스템
- 네트워크 복제 지원
- 능력치 변경 이벤트 처리

---

### 4.3 PGActor (액터 및 캐릭터)

**역할**: 게임 내 모든 액터와 캐릭터 관리

#### 주요 구조

```
PGActor/
├── Characters/
│   ├── PGCharacterBase (베이스 캐릭터)
│   ├── Player/
│   │   └── PGCharacterPlayer (플레이어 캐릭터)
│   └── NonPlayer/
│       └── Enemy/
│           └── PGCharacterEnemy (적 캐릭터)
├── Components/
│   ├── Combat/ (전투 컴포넌트)
│   │   ├── PGPawnCombatComponent
│   │   ├── PGPlayerCombatComponent
│   │   ├── PGEnemyCombatComponent
│   │   └── PGSkillMontageController
│   ├── Stat/ (스탯 컴포넌트)
│   │   ├── PGStatComponent
│   │   ├── PGPlayerStatComponent
│   │   └── PGEnemyStatComponent
│   └── Input/
│       └── PGInputComponent
├── Controllers/
│   ├── PGPlayerController
│   └── PGAIController
├── Projectile/ (투사체)
│   ├── PGProjectileBase
│   └── Pool/ (오브젝트 풀링)
│       ├── PGProjectileManager
│       ├── PGProjectilePool
│       └── PGPooledProjectile
├── AreaOfEffect/ (범위 공격)
│   └── PGAreaOfEffectBase
├── Weapon/ (무기)
│   ├── PGWeaponBase
│   └── PGPlayerWeapon
├── Effects/
│   └── Decal/
│       ├── PGDecalActor
│       └── PGSkillIndicator (스킬 인디케이터)
└── Handler/
    └── Skill/ (스킬 핸들러)
        ├── PGSkillHandler
        ├── PGPlayerSkillHandler
        └── PGEnemySkillHandler
```

#### 핵심 클래스

**APGCharacterBase**
```cpp
- IAbilitySystemInterface 구현
- UPGAbilitySystemComponent 보유
- UMotionWarpingComponent로 모션 워핑 지원
- FPGSkillHandler를 통한 스킬 관리
- CharacterTID로 캐릭터 식별
```

**컴포넌트 시스템**
- **전투 컴포넌트**: 전투 로직 처리
- **스탯 컴포넌트**: HP, MP 등 능력치 관리
- **입력 컴포넌트**: 향상된 입력 시스템

**프로젝타일 시스템**
- 오브젝트 풀링 패턴 적용
- `PGProjectileManager`: 전역 투사체 관리자
- `PGProjectilePool`: 타입별 풀 관리
- `PGPooledProjectile`: 풀링된 투사체
- `PGPatternProjectile`: 스켈레톤 궁수의 15102/15112는 `/Game/DataCenter/SkeletonArcher/BP_PGSkeletonArrow`에서 기존 `SkeletonEnemy/.../Bow/Arrow/SM_Arrow`와 원본 재질을 사용한다. +Y 화살을 -90° 회전해 비행 +X에 맞추며 Box 충돌·속도는 유지한다. 두 사격 모두 단발·직선 예고이고, 기존 몸체용 활 공격 `Anim_Attack`을 알림 없는 전용 몽타주로 연결해 원본 발사 시점과 동기화한다. 재현·저장 재로드·실제 GAS 렌더 검사는 `Tools/Validation/RunSkeletonArcher.py --apply`, 상세 근거는 [궁수 수정 보고서](Docs/todo/SkeletonArcher_Presentation_Report.md)를 따른다. 이전 크리스털 모델은 공통 클래스 기본값으로만 남는다.

---

### 4.4 PGData (데이터 관리)

**역할**: 게임 데이터 테이블 관리 및 로딩 최적화

#### 핵심 클래스: UPGDataTableManager

**특징**:
1. **GameInstanceSubsystem** 기반 싱글톤
2. **AssetRegistry**를 통한 자동 데이터 테이블 스캔
3. **지연 로딩** 및 **LRU 캐시** 시스템
4. **SearchKey 메타데이터** 기반 빠른 검색
5. **동적 타입 기반 API**

**주요 기능**:

```cpp
// 자동 타입 추론 API
UDataTable* LoadDataTable<T>();
T* GetRowData<T>(int64 SearchKey);
T* GetRowDataByName<T>(const FName& RowName);
TArray<T*> GetAllRowData<T>();
void UnloadDataTable<T>();

// 매크로 접근
#define PGData() UPGDataTableManager::Get()

// 사용 예시
auto* SkillData = PGData()->GetRowData<FPGSkillDataRow>(1001);
```

**캐시 시스템**:
- 최대 50개 테이블 캐시
- 100MB 메모리 제한
- 600초(10분)마다 자동 정리
- LRU 알고리즘 적용

**데이터 테이블 구조**:
```
DataTable/
├── ActorAssetPath/
├── AreaOfEffect/
├── AssetPath/
├── Projectile/
└── Skill/
```

---

### 4.5 PGShared (공유 리소스)

**역할**: 프로젝트 전체에서 사용되는 공용 타입 및 상수 정의

#### 디렉토리 구조

```
Shared/
├── Debug/
│   └── PGDebugHelper.h (디버그 헬퍼)
├── Define/
│   └── PGSkillDefine.h (스킬 관련 상수)
├── Enum/
│   ├── PGEnumTypes.h (일반 열거형)
│   ├── PGEnumDamageTypes.h (데미지 타입)
│   ├── PGMessageTypes.h (메시지 타입)
│   ├── PGProjectileEnumType.h (투사체 타입)
│   ├── PGSkillEnumTypes.h (스킬 타입)
│   └── PGStatEnumTypes.h (스탯 타입)
├── Structure/
│   ├── PGDamageFloaterCurveData (데미지 플로터 커브)
│   └── PlayerStructTypes (플레이어 구조체)
├── Message/
│   ├── Base/
│   │   ├── PGMessageEventDataBase
│   │   └── PGMessageEventDataTemplate
│   └── Stat/
│       └── PGStatUpdateEventData
└── Tag/
    ├── PGGamePlayTags (게임플레이 태그)
    ├── PGGamePlayInputTags (입력 태그)
    ├── PGGamePlayEventTags (이벤트 태그)
    └── PGGamePlayStatusTags (상태 태그)
```

**특징**:
- 모든 모듈에서 참조 가능한 공통 타입
- 게임플레이 태그 중앙 관리
- 메시지 이벤트 데이터 템플릿 제공

---

### 4.6 PGUI (사용자 인터페이스)

**역할**: 게임 UI 시스템 관리

#### 구조

```
PGUI/
├── Component/
│   └── Base/
│       ├── PGButton
│       └── PGWidgetComponentBase
├── Widget/
│   ├── Base/
│   │   └── PGWidgetBase
│   ├── Billboard/ (월드 공간 UI)
│   │   ├── PGUIEnemyNamePlate (적 네임플레이트)
│   │   └── PGUIPlayerHpBar (플레이어 HP바)
│   ├── DamageFloater/
│   │   └── PGUIDamageFloater (데미지 플로터)
│   └── HUD/
│       ├── PGUIHudPlayerInfo (플레이어 정보)
│       └── Skill/
│           ├── PGUIHudSkill (스킬 HUD)
│           └── PGUISkillSlot (스킬 슬롯)
└── Manager/
    └── PGDamageFloaterManager (데미지 플로터 관리자)
```

**UI 타입**:
1. **빌보드 UI**: 월드 공간에 표시되는 UI
2. **HUD**: 스크린 공간 UI
3. **데미지 플로터**: 데미지 표시 UI

---

### 4.7 PGAI (인공지능)

**역할**: 비헤이비어 트리 기반 AI 시스템

**구조**:
```
PGAI/
├── Decorator/ (비헤이비어 트리 데코레이터)
├── Service/ (비헤이비어 트리 서비스)
└── Task/ (비헤이비어 트리 태스크)
```

**특징**:
- UE5 비헤이비어 트리 시스템 활용
- 적 AI 행동 패턴 정의
- AIModule 의존성

---

### 4.8 PGMessage (메시지 시스템)

**역할**: 게임 내 이벤트 및 메시지 전달 시스템

**구조**:
```
PGMessage/
└── Manager/ (메시지 관리자)
```

**특징**:
- 이벤트 기반 통신
- 모듈 간 느슨한 결합
- `PGShared`의 메시지 타입 활용

---

## 5. 주요 시스템 아키텍처

### 5.1 캐릭터 시스템

```
APGCharacterBase (기본 캐릭터)
├── Components
│   ├── UPGAbilitySystemComponent (GAS)
│   ├── UPGStatComponent (능력치)
│   ├── UPGPawnCombatComponent (전투)
│   ├── UMotionWarpingComponent (모션 워핑)
│   └── UPGSkillMontageController (스킬 몽타주)
├── FPGSkillHandler (스킬 핸들러)
└── UPGDataAsset_StartUpDataBase (시작 데이터)
```

### 5.2 스킬 시스템

**플로우**:
1. 입력 수신 (`PGInputComponent`)
2. 스킬 핸들러 처리 (`PGSkillHandler`)
3. GAS 어빌리티 활성화 (`PGAbilitySystemComponent`)
4. 스킬 몽타주 재생 (`PGSkillMontageController`)
5. 투사체/범위 공격 생성 (`PGProjectileBase` / `PGAreaOfEffectBase`)
6. 데미지 적용 및 UI 표시 (`PGDamageFloater`)

### 5.3 데이터 플로우

```
DataTable 스캔 (AssetRegistry)
    ↓
PGDataTableManager 캐싱
    ↓
타입 기반 자동 로딩
    ↓
SearchKey 인덱스 검색
    ↓
FTableRowBase 데이터 반환
    ↓
LRU 캐시 관리
```

### 5.4 컴포넌트 기반 아키텍처

**특징**:
- 기능별로 분리된 컴포넌트
- 재사용 가능한 모듈화
- 상속보다 조합 우선

**주요 컴포넌트**:
- **PGPawnExtensionComponentBase**: 모든 Pawn 확장 컴포넌트의 베이스
- **Combat Component**: 전투 로직
- **Stat Component**: 능력치 관리
- **Input Component**: 입력 처리

---

## 6. 프로젝트 맵

**개발용 맵**:
- `DummyDevMap`: 더미 개발 맵
- `EmptyDevMap`: 빈 개발 맵
- `EnemyDevMap`: 적 테스트 맵
- `FeatureDevMap`: 기능 테스트 맵
- `StageDevMap`: 스테이지 진행 테스트 맵

기본 게임·에디터 시작 맵은 `Config/DefaultEngine.ini`의 `RogueArena`다. 2026-10-02 Blender 환경 키트로 임시 바닥·벽·장식 30개를 교체했으며, 원본은 `Tools/Art/RogueEnvironment`, UE 에셋은 `/Game/Environment/RogueArena`에 있다. StageDevMap의 BP/GameMode 설정은 실행 검증 결과와 함께 관리한다.

---

## 7. 주요 기술 스택

### C++ 기능
- **C++20** 표준
- **템플릿 메타프로그래밍** (DataTableManager)
- **Smart Pointers** (TObjectPtr, TSoftObjectPtr)
- **RTTI** (Dynamic Type Discovery)

### 디자인 패턴
- **Subsystem Pattern** (DataTableManager)
- **Component Pattern** (Combat, Stat Components)
- **Object Pooling** (Projectile System)
- **Observer Pattern** (Message System)
- **Handler Pattern** (Skill Handler)

### 최적화
- **지연 로딩** (Soft Object References)
- **LRU 캐싱**
- **오브젝트 풀링**
- **메모리 사용량 모니터링**

---

## 8. 코드 컨벤션

### 네이밍
- **클래스**: `PG` 접두사 (예: `APGCharacterBase`)
- **인터페이스**: `I` 접두사
- **열거형**: `EPG` 접두사
- **구조체**: `FPG` 접두사
- **위젯**: `PGUI` 접두사

### 파일 구조
- **헤더 파일 공개**: PublicIncludePaths 활용
- **모듈별 분리**: 기능 단위 모듈화
- **디렉토리 계층**: 역할별 명확한 분류

---

## 9. 개발 시 주의사항

### 데이터 테이블
- **SearchKey 메타데이터** 필수: `UPROPERTY(meta=(SearchKey))`
- **정수 타입 SearchKey**: int32, int64 등
- **FTableRowBase 상속**: 모든 데이터 행

### GAS 사용
- `UPGAbilitySystemComponent`를 통해 접근
- `PossessedBy`에서 초기화
- 네트워크 복제 고려

### 메모리 관리
- 데이터 테이블 자동 언로드 활용
- 대용량 에셋은 TSoftObjectPtr 사용
- 캐시 크기 모니터링

---

## 10. 확장 가능성

### 새로운 캐릭터 추가
1. `APGCharacterBase` 상속
2. 전용 Combat/Stat Component 구현
3. 데이터 테이블에 캐릭터 정보 추가
4. 스킬 핸들러 구현

### 새로운 스킬 추가
1. 스킬 데이터 테이블 추가
2. GameplayAbility 구현 (필요시)
3. 애니메이션 몽타주 추가
4. 투사체/범위 공격 구현

### 새로운 데이터 타입 추가
1. `FTableRowBase` 상속 구조체 정의
2. SearchKey 메타데이터 지정
3. DataTable 폴더에 에셋 생성
4. PGDataTableManager::FindSearchKeyProperty의 native SearchKey 등록과 패키지 조회 확인 후 자동 스캔 및 로딩

---

## 11. 빌드 설정

**Target 파일**:
- `UPlayground.Target.cs`: 게임 빌드
- `UPlaygroundEditor.Target.cs`: 에디터 빌드

**빌드 구성**:
- PCH 사용: UseExplicitOrSharedPCHs
- C++ 표준: C++20
- 디버그 심볼: DebugGame 구성

---

## 12. 버전 관리

**Git 저장소**:
- 메인 프로젝트: `.git`
- Content 서브모듈: `Content/.git`

**주의**:
- Content 폴더는 별도 저장소
- 바이너리 파일 제외 (.gitignore)
- DerivedDataCache 제외

---

## 요약

**UPlayground**는 GAS 기반의 모듈화된 액션 게임 프로젝트입니다.

**핵심 특징**:
- 8개 모듈로 구성된 확장 가능한 아키텍처
- 지연 로딩 및 LRU 캐싱으로 최적화된 데이터 관리
- 컴포넌트 기반 캐릭터 시스템
- 오브젝트 풀링 기반 투사체 시스템
- 타입 안정성이 보장되는 템플릿 API

**개발 방향**:
- 모듈 간 느슨한 결합
- 재사용 가능한 컴포넌트
- 메모리 효율적인 데이터 관리
- 확장 용이한 구조

이 문서를 참고하여 프로젝트의 전체 구조와 각 시스템의 역할을 빠르게 이해할 수 있습니다.
## 2026-09-19 1·2단계 반영

- 전투 수치 권위는 `UPGAtrributeSet`과 `UPGAbilitySystemComponent`다. `UPGStatComponent`는 초기 데이터와 호환 조회를 연결한다. 피해·회복·스탯 보상은 GameplayEffect로 확정하고 무기 보너스는 교체 가능한 효과로 관리한다.
- 기본 조작은 `PGQuarterViewData`를 사용하는 고정 쿼터뷰, WASD 이동과 지면 조준이다. `PGCombatFeedbackData`와 `PGCombatTuningData`에서 기본 피드백과 피해 수치를 조정한다.
- `APGStageManager`는 유한 배치 스폰, 중복 처치 방지, 토큰 기반 1회 보상 지급, 실패/완료 상태를 관리한다. 현재 보상은 런 내 Stat 보너스이며 파밍 인벤토리·저장은 후속 개발이다.
- 플레이 검증 맵은 `/Game/Maps/StageDevMap`, 튜닝 에셋은 `/Game/DataCenter/Stage12`다. 상세 구현 범위, 빌드/자동 테스트 결과 및 미완료 수용 기준은 [실행 보고서](Docs/todo/Stage12_Implementation_Report.md)를 따른다.

## 2026-09-19 3·4단계 반영

- `PGData/PGProgressionData`는 아이템·가방·드랍·시작 로드아웃·클리어 해금 빌드의 데이터 원본이다. 실제 프리셋은 `/Game/DataCenter/Progression/DA_PGProgression`이다.
- `PGActor/PGProfileSubsystem`이 GameInstance 수명으로 소유 아이템·장착·빌드·체크포인트·런 보상을 저장한다. `PGLootDrop`은 저장에 성공한 획득만 제거한다. `PGUIInventory`는 I 키로 열고, E 키로 근처 아이템을 획득한다.
- 소유 장비/런 보너스는 ASC의 별도 Profile GE로 적용한다. 기존 무기 액터의 GE와 공존한다. 저장은 두 슬롯을 번갈아 사용하며 실패 시 메모리 상태도 확정하지 않는다.
- StageManager는 StagePresentation 메시지와 지급 콜백만 제공하고 PGUIManager가 보상/재시작 창을 소유한다. PGData는 엔진 ASC/Ability 타입을 참조하며 PGAbilitySystem에 의존하지 않는다. PGShared의 커스텀 GAS include 경로도 제거했다.
- 실제 include 및 모듈 선언 목록은 `Tools/Validation/AuditModuleDependencies.py`, 구현/검증/잔여 수용 기준은 [3·4단계 보고서](Docs/todo/Stage34_Implementation_Report.md)를 따른다. 기존 본문의 의존성 예시는 최초 분석 당시 스냅샷이며 실제 Build.cs가 우선한다.

## 2026-09-20 전투 사이클 표현 구현

- PGUIRewardCard/PGUILootLabel이 보상 카드와 드랍 빌보드 표현을 담당한다. 스테이지의 메시지·토큰 지급 경로는 유지한다.
- PGEnemyAbilityAttack은 TelegraphDuration이 있는 스킬을 예고→단일 원형 판정→회복으로 실행한다. 정예 데이터와 예고 반경은 PGSkillDataRow가 소유한다.
- PGCombatFeedbackData에 일반/강타/치명타 프리셋을 추가했다. 신규 표현 에셋은 /Game/DataCenter/CombatCycle에 있다.
- 기존 데이터 에셋 연결은 실행 중인 에디터의 파일 잠금으로 저장 대기 중이다. 적용 상태와 실행 근거는 Docs/todo/CombatCycle_Implementation_Report.md를 따른다.

## 2026-09-20 메인 HUD

- `PGUIMainHUD`는 네이티브 Slate 기반 인게임 메인 화면이다. `APGPlayerController`가 로컬 플레이어에게 기본 생성하며 `bUseLegacyHUD`로 기존 블루프린트 HUD를 선택할 수 있다.
- GAS 생명력/분노, 스킬 핸들러 쿨다운, Enhanced Input 단축키, 스테이지 상태와 기존 인벤토리를 연결한다.
- ImageGen 원본은 `Tools/Art/MainUI`, 런타임 텍스처는 `/Game/UI/Main`에 있다. 적용·검증·잔여 확인은 [메인 HUD 문서](Docs/todo/MainUI_Implementation_Report.md)를 따른다.


- ?? ??? ??: RewardStatDataRow? Perk/PerkPercent/PlaystyleDescription?? ???????? ??? ????. ASC? ?? ??/??? ??? ?? ??? ????, ProfileSubsystem? ? ?? ?????????? ????. UI? ????? ?? ??? ???. ? ??? ??? ?? ????? ?? ?? ??? ???? ??.

## 2026-09-21 HUD 디자인 개편

- PGUIMainHUD는 금속 프레임을 제거하고 네이비·민트·라벤더의 Slate 벡터 패널과 스킬 카드로 변경했다. 하단 키 안내와 플레이어 발밑 HP 표시를 제거했으며 실제 입력과 GAS/웨이브 연결은 유지한다. 현재 리소스 및 검증은 Docs/todo/MainUI_Implementation_Report.md를 따른다.

## 2026-09-21 스킬 아이콘 재제작

- DT_Skill의 플레이어 모션과 원본 애니메이션 자세를 확인하고 ImageGen으로 아이콘 8종을 제작했다. /Game/UI/SkillIcons에 저장하고 SkillIconPath로 연결한다. 기본공격 3타는 아이콘을 공유한다. 궁극기 115는 몽타주가 비어 있어 임시 콘셉트 아이콘이다. 상세 근거 및 검증은 Docs/todo/SkillIcons_Implementation_Report.md를 따른다.

## 2026-09-21 로그라이크 빌드 MVP

- 기본 실행/에디터 맵은 `/Game/Maps/RogueArena`. 기존 스테이지 지형을 레벨 템플릿 API로 복제하고 카툰풍 석재·민트/라벤더 장식 머티리얼을 제작했다.
- 시작 준비 → 일반 5구간(각 3웨이브) → 보스 1구간. 2·4구간은 강화 2회, 나머지 일반 구간은 1회로 보스 전 7회 선택한다. `bManualReady` 모드에서는 보상 선택 후 준비 완료로 진행한다. 기존 타이머 모드도 유지한다.
- `EPGCombatPerk`와 ASC에 출혈·충격파·격분 계열을 연결했다. 전용 강화 12개와 공용 6개, 전설 3개를 데이터로 구성했다. 피해 확정은 GAS를 통하며 추가 피해는 적중 특성을 재귀 발동하지 않는다.
- `DA_PGProgression.bRoguelikeRuns`가 런 전용 장비·강화 초기화와 별도 `PGProfile_Rogue_*` 저장을 선택한다. 최고 구간·누적 승리·해금은 유지한다. 중단 재개는 체크포인트 구간 시작부터다.
- 범위, 제작 스크립트, 테스트와 밸런싱 한계는 [MVP 구현 보고서](Docs/todo/RoguelikeMVP_Implementation_Report.md)를 따른다.

## 2026-09-30 자동 QA

- `Tools/Validation/RunQA.ps1 -Suite full`은 에디터 빌드, PG 자동 테스트, MVP 에셋 검사, 독립 실행 런 진행, 사망/재시작 20회를 순차 실행한다. `quick`은 빌드·자동 테스트·에셋 검사다.
- 실행마다 `Saved/QA`의 고유 디렉터리에 JSON/Markdown 보고서와 로그를 저장하고 테스트 프로필을 분리한다. 자동 처치 기반 진행 검사는 실제 조작·밸런스·화면 품질 검증과 구분한다.
- 최초 QA에서 빌드·자동 테스트 12개·에셋 검사·재시작 20회는 통과했으나, 독립 RogueArena 실행은 1구간 적 15101 스폰 실패로 중단됐다. 전체 결과는 FAIL이다. 재현 조건, 근거와 남은 범위는 [자동 QA 문서](Docs/todo/16_automated_qa.md)를 따른다.

## 2026-09-30 콘텐츠 0단계 진행

- RogueArena 자동 시작이 내비게이션 초기화 타이머를 취소하던 문제를 수정했다. `APGStageManager`가 웨이브 스폰 전에 빈 Dynamic NavMesh를 준비하고, 스폰 실패 조건을 분리해 기록한다.
- `PGProfileSubsystem`은 런 시드와 Assisted 상태를 저장 트랜잭션으로 관리한다. `PGRunTelemetrySubsystem`은 구간별 실제 피해·전투 시간·확정 보상을 `Saved/RunTelemetry`에 기록한다. 스폰과 보상 난수 스트림은 분리한다.
- 수정 후 전체 QA는 경고 포함 통과: PG 테스트 13개, 16웨이브·7선택·6구간, 사망/재시작 20회. 렌더링 독립 실행과 영어 지정 PIE의 스폰·이동도 확인했다. 기본 한국어 PIE의 엔진 smoke test 오류는 별도 실패로 보존한다.
- 실제 입력 검증은 Computer Use 연결 오류로 미완료이며 적 역할·보스·신규 강화 단계는 아직 구현하지 않았다. 단계별 완료 판정과 근거는 [콘텐츠 구현 계획](Docs/todo/17_content_implementation_plan.md)의 실행 기록을 따른다.

## 2026-09-30 콘텐츠 1단계 진행

- 일반 적 3종·정예 2종에 `PGAI/PGRoleAIController`를 연결했다. 거리 유지/제한된 후퇴, 정면 방어와 회복 시 해제, 돌진 강타, 순차 위험 구역을 데이터로 구분한다. 일반 5구간은 기존 웨이브별 적 수를 유지하며 역할 조합을 변경했다.
- `PGSkillDataRow`의 공통 범위·예고·조준 확정·이동·회복 데이터를 `PGEnemyAbilityAttack`의 판정과 예고 재질에 함께 사용한다. `PGPatternProjectile`은 조준된 사격과 단일 적중을 처리한다. 취소·사망 시 타이머/충돌/예고 정리와 무효 데이터 차단을 추가했다.
- `ConfigureContentMilestone.py -PGContentStep=1`은 해당 ID와 BP/테이블만 백업 후 이관한다. `ValidateContentMilestone.py`는 기존 RunQA의 에셋 검사에 포함된다. `RunContentPresentation.py`는 다섯 역할의 예고/회복을 격리 렌더링하며 직접 입력을 사용하지 않는다.
- 전체 QA는 PG 테스트 15개, 16웨이브·7선택·6구간, 사망/재시작 20회를 통과했다. 직접 조작 수용 검증·밀집 전투 표현은 남아 있다. 보스 코드 초안은 보존했으나 2단계 데이터 전환 및 3단계 신규 강화는 아직 적용하지 않았다. 최신 상태와 근거는 [콘텐츠 구현 계획](Docs/todo/17_content_implementation_plan.md#2026-09-30-1단계-구현-진행)을 따른다.

## 2026-10-01 콘텐츠 2단계 진행

- 황혼의 기사(15106)에 횡베기·돌진 강타·황혼 파동과 HP 50% 기준 2페이즈를 연결했다. `PGEnemyDataRow`는 전환 시간·2페이즈 공격 순서·전환/격파 표현을, `PGSkillDataRow`는 공통 범위·타이밍을 소유한다.
- 보스 상태는 `PGShared`의 스냅샷과 `PGMessage`를 통해 메인 HUD에 전달한다. 공격 중 전환·사망 시 기존 판정을 취소하고, 승리 기록은 즉시 저장한 뒤 격파 표시 시간이 끝나면 결과 창을 연다.
- 보스 전용 이관은 `ConfigureContentMilestone.py -PGContentStep=2`, 렌더링 검사는 `RunBossPresentation.py`다. 일반 적/웨이브의 기존 튜닝을 보존한다. 구현·검증 근거와 미완료 직접 플레이 기준은 [콘텐츠 계획의 2단계 기록](Docs/todo/17_content_implementation_plan.md#2026-10-01-2단계-보스전-구현-진행)을 따른다. 3단계 신규 강화는 아직 미구현이다.

## 2026-10-04 핵 앤 슬래시 스킬 P0

- 기본 100/101/102와 111/112에 `UPGPlayerSkillProfile`을 연결했다. PGData가 타격·이동·취소·포즈 매핑을, PGActor의 `UPGPlayerAttackComponent`가 논리 시계·공간 판정·표현을 소유한다. PGAbilitySystem은 시전 Commit과 기존 GAS 피해 경로, 명시적 강화 발동 정책을 처리한다.
- 프로필 경로는 Notify 피해와 중복 실행하지 않는다. 시전별 공격력 스냅샷과 중복 방지, 충격파·격분·쿨다운 반환 제한, 벽/적 캡슐 이동 제한, 히트스톱과 입력 버퍼/콤보 취소를 적용했다. 프로필이 없는 스킬과 기존 회피는 이전 경로를 유지한다.
- `ConfigureHackSlashP0.py`가 다섯 행과 전용 에셋을 백업 후 이관한다. `RunHackSlashP0.py --render`의 빌드·에셋·PG 테스트 39개·실제 공간 판정·오프스크린 렌더와 기존 공격 회귀 검사는 통과했다. 직접 플레이 비교·모션 및 연출 검수·패키지 성능은 미완료이며 P0 수용 완료로 보지 않는다. [구현 보고서](Docs/todo/HackSlashP0_Implementation_Report.md)에 백업·재현 방법·검증 한계를 기록했다.
- 후속으로 `pg.Skill.Observe` 기반 선택적 시전·직접 피해·입력 시각 관측과 비교 집계를 추가했다. `RunHackSlashComparison.py`는 강화 QA를 기본40회와 분리하고 Development 패키지도 실행한다. 당시 패키지·실제 공간 검사, PG 테스트40개 및 기록 검사22개를 통과했다. 패키지 전후8회와1080p 정지 화면은 확인했으나 직접40회·연속 모션·20분 성능 수용은 미완료다. 이후 요청으로 P1 구현을 진행했으며 최신 검증과 수용 상태는 아래 P1 기록을 따른다. P2는 미착수다.

## 2026-10-04 핵 앤 슬래시 P1 구현·검증

- 110/113/114에 전용 프로필을 연결하고, `APGPlayerSkillProjectile`이 원래 CastId와 스킬 ID를 보존한다. 발사된 검기는 다음 시전·회피와 독립적으로 적중하며 시전자 사망·스테이지 변경·런 종료에 정리된다.
- 5종 중 서로 다른 2종을 안전 단계에서 장착하고 저장 성공 후 적용한다. 신규 저장은 데이터의 111+112를 기본 장착하며 기존 v1 저장의 빈 선택은 이전 프리셋을 보존한다. 쿨다운과 반환은 해제 후에도 스킬 ID를 따른다.
- 현재 Enhanced Input 키, 사용 불가 이유, 프로필 기반 설명, 환급과 격분 회피 준비 표시를 연결했다. 인벤토리는 실제 장착과 적용 예정 선택을 구분한다. `RunHackSlashP1.py`와 `RunHackSlashPackage.py --p1`로 자동 회귀와 패키지 공간 검사를 수행한다.
- 최종 PG 테스트 43개·Python 기록 검사 23개와 최신 Development 패키지의 P0/P1 공간 검사를 통과했다. 113/114 및 장착 화면은 정지 캡처를 확인했으나 110은 최신 캡처 2회에서 타격 표현이 보이지 않아 원인 확인이 남았다. P0 직접 비교 40회, 세 강화 빌드의 무보조 6구간, 연속 모션·청취·입력 검수, 20분 패키지 전투 성능은 미완료다. P2는 미착수로 유지한다. 최종 검증 근거와 완료·잔여 판정은 [P1 구현 보고서](Docs/todo/HackSlashP1_Implementation_Report.md)를 따른다.
