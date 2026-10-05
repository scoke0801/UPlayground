# 툰 플레이어 선택과 P09 몬스터 확장

## 사용 경로

- RogueArena 실행 → 전투 준비 또는 웨이브 정비 → `I` → `캐릭터` 탭.
- Bokusei / LianLian / Honoka / Hichi / Siuha / Lili / Nenmir를 선택한다. 표시명 Siuha의 원본 폴더는 `Suiha`, Lili는 `lili`다.
- 기존 검술·장비·GAS·피격 판정은 공유한다. 선택은 두 슬롯 저장 트랜잭션에 포함되어 재시작·새 도전에도 유지된다. 기존 v1 저장의 빈 캐릭터 ID는 원래 외형을 유지한다.
- 사망·공격·회피·전투 진행·보상 선택 중에는 기존 안전 단계 정책에 따라 선택을 막는다. 저장 실패 시 이전 선택과 외형을 유지한다.

## 에셋과 확장

- 플레이어 목록: `/Game/DataCenter/Progression/DA_PGProgression.PlayableCharacters`.
- 외형: `/Game/DataCenter/Characters/DA_<이름>`. `Mesh`, `SourceMesh`, `Retargeter`, `MeshTransform`, 머리 축, `EquipmentBones`를 편집한다.
- P09: 같은 폴더의 `BP_PGEnemy_P09_Female`, `BP_PGEnemy_P09_Male`, `BP_PGEnemy_P09_Female_Armor007`, `BP_PGEnemy_P09_Male_Armor007`.
- P09 적 ID는 순서대로 `15201–15204`. `DT_Enemy`, `DT_CharacterStat`, 원본 사망 행이 있으면 `DT_Death`에 연결한다. 기존 추격자 `15101`의 BT/스킬/스탯을 출발점으로 사용한다.
- P09 변형은 블루프린트의 `CharacterAppearance.DefaultAppearance` 및 해당 외형의 `Parts`를 편집한다. 같은 스켈레톤 의상은 Leader Pose, 별도 헤어 리그는 Head 부착과 기준 포즈 역변환을 사용한다.
- 현재 `DT_StageData.Waves.MonsterSpawnInfos`에 P09 4종을 편성했다. 1~2스테이지는 남녀 기본형, 3~5스테이지는 Armor007도 섞는다. 기존 추격자 총 20마리를 대체하며 적 총수·다른 역할·6스테이지 보스는 유지한다. 편성 원본은 `Tools/Validation/P09WaveRoster.py`, 적용·검증은 `RunP09Waves.py --apply`다.

## 런타임 구조

### 2026-10-05 P09 웨이브 편성 검증

| 스테이지 | 편성한 P09 | 대체한 추격자 수 |
|---|---|---:|
| 1 | 남녀 기본형 | 5 |
| 2 | 남녀 기본형 | 3 |
| 3 | 남녀 기본형·Armor007 | 4 |
| 4 | 여성 기본형·남녀 Armor007 | 4 |
| 5 | 남성 기본형·남녀 Armor007 | 4 |
| 6 | 기존 보스 유지 | 0 |

`ConfigureP09Waves.py`는 스테이지 테이블만 수정하며 원본 패키지와 전후 JSON은 `Saved/Backups/P09Waves/20261005T103004473815Z`에 보존했다. 수정 후 P09를 원래 추격자로 합산했을 때 전체 스테이지 JSON이 수정 전과 동일함을 확인했다. 반복 적용도 동일한 편성을 유지한다. P09의 기존 드롭 풀은 `Rogue.Chaser`다.

`RunP09Waves.py` 결과는 `Saved/QA/P09Waves_20261005T103120/report.json`에 있다. 새 프로세스 에셋 재로드·콘텐츠/드롭 데이터 검증 PASS, 실제 스테이지 매니저를 통한 1~6스테이지 첫 웨이브의 36마리 스폰 수/ID 검사 PASS다. P09 4종 모두 컨트롤러와 표시 메시 연결을 확인했다. 각 단계는 실제 저장된 웨이브를 사용하며 검사 중 AI 판단만 멈춘다. 전 웨이브 구성은 정적으로 검사했고, 전 웨이브 처치/보상 진행·렌더링·밀집 전투 성능을 이번 검사에서 재수행한 것은 아니다.

`PGData/PGCharacterAppearance`가 외형 데이터, `PGActor/PGCharacterAppearanceComponent`가 표시 메시·모듈러 부위·툰 동기화·무기 부착을 소유한다. 원래 메시의 AnimBP와 몽타주는 숨긴 상태로 계속 평가하고, 네이티브 `PGAppearanceAnimInstance`가 UE IKRig의 Retarget Pose From Mesh 노드로 표시 포즈를 만든다. 원본 평가 후 표시 메시가 평가되도록 틱 선행 관계를 지정한다.

몬스터와 플레이어 모두 기존 캐릭터 클래스를 사용한다. 메시의 시각적 크기와 별개로 캡슐, 공격 프로필, 논리 시계, 피해, 쿨다운은 기존 전투 경로를 따른다. 장비 소켓은 원본 본→표시 본 매핑과 기준 자세 회전을 이용해 표시 손/등에 연결한다. 교체할 때 기존 무기를 원본 소켓으로 되돌린 뒤 새 외형에 재연결한다.

