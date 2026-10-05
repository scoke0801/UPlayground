# 스켈레톤 궁수 사격 표현 수정 — 2026-10-05

## 적용 결과

- 사수 15102의 두 스킬 15102/15112를 화살 1발·확산각 0으로 통일했다. 기존 공통 예고 재질은 같은 발사 수 데이터를 사용하므로 두 공격 모두 직선 하나를 표시한다. 15112의 이름은 `별빛 정밀 사격`으로 변경했다.
- `/Game/DataCenter/SkeletonArcher/BP_PGSkeletonArrow`는 `PGPatternProjectile`을 상속하고, 기존 `/Game/ExternalAssets/Characters/Enemies/SkeletonEnemy/Mesh/Weapon/Bow/Arrow/SM_Arrow` 및 원본 재질을 사용한다. 화살의 +Y 전방을 로컬 yaw -90°로 비행 +X에 맞춘다. 원래 메시 크기, 기존 충돌 폭과 이동 속도는 유지한다.
- `/Game/DataCenter/SkeletonArcher/AM_PGSkeletonArcherShot`은 기존 `AM_Skeleton_Bow_Attack`을 복제해 연결했다. 이 몽타주의 몸체 사격 시퀀스는 `Anim_Attack`이다. 이름이 비슷한 `Anim_Bow_Attack`은 활 무기 자체의 별도 스켈레톤용이므로 몸체에 연결하지 않는다.
- 원본 발사 Notify 시각 약 0.588486초를 읽어 `ImpactMontageFraction` 약 0.44136432를 계산한다. 기존 패턴의 준비·회복 시간에 맞춰 사격 포즈를 재생한다. 복제 몽타주의 예고/발사 Notify는 제거해 패턴 타이머와 중복 실행되지 않게 했다. 원본 몽타주와 애니메이션은 수정하지 않았다.

## 재현과 보존

Unreal 5.8에 포함된 Python으로 실행한다.

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunSkeletonArcher.py --apply
```

- `ConfigureSkeletonArcher.py`: 수정 전에 대상 패키지와 전체 스킬 JSON을 `Saved/Backups/SkeletonArcher/<시각>`에 보존한다. `-PGArcherValidate`는 저장된 데이터만 검사한다.
- `ProbeSkeletonArcherPIE.py`: 격리 프로필에서 두 스킬을 실제 GAS로 실행한다. 메시/방향/속도/충돌 폭, 몽타주 진행·발사 포즈, 중복 없는 투사체 생성과 공격 종료/투사체 정리를 검사한다. 확인용 카메라와 배치 배우는 저장하지 않는다.
- `CombatVariety.json`과 콘텐츠/전투 패턴 생성 도구에도 단발 설정 및 전용 표현 연결을 반영했다. 재생성 시 공통 맨손 강타와 3발 확산으로 돌아가지 않는다.
- 엔진 설정과 C++ 런타임은 변경하지 않았다. 기존 공통 투사체 클래스의 크리스털 기본값은 다른 사용처를 위해 유지하며, 궁수는 전용 BP를 참조한다.

## 검증 근거

| 검사 | 결과 | 근거 |
|---|---|---|
| 백업·적용 | PASS | `Saved/QA/SkeletonArcher_20261005T094516/apply.log` |
| 별도 프로세스 저장 재로드 | PASS | 같은 폴더 `reload.log` |
| 기존 스킬 데이터 보존 | PASS | `Saved/QA/SkeletonArcherRegression.log`: 53행 비교, 궁수 두 스킬의 허용 필드 이외 동일 |
| 콘텐츠·전투 패턴 데이터 | PASS | 같은 회귀 로그: 6종 적·15스킬·기존 웨이브 검사 |
| 실제 두 사격과 화면 | PASS | `Saved/QA/SkeletonArcher_20261005T095210/report.json`, `presentation.json`, `render.log` |
| Python 구문·수정 diff 공백 | PASS | 변경 Python 8파일 구문 검사 및 대상 파일 `git diff --check` |

최종 렌더 실행에서 두 공격은 각각 투사체 1개를 생성했다. 속도 950/800cm/s, 충돌 반폭 22/18cm를 유지했고, 실제 발사 포즈는 원본 Notify 시각과 일치했다. 두 공격의 조준·비행·화살 근접 화면 6장을 저장했다. 직선 예고선, 활을 당기는 몸체 자세, 기존 화살 촉/축/깃과 전방 방향을 이미지로 확인했다. 일반 비행 캡처에서는 화살이 작아 최종 비행/근접 검사에 시간 감속과 모션 블러 비활성화를 사용했다.

최초 렌더 도구의 Python 카메라 생성 API 오류는 검증 도구에서 수정했다. 당시 실패 기록은 그대로 보존하며 최종 수용 근거는 위 `095210` 실행이다.

검증은 자동 시전 기반의 격리 PIE다. 실제 키보드 조작, 다수 궁수의 밀집 전투 가독성, 활 시위·손가락/발사 원점의 추가 폴리싱은 별도 확인 범위다.
