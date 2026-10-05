# 플레이어 검기 FX 개선 — 2026-10-05

## Niagara 전환

현재 8개 공격은 실제 Niagara 시스템을 사용한다. 아래의 Plane 셰이더 구현은 초기 단계의 기록이며, Niagara 참조가 없는 프로필의 대체 표현으로 남긴다.

- 근접/회전: `/Game/Art/PlayerCombatFX/NS_PGPlayerSlash`
- 발사형 113: `/Game/Art/PlayerCombatFX/NS_PGPlayerBlade`
- 원본: `/Game/ExternalAssets/VFX/MixedVFX/Particles/Slashes/SeparateParts/Slashes/NS_HolySlash_OnlySlash`. 처음부터 새로 그린 효과가 아니라 프로젝트에 있던 MixedVFX를 복제·개조한 것이다. 원본 파일은 SHA-256으로 불변을 검사한다.
- 검기 메시, 스파크, 연무 3개 이미터를 CPU 로컬 공간 시뮬레이션으로 구성하고 장식 깃털 이미터는 비활성화한다. Niagara 에디터에서 모듈·곡선·렌더러를 직접 편집할 수 있다.
- `PGNiagaraFXTools`는 편집 전용 Python 브리지다. 실제 그래프의 `Color.Scale Color`/`Scale Alpha` 입력을 `User.SlashTint`(Vector3)/`User.SlashAlpha`(Float)에 연결한다. 런타임 편집 모듈 의존성은 없다.
- 근접은 각 타격 페이즈에 인스턴스를 만들고 공격 논리 시계로 `DesiredAge`를 갱신한다. 히트스톱 때 같은 시각을 유지한다. 회전기는 반대편 원호 2개를 재생한다. 발사형은 투사체에 부착하고 원래 시전의 시각/프로필을 유지한다.
- 종료/취소/사망에서는 즉시 비활성화하고 수동 풀로 반환한다. 투사체는 자신의 `EndPlay`에서 반환한다. 풀링이 꺼져 있으면 컴포넌트를 파괴한다. 반환 시 부착과 틱 선행 조건을 제거한다.
- 장착 준비에서 Niagara PSO 사전 준비를 요청한다. 첫 사용의 전체 지연이나 밀집 전투 성능 수용은 별도 측정 대상이다.

### 편집 항목

`PGPlayerSkillProfile.Presentation`의 `SlashTint`, `SlashIntensity`, `SlashDuration`, `SlashHeight`, `bReverseSlash`를 사용한다. `NiagaraReferenceRadius`는 원본 메시 반경 200cm, `NiagaraReferenceDuration`은 원본의 읽기 쉬운 검기 구간 0.24초, `NiagaraRotation`은 원본 축 보정이다. 재생 끝 40%에는 전체 레이어 투명도를 부드럽게 낮춘다. `SlashWidth`는 기존 Plane 대체 표현용이다. Niagara의 원호 모양과 퍼짐은 시스템 에디터에서 조정하며, 공격 판정 각도는 기존 `HitPhases` 데이터로 계산한다.

### 재현과 검증

Development 빌드 후 Unreal 번들 Python으로 `Tools/Validation/RunPlayerSlashFX.py --niagara --apply`를 실행한다. 생성/갱신 시 대상 시스템과 프로필을 `Saved/Backups/PlayerNiagara/<UTC>`에 백업하고 기존 게임플레이·스타일 필드를 대조한다. 검사만 할 때는 `--apply`를 생략하고, 렌더만 반복할 때는 `--niagara --render-only`를 사용한다. 원본 데이터 생성 도구로 프로필을 다시 만든 경우 Niagara 적용 도구를 마지막에 실행한다.

`Saved/QA/20261005T031928Z_9202dd_player_niagara_fx/report.json`은 저장 재로드·PG 자동 테스트 45개·8종 실제 Niagara 시뮬레이션과 GAS 공간 판정·1280×720 SM6 캡처가 PASS다. 이 첫 캡처에서 기본 1타의 본체가 일찍 사라지는 구간을 확인해 재생 구간/페이드와 PSO 준비를 후속 보정했다. 최종 보정 검증은 아래에 별도 기록한다.

