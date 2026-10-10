# 휴머노이드 이동 애니메이션 교체 — 2026-10-09

## 적용 내용

- 플레이어 공통 `ABP_LocalPlayer`의 지상 이동을 맨손 조깅/거리·방향 워핑에서 Frank Sword2의 검 대기·8방향 걷기·달리기로 교체했다. 선택 외형 9종과 기본 ElfSelena 전투 본체에 적용된다.
- 기존 공중 상태 머신·Control Rig·UpperBody/FullBody 슬롯을 유지한다. 새 지상 블렌드는 실제 속도와 캐릭터 기준 이동 각도를 받는다. 히트스톱의 0초 프레임에 변위/시간으로 계산한 속도를 사용하지 않는다.
- P09 남녀 일반·숙련·정예 6종과 기존 템플릿 4종을 스켈레톤 Warrior AnimBP에서 분리했다. 일반/숙련은 Sword2, 방패 정예는 Warrior의 8방향 모션을 사용한다. `PGCreatureAnimInstance`는 2D 블렌드에 방향/속도를, 기존 1D 보스·골렘에는 이전과 같은 속도를 전달한다.
- 원본 루트 이동 클립의 거리/시간과 리타겟된 골반 높이 비율로 각 방향 재생률을 보정한다. 게임 이동 속도·공격 접점·피해·AI·웨이브·그립은 바꾸지 않는다.
- 스켈레톤 몬스터와 이미 독립된 카타나 이동을 사용하는 월식의 검성은 유지한다. 엔트·거미·리치·골렘 등 비인간형 전용 모션도 유지한다.

## 에셋과 재현

- 원본 설정: `Tools/Validation/Data/HumanoidLocomotion.json`.
- 생성 에셋: `/Game/DataCenter/HumanoidLocomotion`의 리타겟 모션 51개, `BS_PlayerSword`, `BS_P09_Sword8Way`, `BS_P09_Shield8Way`. 각 블렌드는 정지·걷기·달리기와 ±180도 경계 중복을 포함한 27개 샘플이다.
- `python Tools/Validation/RunHumanoidLocomotion.py --step apply`: 패키지 백업 → 리타겟 → 보간 데이터 재구축 → 그래프/10종 P09 연결 → 새 프로세스 재로드. 적용/재로드 실패 시 해당 실행의 파일 트랜잭션을 복구한다.
- `--step validate`: 최종 저장 참조·51개 모션의 유효 포즈·81개 런타임 샘플 조회·보호 에셋 해시 확인.
- `--step preview`: 격리 PIE에서 9종 플레이어·6종 P09 이동, 전후좌우 표본, 발 움직임·표시 정면 축·1280×720 연속 프레임 검사. 캡처 중에는 조준 갱신과 충돌을 격리하므로 실제 전투 입력 검사는 아래 별도 회귀를 따른다.
- `ConfigurePlayableCharacters.py`와 `ConfigureMonsterVariations.py`도 새 P09 이동을 연결하므로 재생성 시 스켈레톤 이동으로 되돌아가지 않는다.
- Content는 기존 저장소 규칙상 Git 제외다. 실제 `.uasset`은 로컬 Content에 적용했으며 생성 코드·설정은 추적 대상이다.

## 검증 근거

- UE 5.8 Development 및 DebugGame 에디터 빌드 성공.
- 최종 적용/재로드: `Saved/HumanoidLocomotion/20261009T091104347258/apply-report.json`, `validation.json`. 스켈레톤·공격·외형 관련 380개 패키지 해시 유지.
- 이동 공격: `Saved/QA/20261009T091155Z_ce5a62_mobile_combat/report.json`. PG 50개(41 성공, 9 경고 포함 성공, 실패 0), 공간 판정, 콤보, Bokusei/Hwarin/Hichi 15회 실제 이동 공격 통과.
- 몬스터 전투: `Saved/QA/MonsterVariations_20261009T091207667930/report.json`. P09 장비 6종과 몬스터 19회 시전 통과. 기존 골렘 1D 이동도 이 검사에 포함된다.
- 대시: `Saved/QA/20261009T091715Z_6678e0_player_dash/report.json` 통과.
- 최종 이동 렌더: `Saved/HumanoidLocomotion/20261009T091104347258/preview/20261009T092339961446/preview-report.json`과 `preview.json` PASS. 22개 이동 조건·1280×720 이미지 88장·발 움직임·모델 정면 축 일치를 검사했고 프로세스 정상 종료를 확인했다. 플레이어 전후좌우와 P09 검/방패 포즈의 대표 화면을 직접 확인했다. 최신 경로는 `Saved/HumanoidLocomotion/latest-preview.txt`다.

## 백업과 검증 범위

최초 원본은 `Saved/HumanoidLocomotion/20261009T090228544205/backup`에 보존했다. 이후 `20261009T090711195171`, `20261009T091104347258`은 순차 수정의 백업이다. 전체 에셋 복원은 에디터를 종료한 상태에서 트랜잭션을 역순으로 복원한다. 생성 도구는 사용자가 이미 실행 중인 에디터를 강제로 종료하지 않는다.

초기 2D 블렌드는 샘플 목록만 저장하고 런타임 보간 데이터를 생성하지 않아 정지 포즈가 발생했다. `ResampleData` 호출과 실제 샘플 조회 검사를 추가해 해결했다. 프리뷰 중 빈 화면과 종료 코드 오류도 성공으로 처리하지 않고 실패 로그를 보존했다. 최종 프리뷰는 조준 컨트롤러를 격리하고 검사용 이동 범위를 제한해 경기장 벽이 근접 카메라를 가리지 않게 했다. 실제 입력과 제한 없는 이동은 별도 이동 공격·대시 회귀에서 검사했다.

직접 키보드 장시간 플레이, 모든 외형의 의상 관통/경사면 발 접지, 패키징 성능 검수는 별도다. 이동 공격 회귀는 실제 Enhanced Input 주입 경로이며 수동 플레이 수용을 대신하지 않는다.
