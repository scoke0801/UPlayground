# 플레이어 공격 VFX 재제작

## 변경 범위

기존에는 100/101/102/110/111/112/113/114가 같은 HolySlash 기반 Niagara의 복제본을 공유했다. 새 `/Game/Art/PlayerCombatFX/Authored/M_PGCombatVFX`와 스킬별 MI 8개를 만들고, 프로필의 `bUseAuthoredVFX`로 선택한다. 기존 Niagara 원본·복제본은 보존한다.

새 효과는 횡베기, 직선 찌르기, 기울어진 세로 베기, 양방향 회전 궤적, 확산 충격 고리, 직선 검기의 여섯 형태다. 밝은 중심선·색상 가장자리·가는 보조 궤적을 분석적 셰이더로 그린다. 텍스처 스트리밍이나 외부 HLSL 파일에 대한 런타임 의존성은 없다. `CombatVFX.ush`는 생성 시 머티리얼에 내장된다.

`SwingShapes`는 원본 P_HitPoint로 해석된 **접점 번호**를 따른다. 피해 판정의 Fan/Disc와 연출 형상을 분리한다. 원본 접점 전후의 골반 기준 손 위치를 조사해 원월참의 2번째 접점, 공중 연격의 마지막 접점을 찌르기로 연결했다. 피해 반경·타이밍·배율·이동·몽타주·쿨다운은 변경하지 않았다.

| ID | 접점별 연출 |
| --- | --- |
| 100 | 횡베기 |
| 101 | 역방향 횡베기 |
| 102 | 강한 세로 베기 |
| 110 | 세로 → 횡 → 세로 → 세로 → 찌르기 |
| 111 | 세로 → 횡 → 세로 → 회전 |
| 112 | 세로 → 찌르기 → 회전 → 회전 → 세로 → 지면 충격 |
| 113 | 내려찍기 충격 → 세로 → 횡 → 세로 → 횡 |
| 114 | 시전자 횡베기 + 독립적으로 이동하는 직선 검기 |

## 빌드 반영

시전 시 ASC의 실제 강화 소유와 강화량을 복사한다. 출혈은 붉은 잔광, 충격파는 푸른 압력선, 격분은 금색 추가 궤적을 합성한다. 혼합 빌드는 세 채널을 유지한다. 출혈 위력·충격 범위·격분 잔상 및 활성 격분 스택에 따라 층의 강도가 증가한다. 상위 강화만 있고 기본 계열을 소유하지 않으면 효과를 켜지 않는다.

공유 프로필을 수정하지 않으며 다음 시전은 현재 빌드를 읽고, 이미 발사된 검기는 시전 당시 표현을 보존한다. 근접 효과는 논리 시계를 사용해 히트스톱과 함께 멈추고 취소/사망/종료 시 숨겨진다. 검기는 기존 수명·벽 충돌·사망·스테이지 종료 경로에서 제거된다.

## 수정·재현

- 형태·색·폭·잔광 길이: `Tools/Art/PlayerCombatVFX/styles.json`.
- 셰이더: 같은 폴더의 `CombatVFX.ush`.
- 에디터: `UPGPlayerSkillProfile`의 Presentation / Presentation|Build. `SwingShapes` 밖의 접점은 `DefaultSwingShape`를 사용한다.
- 적용과 검증: Development 에디터를 빌드한 후 `python Tools/Art/PlayerCombatVFX/RunCombatVFX.py --apply --builds`.
- 화면만 재검사: `--render-only`, 특정 빌드만 검사: `--render-only --build 1` (0 기본, 1 출혈, 2 충격, 3 격분, 4 혼합).
- 모션 조사: Unreal Python commandlet으로 `InspectCombatMotion.py` 실행. 출력은 `Saved/QA/CombatVFX_MotionContacts.json`.
- 디버그: `pg.Skill.DebugCast 1`은 접점/형태/빌드 가중치, 2는 프레임별 재질·표시 상태를 기록한다.

매 적용 시 기존 프로필과 생성 대상 재질을 `Saved/Backups/CombatVFX`에 보존한다. 최초 적용 전 백업은 `20261009T100713924116Z`, 마지막 적용 백업 포인터는 `Saved/CombatVFX_LastBackup.txt`다. 에셋 저장 도중 검사 실패도 로그와 백업을 남긴다.

## 검증 상태

- UE 5.8 Development·DebugGame 에디터 빌드 PASS (`Saved/CombatVFX_Build.log`, `Saved/CombatVFX_DebugBuild.log`). 아래 실행 테스트와 렌더는 Development 기준이다.
- 저장 후 새 프로세스 재로드: 프로필 8개, 형태/재질/빌드 옵션 연결 및 게임플레이 필드 보존 PASS.
- PG 자동 테스트 51개 PASS (42 Success, 9 Success with warnings, 실패/미실행 0). 신규 `PG.HackSlash.CombatVFXBuildSnapshot`은 혼합 빌드·공유 에셋 불변·강화 제거·기본 강화 미소유를 검사한다.
- 기본 8종의 24개 접점에서 실제 GAS 공간 적중과 누적 피해 PASS. 출혈/충격파/격분/혼합은 무대상 시전으로 분리해 추가 피해가 기존 공간 기대값에 영향을 주지 않게 했다.
- 기본+4빌드의 120개 1280×720 SM6 렌더 PASS. 첫 시전의 밝은 효과 픽셀 1,851개, 출혈/충격/격분/혼합의 유색 효과 픽셀 1,724/1,114/1,971/607개. 직접 이미지를 열어 형태·방향·빌드 색상을 확인했다.
- 최종 실행: `Saved/QA/CombatVFX_20261009T104016413036Z/report.json`, `pixel-review.json`, `CombatVFX_Comparison.png`. 모션 조사 도구도 저장된 이전 감사 파일 없이 재실행 PASS (`Saved/CombatVFX_Motion.log`).

초기 화면 검사에서 첫 효과 누락을 발견했다. 컴포넌트 PSO 우회만으로 해결되지 않아 임시 클래스를 제거하고, 에디터 로드아웃 준비에 `UMaterialInterface::EnsureIsComplete`를 연결했다. 검증용 조준은 `PGCombatVFXFixedAim`으로 고정해 데스크톱 포인터가 결과를 바꾸지 않게 했다. 런타임 조준에는 영향을 주지 않는다. 이전 실패·중단 실행도 Saved에 남겨 두었다.

고정 시점의 격리 렌더는 직접 조작·연속 모션 감상·밀집 전투 성능 검수를 대신하지 않는다. 패키지 GPU 성능·장시간 플레이는 이번에 검사하지 않았다. 타격 시 발생하는 기존 공통 피격/강타 파티클은 별도 피드백 계층으로 유지했다.