최종 보정은 `Saved/QA/20261005T033029Z_cd504d_player_niagara_fx/report.json`의 모든 단계에서 PASS다. 저장 재로드, 기존 게임플레이/스타일 및 원본 불변, 자동 테스트 45개, 8종 실제 재생/공간 적중/종료 후 활성 FX 0개를 확인했다. 8장 PNG를 모두 확인했으며 기본 1타의 본체도 선명하게 표시된다. 이미지는 같은 실행의 `User/Saved/QA/HackSlashP0/Skill_<ID>.png`에 있다.

Development·DebugGame 빌드는 `Saved/Logs/PlayerNiagara_Development.log`와 `PlayerNiagara_DebugGame.log`에서 Succeeded다. 초기 변환 전 바이너리 백업은 `Saved/Backups/PlayerNiagara/20261005T031822001528Z`, 최종 조정 전 백업은 `Saved/Backups/PlayerNiagara/20261005T033045933647Z`다. 초기 검증 실패 기록은 삭제하지 않았다. NullRHI의 렌더 준비 판정, 비동기 컴파일 대기, 색상 구조체 직렬화를 보정했고, 동시 빌드가 만든 이전 오브젝트를 재컴파일한 후 위 최종 결과를 얻었다.

직접 조작의 연속 타격감, 무기 소켓을 추적하는 리본, 패키지 밀집 전투 성능은 이번 자동 검증 범위에 포함하지 않는다. 캡처는 타격 순간 월드를 잠시 정지해 확인한 화면이다.

## 변경 범위

기존 `M_PlayerSlash`의 고정 폭 발광 원호를 진행 방향이 있는 검기로 교체한다. 밝은 칼날 가장자리, 폭이 변하는 내부 면, 분리된 얇은 잔상과 시간에 따른 소멸을 하나의 Additive/Unlit 재질로 표현한다. 별도 텍스처·광원·파티클을 추가하지 않으며 기존 무충돌 Plane 컴포넌트를 재사용한다.

- 기본 100/101/102는 청록 → 청보라 → 보라색 강타로 구분한다. 2타는 반대 방향이며 연타의 타격 페이즈마다 진행 방향이 교대한다.
- 원형 스킬은 서로 반대편에 있는 두 원호가 진행한다. 발사형 113은 검기 형태를 유지하다 사거리/수명 끝부분에서 소멸한다.
- `UPGPlayerSkillProfile`의 Presentation에서 `SlashTint`, `SlashDuration`, `SlashWidth`, `SlashIntensity`, `SlashHeight`, `bReverseSlash`를 조정한다. 근접 검기는 논리 시계를 사용해 공격 배속·히트스톱을 따른다. 취소/종료/사망 정리는 기존 컴포넌트 수명주기를 따른다.
- 검기 머티리얼에 기존 `HalfAngleCos`/`Tint`와 `Progress`, `BladeWidth`, `Intensity`, `Direction`, `Projectile`을 전달한다. 발사형 검기의 움직임/적중/사거리 계산은 그대로다.
- `PGHackSlashProbe`의 첫 타격 캡처 시각을 이전의 하드코딩된 시간 대신 현재 프로필에서 읽는다.
- 장착 준비 단계에서 무충돌 검기 메시를 숨긴 상태로 만들고 재질의 PSO 준비를 요청한다. 시전 시작 시 표현 시각을 초기화해 이전 공격의 검기가 다음 공격 준비 동작에 나타나지 않도록 한다.

## 에셋과 재생성

- 재질: `/Game/DataCenter/HackSlashP0/M_PlayerSlash`
- 프로필: `HackSlashP0`의 100/101/102/111/112 및 `HackSlashP1`의 110/113/114
- 셰이더 원본: `Tools/Art/PlayerSlashFX/PlayerSlash.ush`. Python 제작 시 Custom 노드에 삽입하므로 패키지 런타임에서 이 파일을 읽지 않는다.
- `PlayerSlashMaterial.py`: 공통 재질 제작. 기존 `ConfigureHackSlashP0.py`도 같은 제작 함수를 사용한다.
- `ConfigurePlayerSlashFX.py`: 재질 및 8개 프로필을 백업하고 표현 값만 갱신한다. 피해·범위·타격/모션/이동 시각·쿨다운을 수정하지 않는다.
- `ValidatePlayerSlashFX.py`: 새 프로세스에서 저장된 스타일/재질 참조와 이관 전 게임플레이 필드(중첩 구조체 포함)를 대조한다.
- 백업: `Saved/Backups/PlayerSlashFX/<UTC>`와 `Saved/PlayerSlashFX_LastBackup.txt`.

