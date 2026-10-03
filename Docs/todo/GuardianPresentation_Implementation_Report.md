# 철갑 수호자 디자인 고도화

작성: 2026-10-03. 대상은 철갑 수호자 `15103`과 기존 수호자·사수 조합의 표현 검증이다.

아래는 1차 구현 기록이다. 현재 후속 적용과 최신 검증 결과는 문서 끝의 **전용 모션 보강**을 따른다.

## 적용 내용

- 기존 해골 리그에 전용 방패와 양쪽 견갑을 부착했다. 청회색 철판, 황동 테두리, 상아색 장식, 푸른 균열 문양으로 방어 역할을 구분한다. 방패 212삼각형과 견갑 각 104삼각형으로 총 420삼각형이며, 공통 머티리얼 하나를 사용한다.
- 방어 중 방패를 세우고, 공격 예고 중 문양 색을 바꾸며, 조준 확정과 회복 시작에 짧은 소리를 재생한다. 회복 중 방패를 내리고 문양 발광을 낮춰 반격 시점을 드러낸다.
- 실제 해골 검 공격 애니메이션을 전용 몽타주로 복제하고 예고·판정·회복 시간에 재생 구간을 맞췄다. 피해는 기존 패턴 타이머와 GAS가 확정한다. 몽타주와 원본 시퀀스에 추가 피해를 발생시키는 노티파이가 없는지 검사한다.
- 방어 적중은 작은 불꽃과 금속음, 짧은 히트스톱을 사용한다. 수호자 공격의 범위 이펙트 크기를 줄이고 일반 피격 프리셋을 선택한다. 이 선택은 공격 중 중복 타격을 막는 상태와 분리했다.
- 취소·사망·EndPlay에서 표현 타이머를 정리하고, 사망 후 도착한 예고 호출을 무시한다. 부착물도 본체와 함께 디졸브되고 제거된다.

## 데이터와 코드

| 위치 | 역할 |
|---|---|
| `PGData/DataAsset/Combat/PGEnemyPresentationData.h` | 부착 메시·소켓·방어/회복 자세·색·소리·방어 적중 이펙트 |
| `PGActor/Components/Combat/PGEnemyPresentationComponent` | 상태에 따른 부착물·발광·소리 갱신 |
| `PGEnemyDataRow.Presentation` | 적별 표현 데이터 연결. 현재 15103만 연결 |
| `PGSkillDataRow` | 몽타주 동기화 선택, 판정 위치 비율, 이펙트 크기와 강타 피드백 선택 |
| `/Game/Art/Guardian` | 메시 2종, 공통 머티리얼, 짧은 소리 5종 |
| `/Game/DataCenter/Guardian` | `DA_PGGuardianPresentation`, `AM_PGGuardianSlam` |
| `Tools/Art/Guardian` | Blender 제작 스크립트, 원본 `.blend`, FBX, WAV와 미리보기 |

컴포넌트는 매 프레임 Tick을 사용하지 않는다. 자세 전환·예고 중에만 30Hz 타이머를 사용한다. 부착 메시의 단순 충돌을 제거하고 런타임 충돌·오버랩·내비게이션 영향도 껐다. 동적 머티리얼은 개체별로 생성하므로 대량 전투 성능은 별도 프로파일링이 필요하다.

기존 예고 1.1초, 조준 추적 0.385초, 회복 1.6초, 반격 피해 보너스 35%, 범위 290cm/반각 65도를 유지했다. 적 스탯·드랍 풀·웨이브 데이터도 유지한다. 강타 피드백의 선택만 바뀌며 피해 수치는 바뀌지 않는다.

## 재생성과 검증 도구

1. `BuildGuardianArt.py`를 Blender에서 실행해 원본 메시와 소리를 생성한다.
2. Unreal Python 커맨드릿으로 `ConfigureGuardianPresentation.py`를 실행한다. 기존 패키지를 `Saved/Backups/Guardian/<실행 시각>`에 백업하고 15103의 표현 필드만 갱신한다. 테이블 내용이 같으면 다시 저장하지 않는다. 후속 작업부터는 `ConfigureGuardianMotion.py`도 호출해 전용 모션과 방패 보정을 함께 재생성한다.
3. `RunQA.py --suite quick`으로 빌드·PG 테스트·MVP 에셋을 검사한다. `ValidateGuardianPresentation.py`는 기존 MVP 에셋 검사에 포함된다. 제작 시에만 실행 직전 행과 비교해 다른 데이터 보존을 확인하므로 이후 정상적인 밸런스 작업을 막지 않는다.
4. `RunGuardianPresentation.py`로 렌더링 PIE 검사를 실행한다. 실행별 테스트 프로필과 `Saved/QA/<고유 ID>_guardian` 경로를 사용하며 맵·BP를 저장하지 않는다.

