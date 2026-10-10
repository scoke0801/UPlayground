# 플레이어 공격 VFX 재제작

## 2026-10-10 ExternalAssets 전투 VFX 연결

플레이어 100/101/102/110/111/112/113/114의 24개 접점과 114의 이동 검기에 외부 Niagara 레이어를 추가한다. 기존 접점별 형상과 출혈·충격파·격분 머티리얼 레이어를 함께 사용한다. 피해·판정 범위·모션·이동·쿨다운·일반 피격 피드백은 변경하지 않는다.

| 표현 | 외부 원본 | 프로젝트 조정 |
| --- | --- | --- |
| 횡베기·세로 베기·회전·이동 검기 | `MixedVFX/Particles/Slashes/SeparateParts/Slashes/NS_HolySlash_OnlySlash` | 접점별 회전·크기·반전, 회전 베기 2방향, 깃털·연기 제거, 가산 합성 복제 재질, 색/알파 바인딩 |
| 찌르기 접점 섬광 | `SlashTrail_SoftTofu/Niagara/Basic/NS_Hit_Basic_Once` | 전방 오프셋, 불꽃 12개, 섬광 밝기·크기·속도·수명 축소 |
| 지면 충격 파편 | `Niagara/GroundRocks/NS_GroundBurstRocks` | 낮고 작은 형상, 적색 발광 원본 재질 대신 비발광 회색 `M_PGImpactDebris` |

원본 경로는 모두 `/Game/ExternalAssets/VFX` 하위다. 시스템 복제본 3개, 베기 복제 MI와 파편 재질은 `/Game/Art/PlayerCombatFX/External`에 있다. 기존 메시·텍스처·모듈 의존성은 외부 원본을 참조한다. 선택한 원본 시스템 3개와 베기 MI의 SHA-256을 적용 전후·재로드 시 대조한다.

`SlashTrail_SoftTofu` Basic/Wind와 `SwordTrailVFX/NS_Trail_01`도 실제 로드·컴파일·렌더러를 조사했다. 이들은 월드 공간 리본으로 이동 위치 이력이 필요하므로 이번 접점형 효과로 연결하지 않았다. 무기 소켓 연속 궤적은 별도 후속 범위다.

### 실행·조정

- 프로필의 `Presentation|External → ExternalVFX`에서 형상별 시스템, 기준 반경·시간, 스케일·로컬 오프셋·회전·강도·반전을 조절한다. 기본값은 빈 맵으로 기존 프로필을 보존한다.
- `UPGPlayerAttackComponent`가 기존 논리 시계로 재생·히트스톱·취소/사망 정리를 담당한다. 로드아웃에서 미리 로드·컴파일하고 접점에서 동기 로드하지 않는다. Niagara는 수동 반환 풀을 사용한다.
- 검기는 기존 액터에 별도 외부 레이어를 부착하며 시전 종료 이후에도 검기 수명과 벽 충돌·사망·스테이지 종료 정리를 따른다. 외부 설정과 기존 빌드 표현은 시전 스냅샷을 사용한다.
- `pg.Skill.ExternalVFX 0/1`로 이후 생성될 외부 레이어를 끄거나 켠다. 기존 형상·빌드 레이어와 피해는 유지한다. 이미 생성된 파티클은 기존 수명이 끝날 때 정리된다.
- `python Tools/Art/PlayerCombatVFX/RunExternalVFX.py inspect`는 후보 6개의 컴파일·사용자 파라미터·렌더러를 `Saved/QA/ExternalCombatVFX/inspection.json`에 기록한다.
- 같은 도구의 `build`, `apply`, `validate`, `render`는 에디터 빌드, 백업 후 적용, 새 프로세스 검증, 전체 5빌드 렌더·자동 검사를 각각 실행한다. `apply`는 에셋을 로드한 별도 플레이/검사 프로세스가 종료된 상태에서 실행한다.
- 선택 렌더: `RunCombatVFX.py --external --render-only --build 4`. 외부 레이어 없는 비교는 `--external-off`를 사용한다. `ReviewExternalVFX.py <결과 디렉터리> --build 4`로 실제 캡처 접점표를 만든다.