Development 에디터 빌드 후 Unreal 번들 Python으로 `Tools/Validation/RunPlayerSlashFX.py --apply`를 실행한다. 이후 검증만 반복할 때는 `--apply`를 생략한다. 실행마다 `Saved/QA/<UTC>_player_slash_fx`에 로그/보고서를 남긴다. 실행 중인 이전 DebugGame 에디터는 새 C++ 필드를 로드하지 않으므로 새 빌드로 재실행해야 한다.

## 검증 기록

에셋 이관/저장 재로드는 `Saved/QA/20261005T022809Z_9e608d_player_slash_fx/report.json`에서 PASS다. 백업은 `Saved/Backups/PlayerSlashFX/20261005T022824117040Z`이며, 8개 프로필의 표현 외 필드를 직렬화해 이관 전후 및 새 프로세스에서 일치함을 확인했다.

런타임 C++의 Development·DebugGame 빌드는 `Saved/Logs/PlayerSlashFX_Final_Development.log`와 `PlayerSlashFX_Final_DebugGame.log`에서 모두 Succeeded다. `Saved/QA/20261005T023458Z_84db0f_player_slash_fx/report.json`에서 저장 재로드, PG 자동 테스트 45개(38 성공·7 경고 포함 성공·실패 0), 8종 실제 공간 판정과 SM6 렌더 실행이 PASS다. 이 실행의 기본 1타 PNG에는 검기가 보이지 않아 화면 수용은 보류하고 별도 캡처 검사를 진행했다.

캡처 도구는 `PGHackSlashCapture` 실행에서만 월드를 잠시 정지하고 0.35초 동안 같은 상태를 렌더한 뒤 이미지를 읽으며, 스크린샷 소비 후 재개한다. 이 도구의 양쪽 빌드는 `PlayerSlashFX_Capture_Development.log` / `PlayerSlashFX_Capture_DebugGame.log`에서 Succeeded다. `RunPlayerSlashFX.py --render-only`는 저장 재로드/자동 테스트를 반복하지 않고 이 정지 캡처와 적중 검사를 재실행한다.

- `Saved/QA/20261005T023839Z_08cec5_player_slash_fx/report.json`: 정지 캡처 도구의 P0/P1 실행과 8종 적중 검사는 PASS. P1의 110/113/114 화면과 검기는 확인했다.
- `Saved/QA/20261005T024155Z_7de356_player_slash_fx`: 여러 렌더 프레임을 기다린 P0 캡처와 적중 검사는 PASS. `User/Saved/QA/HackSlashP0/Skill_100.png`에서 기본 1타 검기도 확인했다. 같은 실행의 추가 P1 재캡처는 외부 `Build.bat` 잠금에 대기해 해당 자식 프로세스만 종료했으며, 이 실행의 전체 보고서는 FAIL로 보존한다. P1 근거는 위의 완료된 이전 실행을 사용한다.

이 결과는 저장된 재질/프로필, 실제 GAS 공간 판정, 정지된 타격 상태의 화면 검증이다. 첫 공격을 포함한 연속 플레이의 자연스러움이나 초기 렌더 지연이 완전히 해결됐다는 인증은 아니다.

초기 저장 시도는 기존 DebugGame 에디터의 파일 잠금으로 실패했으며 종료 후 이관했다. 잠금 중 임시 PIE 프리뷰에서는 작은 뷰포트와 첫 캡처 문제, immersive 종료 시 Slate 크래시가 관찰되어 이 프리뷰를 최종 수용 근거로 사용하지 않는다. 최종 도구는 저장된 에셋을 독립 `-game` 프로세스로 렌더하고 종료 코드·셰이더 오류·각 PNG의 1280×720 크기를 검사한다. 캡처 실행의 프레임 시간은 이미지 readback에 영향을 받으므로 성능 측정치로 사용하지 않는다.

## 남은 폴리싱 범위

무기 소켓을 실제로 추적하는 3D 리본, 지면 파편, 별도 스킬 사운드 제작은 남아 있다. 직접 조작에서의 연속 모션 체감과 밀집 전투의 패키지 성능은 별도 검수가 필요하다. Niagara 전환의 현재 범위는 이 문서 상단 기록을 따른다.