렌더링 검사는 이동/방어/예고/조준 확정/타격/회복/취소/혼합 전투 2장을 기록한다. 이동 장면은 `AddMovementInput`으로 실제 CharacterMovement를 구동하는 자동 검사다. 실제 3구간 3웨이브 데이터의 수호자 2마리·사수 3마리를 별도로 배치해 AI를 실행한다. 실제 스테이지 전체 진행이나 사람이 직접 조작한 플레이 검증은 아니다.

`PGGuardianProbe status|hit|kill`은 개발 빌드의 `-PGTestProfile`과 `PGGuardianPrimary` 태그가 있는 수호자만 대상으로 한다. `hit`은 실제 OnHit/GAS 경로를 사용한다. 렌더링 검사는 체력 보조를 사용하며 Assisted로 기록된다.

## 검증 결과

최종 상태는 빌드 성공, 자동 테스트 23개 통과, 에셋 검사 통과, 렌더링 검사 통과다. 이전 실행과 실패 로그도 `Saved/QA`에 보존한다.

| 검사 | 최종 결과와 근거 |
|---|---|
| UE 5.8.2 Editor Development 빌드 | 성공, 91.93초. [빌드 로그](../../Saved/Guardian/build.log) |
| PG 자동 테스트 + 에셋 | `20261003T021533Z_7bdff04d`, 23개 통과, 에셋 오류 0. [QA 보고서](../../Saved/QA/20261003T021533Z_7bdff04d/report.md). 바로 앞 빌드를 사용해 QA 내부 빌드 단계는 생략 |
| 최종 렌더링 | `20261003T021538Z_b2fb08c0_guardian`, 7장·상태·피해 경로·취소·사망 정리·프로세스 종료 PASS. [결과 JSON](../../Saved/QA/20261003T021538Z_b2fb08c0_guardian/report.json) |

최종 피해 기록은 정면 방어 63.572, 후면 211.905, 회복 286.071이다. 정면 70% 감소와 회복 35% 추가 피해가 유지된다. 자동 테스트에는 표현 생명주기와 작은 피격 프리셋에서도 애니메이션 노티파이의 추가 타격이 차단되는 회귀 검사가 포함된다.

화면 검토: [방어 자세](../../Saved/QA/20261003T021538Z_b2fb08c0_guardian/guard.png), [회복 자세](../../Saved/QA/20261003T021538Z_b2fb08c0_guardian/recovery.png), [혼합 전투](../../Saved/QA/20261003T021538Z_b2fb08c0_guardian/mixed_wave_late.png).

QA 종합 표기는 `PASS_WITH_WARNINGS`다. 기존 GameplayCue 검색 경로, 격리 테스트의 시작 무기, 스트리밍 풀 우선순위, 적 이름표의 직렬화 클래스 경고가 남아 있다. 빌드는 엔진 API 폐기 예정 경고를 포함한다. 동시 진행 중인 UI·인벤토리 파일은 이 작업에서 수정하지 않았다.

- 원본 메시 재임포트 시 생성된 충돌을 제거했다.
- Unreal Python Rotator의 위치 인수 순서 때문에 눕던 방패를 명명 인수로 보정했다. 머티리얼 VertexColor의 이름 없는 RGB 출력 연결을 수정하고 연결 성공 여부를 검사한다.
- `20261003T021222Z_f29baaa0_guardian` 렌더링 검사는 7장 캡처, 정면/후면/회복 피해 배율 0.3/1.0/1.0, 취소와 사망 정리, 정상 프로세스 종료를 통과했다. 앞선 `20261003T020902Z_c49d91e3_guardian`은 상태 관찰 후 종료 코드 오류로 FAIL 처리했으며 성공으로 간주하지 않았다.

## 남은 폴리싱

- 1차의 기존 검 공격 재사용은 아래 후속 작업에서 전용 대기·이동·강타 시퀀스로 교체했다. 손목·손가락과 보행의 무게감은 추가 수작업 폴리싱 여지가 있다.
- 직접 조작으로 정면 방어·측후면 공략·회복 반격의 손맛과 소리의 상대 음량을 확인해야 한다. 세 빌드별 난이도와 밀집 전투, 장시간 GPU 성능, 패키징 실행은 이번 검증에 포함하지 않았다.
- 나머지 몬스터 5종의 전용 실루엣·모션·소리 고도화는 별도 작업이다.