툰 재질과 Stencil 73은 기존 아트 설정을 재사용한다. 실제 플레이 카메라에 기존 툰 외곽선 블렌더블을 추가한다. 신규 런타임 참조는 이미 쿠킹 대상인 `/Game/DataCenter`에서 시작한다.

P09의 FBX 루트 스케일 100은 `bReconstructScaledTranslations`로 루트→골반 경로의 이동만 부모 스케일 아래의 로컬 단위로 복원한다. FK 자손의 이동 단위와 루트 자체가 골반인 Honoka/Siuha에는 기본 엔진 경로를 사용한다. 같은 스케일 값만 보고 전체 본을 일괄 보정하면 체형이 축소되므로 외형별 설정 및 골반 경로로 한정했다.

## 재생성과 검증

UE 5.8 에디터 Development 빌드 후 엔진 Python으로 실행한다.

```powershell
python Tools/Validation/RunPlayableCharacters.py --step inspect
python Tools/Validation/RunPlayableCharacters.py --step configure
python Tools/Validation/RunPlayableCharacters.py --step validate
python Tools/Validation/RunPlayableCharacters.py --step runtime
python Tools/Validation/RunPlayableCharacters.py --step runtime --render
python Tools/Validation/RunPlayableCharacters.py --step automation
```

`configure`는 현재 `PlayableCharacterPolish.json`을 원본으로 사용하고, 모든 대상 패키지를 `Saved/PlayableCharacters/Runs/<실행>/backup`에 먼저 복사한다. 새 프로세스 재로드·의미 비교와 실패 복구, 편집값 내보내기는 [폴리싱 1차 기록](PlayableToonCharacters_Polishing_Implementation_Report.md)을 따른다. 런타임 검증은 별도 `Characters_` 프로필을 사용하고 일반 사용자 저장을 수정하지 않는다.

`PGCharacterProbe`는 7종 선택, 저장 실패 원자성, 알 수 없는 ID 거부, 공격 중 선택 차단, 실제 GAS 기본 공격의 표시 본 이동, P09 4종의 AI/GAS/표시 포즈를 검사한다. 기존 프로필 자동 테스트에는 캐릭터 ID 직렬화와 새 도전 보존 검사를 추가한다.

## 검증 기록

- Python 문법 검사 및 모듈 의존성 감사 통과.
- UE 5.8 Development Editor 빌드 통과: `Saved/PlayableCharacters/build-proportions.log`. 기존 엔진 deprecated API/도구 체인/순환 의존 경고가 있으며 신규 오류는 없다.
- 새 프로세스 에셋 재로드 PASS: `Runs/20261005T033857_validate` (이하 실행 폴더는 `Saved/PlayableCharacters` 기준).
- 전체 PG 자동 테스트 45개 통과: `Runs/20261005T034037_automation/Automation/index.json` — 성공 38, 경고 동반 성공 7, 실패/미실행 0.
- 최종 실제 게임/렌더 검사 PASS: `Runs/20261005T034302_runtime` — 플레이어 7종 선택·실제 GAS 공격·체형·저장 실패 원자성·공격 중 교체 차단, P09 4종 AI/GAS/포즈 검사 통과. P09 머리–골반 길이는 약 41.2cm로 원래 체형을 유지한다. 초기의 이동량 검사만으로 잡지 못했던 몸통 축소를 수정하고 길이 하한 회귀 검사를 추가했다.
- `CharacterSelection.png`에서 7개 버튼·선택 상태·한국어 안내·레이아웃을 확인했다. 대기/공격 14장과 P09 4장도 같은 폴더에 저장한다. 직접 입력 플레이, 패키지 쿠킹 및 대규모 전투 성능 검증은 이번 결과에 포함하지 않는다.

저장소의 기존 `/Content` 제외 정책은 유지했다. 생성된 `.uasset`은 현재 워크스페이스에 저장되어 있으며, 재생성 도구 4개는 `.gitignore` 예외로 등록했다. 다른 체크아웃에서는 기존 원본 아트와 P09 manifest/configure 기록을 갖춘 뒤 `configure`를 실행해야 한다.

## 품질 범위

손 그립·발 고정·헤어 및 의상 물리의 후속 구조, 구현 순서와 수용 기준은 [캐릭터 폴리싱 설계](PlayableToonCharacters_Polishing_Design.md)를 따른다. 설계 문서 작성은 해당 기능의 구현 완료를 의미하지 않는다.

공유 전투 세트의 FK 리타게팅 연결이다. 캐릭터별 고유 스킬·밸런스, 발 고정 IK, 손가락 그립 미세 조정, 헤어·의상 보조 물리는 별도 폴리싱 범위다. P09는 기존 추격자 역할로 아레나에 편성했으며 신규 전투 역할을 추가한 것은 아니다. 원본·표시 포즈를 함께 평가하므로 다수 P09 동시 전투의 패키지 성능 수용 검사는 별도로 필요하다.
