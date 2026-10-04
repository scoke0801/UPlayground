# UPlayground 프로젝트 분석 문서

## 2026-10-04 핵 앤 슬래시 P0 전투 비교 도구

- P0 기본 3타·질풍연참·원월참 구현에 이어 `PGSkillScenario`와 `RunHackSlashComparison.py/.ps1`로 M10/M15/RING/E1의 격리 전투를 재현한다. 초기 배치를 시드로 고정하고 이관 전 스킬 행을 런타임에만 복원해 전후 비교한다.
- 피해·피격·처치·스킬 사용과 사망/시간 초과/중단 기록을 보존한다. 4개 시나리오 × 5시드 × 전후 2종의 직접 비교 목록을 만들며, 무입력 점검은 이 목록의 완료로 집계하지 않는다.
- UE 5.8 빌드, 기존 P0 회귀 및 8개 전후 배치 점검을 확인했다. 직접 조작·연속 모션·강화별 비교·패키지 성능 수용은 남아 있다. 범위와 재현은 [P0 구현 보고서](Docs/todo/HackSlashP0_Implementation_Report.md)의 전투 비교 도구 기록을 따른다.

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
- `PGPatternProjectile`: 적의 별빛 조준 사격(15102)은 Blender 임시 모델 `/Game/Art/Projectiles/SM_PG_CrystalBolt`를 사용한다. +X 전방·44×22×22cm·304삼각형·단일 버텍스 컬러 머티리얼이며 기존 Box 충돌과 스킬 수치를 유지한다. 원본/FBX는 `Tools/Art/Projectiles`, 재임포트·연결 검사는 `Tools/Validation/ReplaceProjectileModel.py`, 실제 발사 화면 검사는 `Tools/Validation/PreviewProjectilePIE.py`에 있다. 임포트 후 에디터를 빌드하고 `-PGValidateProjectile`로 연결을 검사한다.

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