## 2026-10-03 후속: 전용 모션 보강

철갑 수호자 `15103`에 전용 대기·걷기·달리기·강타 시퀀스 4개, BlendSpace 2개와 자식 AnimBP를 연결했다. 런타임 C++ 코드는 변경하지 않았다.

- 대기·이동은 기존 해골 발동작을 보존하고, 방패 팔을 굽혀 몸 앞에 유지한다. 검을 쥔 오른팔도 준비 자세로 정리했다.
- 강타는 안정된 하체 위에 검을 들어 올리기 → 짧은 내려치기 → 몸을 숙인 회복 → 준비 자세 복귀를 구성했다. 타격 지점은 기존 `ImpactMontageFraction=0.48`과 일치한다. 기존 예고 1.1초·회복 1.6초와 GAS 피해 판정을 유지한다.
- 팔 위치와 상체 기울기는 제작 시 두 본 IK로 계산한 뒤 30fps 애니메이션 트랙으로 저장한다. 런타임 IK·Tick·새 타이머는 추가하지 않는다. 새 방패 손 위치에 부착 변환을 보정했다.
- 공유 해골 AnimBP와 원본 애니메이션을 수정하지 않고 `ABP_PGGuardian`의 `DefaultBlendSpace`·`StrafingBlendSpace` 기본값만 지정했다. 다른 적의 AnimBP 연결도 검사한다.

| 위치 | 역할 |
|---|---|
| `Tools/Art/Guardian/GuardianMotion.json` | 손·팔꿈치 기준점, 방패 자세, 강타 키 자세와 제작 샘플링 설정 |
| `Tools/Validation/ConfigureGuardianMotion.py` | 시퀀스 제작, BlendSpace·몽타주·15103 BP 연결, 백업·원본 해시 확인 |
| `Tools/Validation/ValidateGuardianMotion.py` | 저장된 참조, 리그, 노티파이 부재, 반복 경계, 발 접지, 들어 올리기·타격 자세 검사 |
| `/Game/DataCenter/Guardian/AS_PGGuardianIdle`, `Walk`, `Run`, `Slam` | 전용 시퀀스 4개 |
| `/Game/DataCenter/Guardian/BS_PGGuardianDefault`, `BS_PGGuardianStrafing`, `ABP_PGGuardian` | 전용 이동 표현과 기존 상태 그래프 연결 |

모션만 다시 만들 때는 Unreal Python 커맨드릿으로 `ConfigureGuardianMotion.py`를 실행한다. 기존 수호자 아트·표현 에셋이 먼저 있어야 한다. 실행별 백업과 보고서는 `Saved/Guardian/Motion/<실행 시각>`에 보관한다. 일반 에셋 검사는 `ValidateGuardianPresentation.py`를 통해 새 모션 검사도 실행한다.

### 검증 결과

| 검사 | 결과와 근거 |
|---|---|
| UE 5.8.2 Editor Development 빌드·PG 자동 테스트·에셋 | 빌드 PASS, **25개 테스트 성공·실패 0**, 에셋 PASS. 기존 경고 때문에 종합 `PASS_WITH_WARNINGS`. [QA 보고서](../../Saved/QA/20261003T030144Z_d3f25450/report.md) |
| 렌더링 PIE | **9장·상태·피해 경로·취소·사망 정리·정상 종료 PASS**. [결과](../../Saved/QA/20261003T030018Z_a260b03b_guardian/report.json) |
| 기존 명령으로 전체 재생성 | `ConfigureGuardianPresentation.py`에서 새 모션 제작과 검사까지 PASS. 관련 없는 행 보존 검사도 PASS. [로그](../../Saved/Guardian/motion_regeneration.log), [모션 생성 보고서](../../Saved/Guardian/Motion/20261003T030334353346Z/report.json) |

정면/후면/회복 피해는 **63.572 / 211.905 / 286.071**로 기존 결과와 같다. 런타임 손 위치도 기록했다. 조준 확정 시 오른손 높이 약 155cm, 타격 시 약 95cm로 내려가며, 방어 대기에서는 왼손 높이 약 109cm를 유지한다. 원본 클립·공유 AnimBP·적/스킬 테이블의 파일 해시 보존도 모션 생성 시 검사했다.