최초 적용 전 프로필 백업은 `Saved/Backups/ExternalCombatVFX/20261010T085103423044Z`이며 최신 성공 백업은 `Saved/ExternalCombatVFX_LastBackup.txt`에 기록한다. 실패한 적용의 백업·로그도 보존한다. 복구 시 해당 백업의 `DataCenter` 프로필을 되돌리면 기존 전용 VFX만 사용한다.

### 검증과 한계

- 후보 6개 UE 5.8 로드·Niagara 컴파일 성공. Development 에디터 빌드 성공.
- 새 프로세스의 프로필 8개·시스템 3개 재로드, 형상별 전체 변환·색 강도·수명 매핑, 원본 시스템 및 기존 전투/연출 값 보존 PASS.
- PG 자동 테스트 59개 PASS. `PG.HackSlash.ExternalVFXProfile`은 0 반경·NaN 변환 거부, 시전 스냅샷 및 접점·피해 보존, 외부 레이어 없는 기존 데이터 호환성을 확인한다.
- 첫 렌더에서 검은 베기 면과 붉은 파편 과발광을 발견하여 수정했다. 연기 제거 외에 베기 MI의 블렌딩도 가산 합성으로 바꿨다. 추가 재로드 검사로 Python Rotator 인자 순서 문제를 발견했고 명명 인자로 회전축을 확정했다. 별도 던전 검사 프로세스의 파일 잠금으로 실패한 저장은 프로세스 종료 후 재시도했다.
- 새 베기 MI가 첫 공격에서만 늦게 준비되는 현상을 실제 캡처로 확인했다. 에디터 로드아웃 준비에서 Niagara가 사용하는 재질의 `EnsureIsComplete`도 수행한다. 기존 흰색 형상만 보이는 캡처를 통과시키지 않도록 외부 청록 베기 영역 전용 픽셀 검사를 추가했다. 정상/누락 캡처에서 각각 4,727/54픽셀로 구분되며 통과 기준은 150픽셀 초과다.
- 최종 기본 실행 `Saved/QA/CombatVFX_20261010T090847540692Z/report.json` PASS: 기존/외부 에셋 재로드, PG 59개, 기본 24개 접점의 실제 피해·외부 생성·1280×720 캡처. 첫 시전 외부 베기 2,490픽셀 PASS. 최종 네이티브 빌드는 `Saved/QA/ExternalCombatVFX/build_20261010T090735/build.log`다.
- 혼합 빌드 실행 `Saved/QA/CombatVFX_20261010T091315895343Z/report.json` PASS: 무대상 시전으로 피격 파티클을 분리한 24개 접점·생성·캡처. 첫 기본 공격의 유색 픽셀 442개로 기존 빌드 레이어 표시도 확인했다. 기본/혼합 합계 48장과 두 결과 폴더의 `ExternalVFX_Build0.png`, `ExternalVFX_Build4.png`를 직접 열어 검수했다. 이번에는 개별 출혈/충격파/격분 3개 빌드의 렌더를 다시 실행하지 않았다.
- 최종 에셋 보정은 `Saved/QA/ExternalCombatVFX/apply_20261010T090451/report.json`, 독립 재로드는 `validate_20261010T090531/report.json`이다. 실패한 초기 렌더도 삭제하지 않았으며 최종 결과와 구분한다.
- 직접 조작·연속 모션 체감·밀집 전투 성능 및 패키지 실행은 이번 자동 검사의 수용 범위에 포함하지 않는다. 일반 피격/강타의 기존 분홍·주황 파티클은 별도 계층으로 유지되어 적중 화면에서는 함께 표시된다.

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