화면 검토: [방어](../../Saved/QA/20261003T030018Z_a260b03b_guardian/guard.png), [조준 확정](../../Saved/QA/20261003T030018Z_a260b03b_guardian/locked.png), [타격](../../Saved/QA/20261003T030018Z_a260b03b_guardian/impact.png), [회복](../../Saved/QA/20261003T030018Z_a260b03b_guardian/recovery.png), [혼합 전투](../../Saved/QA/20261003T030018Z_a260b03b_guardian/mixed_wave_late.png).

이동 캡처는 스크립트 입력을 사용하며 `assisted=true`, `direct_input=false`, `scripted_movement=true`로 기록한다. 혼합 배치에서는 AI를 활성화해 관찰하지만, 이 검사로 내비게이션 경로·세 빌드 난이도·직접 조작 손맛을 통과 처리하지 않는다. 실제 플레이의 음량·모션 무게감, 장시간 GPU 성능과 패키징 검증은 계속 남아 있다.

## 2026-10-03 후속: 반복 부하·오디오 계측

### 재현 도구와 범위

- `RunGuardianSoak.py` → `ProbeGuardianSoakPIE.py`는 별도 테스트 프로필에서 **기본 조합 5분 → 밀집 조합 10분 → 기본 조합 복귀 5분**을 실시간으로 실행한다. 기본은 수호자 2·사수 3, 밀집은 수호자 20·사수 30이다. 시작마다 실제 3구간 3웨이브의 기본 조합도 대조한다.
- `PGGuardianScenario`는 개발 빌드의 `-PGTestProfile=GuardianSoak_*`에서만 동작한다. 배치 수를 0~10으로 제한하며 전용 태그가 있는 테스트 적만 정리한다. 테스트 적은 드랍하지 않는다. 체력 보조는 기존 `PGStress 0 0`을 사용하며 프로필에 Assisted를 기록한다.
- AI가 기존 GAS로 공격한다. 수호자를 사정거리 안에 배치하고 60초마다 위치를 재설정한다. 플레이어는 공격하지 않는다. 사정거리 밖에서 접근하는 이동은 예비 검사에서 관찰되지 않았으므로 **내비게이션·직접 조작·난이도·실제 웨이브 완주 검사로 간주하지 않는다**.
- 각 단계의 첫 30초를 CSV 측정에서 제외한다. 프레임·Game/Render/GPU 시간, 프로세스 물리 메모리, GPU 로컬 메모리를 저장하고 5초마다 적·방어구·전체 Actor 수와 실제 게임 시간을 기록한다. 종료 시 테스트 적 제거, 프로세스 정상 종료, 수호자 에셋과 데이터 테이블의 SHA-256 보존을 검사한다.
- 각 단계의 준비 구간에서 마스터 출력 15초를 WAV로 녹음한다. 백그라운드 CPU 제한·음소거는 측정 프로세스 안에서만 해제하며 설정·맵·BP를 저장하지 않는다. 녹음 중에만 무음 구간의 서브믹스 자동 비활성화를 막아 연속 PCM을 확보한다. 고해상도 캡처는 2분 예비 실행에서만 사용하고 20분 성능 측정에서는 제외한다.
- `AnalyzeGuardianAudio.py`는 수호자 원본 5개, 기존 전투/드랍 음원 6개와 녹음 파일의 sample peak·RMS·50ms 최대 RMS·클리핑·무음을 검사한다. LUFS, true peak, 청감상 음량·마스킹 평가는 아니다.
- `TestGuardianValidation.py`의 6개 검사는 CSV 메타데이터/NaN 제외, UE 5.8 메모리 열과 단위, 무음, PCM 양쪽 한계값과 알려진 진폭을 확인한다.

재현 명령(저장소 루트, PowerShell):

```powershell
$uePython = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe'
& $uePython Tools/Validation/TestGuardianValidation.py
& $uePython Tools/Validation/RunGuardianSoak.py
# 도구 연결만 확인하는 2분 예비 실행. 20분 검증을 대체하지 않는다.
& $uePython Tools/Validation/RunGuardianSoak.py --seconds 120
```

### 검증 기록

- 추가 C++ 명령을 포함한 Editor Development 빌드, 기존 PG 자동 테스트 **25개**, 에셋 검사 PASS. 기존 경고로 종합 `PASS_WITH_WARNINGS`. [QA 보고서](../../Saved/QA/20261003T034033Z_d498097d/report.md).
- 원본 음원 11개에서 무음·PCM 클리핑 없음. [원본 레벨 보고서](../../Saved/Guardian/audio_source_report.json).
- 예비 실행은 Python 비공개 API, 백그라운드 프레임 제한, 공격 거리와 무음 구간 생략을 확인하고 도구를 보정했다. 실패 실행은 `Saved/QA/*_guardian_soak`에 그대로 남겼다. `20261003T035144Z_dc00f996_guardian_soak`은 2분 AI 반복·상태·정리 검사를 통과했으나, 녹음 길이 검사를 보강하기 전 실행이므로 최종 연속 오디오 근거로 사용하지 않는다.
- 첫 20분 실행 `20261003T035507Z_f391ce61_guardian_soak`은 **FAIL**이다. 복귀 구간에서 약 50초의 프레임 지연, 실제 게임 시간 진행 부족, 15초 요청보다 긴 WAV가 기록됐다. 적과 방어구는 유지되지만 Actor 수는 99→239→154로 남았다. [실패 보고서](../../Saved/QA/20261003T035507Z_f391ce61_guardian_soak/report.json)를 보존하며 장시간 안정성 통과로 간주하지 않는다.
- 정리 코드를 대조해, 적의 사망 디졸브를 거치지 않는 `Destroy()` 경로에서 무기가 남는 문제를 수정했다. `PGPawnCombatComponent::EndPlay`가 등록된 보유 무기와 참조를 정리한다. `PG.Combat.CarriedWeaponOwnerCleanup`은 직접 소유자 제거, 이미 제거된 무기, 보유 참조 해제와 무관한 무기 보존을 확인한다. 수호자 전투 수치와 에셋은 변경하지 않았다.
- 같은 시각의 별도 전투 조작 코드 편집/빌드와 겹친 실패 빌드도 보존했다. 해당 파일은 이 작업에서 편집하지 않았다. 변경 완료 후 무기 정리 회귀 검사를 포함한 [재검증 QA](../../Saved/QA/20261003T041929Z_8ebc5391/report.md)는 빌드·자동 테스트 **28개(실패 0)**·에셋 PASS다. 이 중 새로 추가한 검사는 `PG.Combat.CarriedWeaponOwnerCleanup`이다.
- 수정 후 20분 재측정: `20261003T042312Z_1c4c5c7c_guardian_soak`. **AI 반복·실제 시간 진행·정리·정상 프로세스 종료 PASS, 종합 FAIL(복귀 오디오 녹음 길이 부족)**. 고해상도 캡처를 제외하고 무기 수를 계측했다. 프레임 지연 전체가 무기 잔존 때문에 발생했다고 단정하지 않는다. [최종 보고서](../../Saved/QA/20261003T042312Z_1c4c5c7c_guardian_soak/report.json), [오디오 보고서](../../Saved/QA/20261003T042312Z_1c4c5c7c_guardian_soak/audio.json).

### 최종 계측 결과와 판정

환경: UE 5.8.2 Editor Development PIE, i5-12400F, RTX 3060 Ti 8GB, 드라이버 581.29, DX12. 실행 해상도 인자는 1280×720, ScreenPercentage 100, 주요 품질 변수 3, VSync off, 60fps 상한이다. 에디터·Python 계측과 개발 환경의 백그라운드 작업 영향을 포함하므로 독립 패키지의 성능 보증이나 수정 전후의 정밀 비교로 사용하지 않는다.

총 실제 시간 약 1,200초, 게임 시간 약 1,197.2초, 준비 구간 제외 **51,404프레임**을 기록했다. 수호자 강타 시작 관찰은 기본/밀집/복귀 순서로 147/1,460/146회다. 수호자·사수의 기존 AI/GAS 공격을 사용했지만 이동 관찰은 0회이며 경로 탐색은 검증하지 않았다.

| 단계 | Frame p95 / p99 | GameThread p95 | GPU p95 | 무기 수 |
|---|---:|---:|---:|---:|
| 기본 5분, 적 5 | 20.920 / 34.248ms | 20.127ms | 8.713ms | 초기 생성 완료 후 6 |
| 밀집 10분, 적 50 | 34.794 / 87.007ms | 34.618ms | 9.455ms | 51 |
| 복귀 5분, 적 5 | 25.770 / 49.904ms | 25.393ms | 8.504ms | 6 |

- **제안 60fps 기준(p95 16.7ms)은 미달**이다. 밀집 시 GameThread 시간이 GPU 시간보다 크지만, 구체적인 병목 확정에는 독립 패키지/Insights 측정이 필요하다. CSV의 RenderThreadTime은 대부분 0에 가까워 렌더 스레드 비용이 없다는 근거로 해석하지 않는다. 최대 프레임 지연도 678/771/875ms가 남아 있어 상용 성능 통과로 표기하지 않는다.
- 무기는 **6→51→6→종료 후 1(플레이어)**로 정리됐다. 최종 테스트 적은 0개다. 밀집 Actor 수는 234, 복귀 후는 99~101개로 첫 실행의 잔존 55개가 사라졌다. 사망 디졸브가 아닌 직접 제거 경로도 검사했다.
- 프로세스 물리 메모리의 처음/마지막 300프레임 평균은 약 **3,924.8→3,998.5MB(+73.7MB)**다. 캐시·할당자·에디터 영향을 분리하지 않았으므로 메모리 완전 복귀나 누수 부재를 판정하지 않는다. GPU 로컬 메모리와 단계별 원자료는 JSON/CSV에 남겼다.
- 수호자 에셋 및 데이터 테이블 **33개 파일의 실행 전후 SHA-256이 동일**하다. 기존 전투 데이터는 유지했다.

| 엔진 마스터 녹음 | 파일 길이 | Sample peak | 판정 |
|---|---:|---:|---|
| [기본](../../Saved/QA/20261003T042312Z_1c4c5c7c_guardian_soak/baseline.wav) | 15.019초 | −1.103 dBFS | 길이·무음·PCM 한계값 검사 통과 |
| [밀집](../../Saved/QA/20261003T042312Z_1c4c5c7c_guardian_soak/dense.wav) | 14.997초 | −0.001 dBFS | 길이 통과, 출력 여유 부족 경고 |
| [복귀](../../Saved/QA/20261003T042312Z_1c4c5c7c_guardian_soak/return.wav) | 8.917초 | −0.001 dBFS | **15초 연속 녹음 기준 미달**, 출력 여유 부족 경고 |

녹음된 PCM 샘플에서 무음/양쪽 한계값 클리핑은 없지만, 이것만으로 재생의 연속성·음질을 보장하지 않는다. 특히 복귀 WAV 길이 부족은 아직 원인을 분리하지 못했다. 믹서 캡처와 실제 장치 출력을 대조해야 하며, 소리가 실제로 끊겼거나 단순히 도구 문제였다고 어느 쪽으로도 확정하지 않는다. 음량 수치를 임의로 낮춰 이 문제를 통과 처리하지 않았다.

남은 순서는 오디오 길이 부족의 원인 분리와 동시 재생 음량/청감 확인, 독립 패키지의 CPU·프레임 지연 프로파일링, 아래 직접 조작 수용 검사다.

### 직접 플레이 수용 검사

격리 프로필로 에디터를 실행한 뒤 PIE에서 `PGStartStage 3`으로 실제 3구간을 시작한다. 위 soak 명령의 체력 보조와 자동 재배치를 사용하지 않는 별도 플레이 기록이 필요하다. 기존 WASD 이동·마우스 조준·프로젝트 스킬 입력을 사용한다.

```powershell
$playProfile = 'GuardianFeel_' + [guid]::NewGuid().ToString('N')
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' (Join-Path $PWD 'UPlayground.uproject') "-PGTestProfile=$playProfile" '-PGRunSeed=173001'
```

| 항목 | 직접 확인할 내용 |
|---|---|
| 정면/측후면 | 같은 공격을 각 방향에서 반복해 방어음과 피격 반응이 방어 여부를 읽게 하는지 확인 |
| 예고/회피 | 조준 확정 전후에 방향을 바꾸고 회피하여 표시와 실제 피격 시점의 납득 가능성 확인 |
| 회복 반격 | 내려친 직후 접근해 공격했을 때 자세·피드백으로 반격 창이 분명한지 확인 |
| 손·발·무게감 | 대기↔걷기↔달리기 전환의 발 미끄러짐, 손목/손가락, 방패 관통, 강타 무게감 확인 |
| 청감 | 동일 출력 장치와 시스템 음량에서 단독·혼합 전투의 예고/조준 확정/타격/방어음을 비교하고 듣기 피로·마스킹 기록 |

이 표는 실행 결과가 아닌 남은 수용 검사다. 패키지 성능, 세 빌드의 난이도, 20회 구간 전환 및 타이머/구독 수 누수 검사는 이번 수호자 부하 측정과 별개다.
