# 보상·결과 UI 전용 SFX — 2026-10-03

보상 열기·선택 확정·승리에 사용할 오리지널 SFX 3종을 제작하고 실제 UI 슬롯에 연결했다. 외부 녹음·샘플·음악을 사용하지 않은 결정적 절차 합성이며, 표준 Python만으로 재생성할 수 있다.

## 제작물

| 에셋 | 길이 | 표현 | 원본 피크 / RMS |
|---|---:|---|---:|
| `S_PG_UI_RewardOpen` | 0.82초 | 부드러운 금속·유리 질감의 D–A–D 상승음. 카드 간격과 같은 70ms 간격 | −8 / −21.43 dBFS |
| `S_PG_UI_RewardConfirm` | 0.32초 | 낮은 몸통과 짧은 금속성 클릭. 선택 입력 수락을 표현 | −8 / −23.77 dBFS |
| `S_PG_UI_Victory` | 2.40초 | D 장조로 해소되는 상승 화음과 짧은 공간 여운 | −6 / −23.00 dBFS |

- 소스: `Tools/Art/UISFX/*.wav`, 48kHz·스테레오·16bit PCM. `manifest.json`에 설명·SHA-256·길이·피크·RMS·DC·모노 합산 손실을 기록한다.
- 미리듣기: `Tools/Art/UISFX/PG_UI_SFX_Preview.wav`. **열기 → 선택 → 승리** 순서이며 각 에셋 볼륨·공통 UI 볼륨·UE Windows 기본 헤드룸 −3dB를 반영했다. 소리 사이에는 무음 간격이 있다.
- 세 원본 모두 클리핑 0, 시작·끝 샘플 0, DC 절댓값 0.00004 미만. 모노 합산 손실은 최대 0.046dB다. 이 수치는 청감 평가나 LUFS 측정이 아니다.

## 런타임 연결

- 런타임 에셋은 `/Game/DataCenter/Audio/UI`의 SoundWave 3개, `SC_PG_UI`, `SMX_PG_UI`다. 기존 `/Game/DataCenter` always-cook 범위 안에 둔다.
- `Config/DefaultGame.ini`의 `PGUIStyleSettings`에 세 소프트 참조를 연결한다. Project Settings → Game → PG UI Style에서 교체할 수 있다.
- `SC_PG_UI.Volume=0.8`; Wave 볼륨은 열기 0.85, 선택 0.9, 승리 0.85다. 2D 재생, 일시정지 중 UI 재생, 환경 리버브·영역 감쇠 제외를 사용한다. 원본에 공간 여운이 포함되어 있다.
- 짧은 효과음은 PCM·Force Inline으로 설정했다. 세 원본 PCM 합계는 약 664KiB이며 첫 재생 스트리밍 대기를 피한다. WAV별 동시 발음은 1개·Prevent New·재트리거 간격 80ms다. 반복 열기가 겹쳐 커지지 않으며 다른 종류의 사운드를 끊지 않는다.
- 일반 보상 창에서 열기, 유효한 선택 입력을 수락할 때 선택음을 재생한다. 선택음은 저장 성공 알림이 아니며 저장 실패 후 재선택도 같은 클릭을 사용한다. 확정 중 중복 입력은 기존 UI 가드로 차단한다.
- 저장 완료 결과에는 승리를 재생한다. 저장 대기와 일반 도전 실패 화면에서는 열기·승리음을 재생하지 않는다. 저장 재시도 성공 후 완료 결과로 바뀌면 승리음이 재생된다. 저장된 결과를 다시 열 때도 승리음을 재생하는 기존 훅의 의미를 유지한다.

## 재생성·검증 도구

1. UE 내장 Python으로 `Tools/Art/UISFX/BuildUISFX.py` 실행.
2. UE Python commandlet으로 `Tools/Validation/ConfigureUISFX.py` 실행. 같은 이름의 기존 패키지는 `Saved/Backups/UISFX/<UTC>`에 복사한 후 갱신한다.
3. 별도 UE Python commandlet으로 `Tools/Validation/ValidateUISFX.py` 실행. 저장된 슬롯·클래스·버스·포맷·길이·볼륨·동시 발음·cook 설정을 읽기 전용으로 검사한다.
4. UE 내장 Python으로 `Tools/Validation/RunUISFX.py` 실행. 실제 네이티브 위젯을 열어 전용 UI 버스를 WAV로 기록하고 원본 파형·볼륨과 비교한다.

`PGRewardProbe audio`는 비 Shipping 및 격리된 `-PGTestProfile=UI_*`에서만 실행된다. 열기·중복 선택 입력·승리·저장 대기·실패 상태·8회 연속 열기·일시정지를 검사한다. 실제 프로필에 보상을 지급하지 않는다. 검증에서는 `-DeterministicAudio`로 UE 오프라인 믹서를 사용하고, 명령줄에서만 백그라운드 볼륨과 무음 구간 렌더링을 허용한다. 게임의 기본 백그라운드 정책과 버스 자동 휴면은 바꾸지 않는다.

## 검증 범위

- UE 5.8 Development Editor 빌드 성공: `Saved/Logs/UISFXBuildFinal-backup-2026.10.03-03.41.05.log`. 이후 녹음 대기 시간만 늘리려던 추가 빌드는 별도로 실행 중인 수호자 장시간 검증의 DLL 점유로 링크가 차단되었다(`UISFXBuildFinal.log`, `UISFXBuildVerified.log`). 해당 시간 변경은 회수했다. 최종 SFX C++ 코드는 성공한 빌드와 같은 1.2초/2.8초 녹음 대기를 사용하며, 아래 검사에서 시작과 여운 전체가 기록되었음을 확인했다. 실행 중인 다른 검증 프로세스는 종료하지 않았다.
- 새 프로세스의 에셋 검사 PASS: `Saved/QA/UISFX_Assets.json`, `Saved/Logs/ValidateUISFXAssets.log`. 세 에셋·설정 슬롯·UI 버스·포맷·동시 발음·cook 경로를 확인했다. 명령릿에는 기존 텍스처 스트리밍 우선순위 경고 1개가 있다.
- 실제 UI 믹서 출력 **7개 모두 PASS**: `Saved/QA/UI_SFX_20261003T035350Z_9b85b22a/report.json`. 같은 폴더의 `Audio/*.wav`가 엔진 녹음 원본이다. 다섯 유음 구간의 원본 대비 파형 상관은 소수 여섯 자리에서 1.000000, gain은 열기/승리 약 0.4813, 선택 약 0.5096으로 에셋·UI 클래스·Windows 헤드룸 설정과 일치했다. 클리핑은 모두 0, 저장 대기/실패는 샘플 전체가 0이었다. 8회 연속 열기는 한 번 열기와 동일 음량, 일시정지 승리는 일반 승리와 동일 음량이었다. 중복 선택 호출도 한 번의 선택음으로 기록되었다.
- 기존 자동화 **25개 통과**(일반 성공 21, 기존 경고 포함 성공 4), 실패 0 및 전체 에셋 검사 PASS: `Saved/QA/20261003T035824Z_f7cc6437/report.json`. `RunQA.ps1 -Suite quick -SkipBuild`로 검사했으며 결과는 `PASS_WITH_WARNINGS`다. 경고는 기존 GameplayCue 경로·테스트 월드 초기화에 관한 것이다.
- 초기 녹음 실패 기록은 보존했다. `UI_SFX_20261003T034236Z_8b9bc507`은 백그라운드 음소거와 버스 휴면으로 무음/누락, `...034505Z_63c67d39`는 백그라운드 음소거, `...034812Z_3cc53c74`는 Windows 기본 −3dB 헤드룸을 누락한 기대 음량으로 실패했다. 검증기의 INI 섹션 구문·무음 구간 렌더링·기대 gain을 수정한 뒤 최종 7개를 통과했다. 실제 게임의 음소거/헤드룸 정책은 바꾸지 않았다.

헤드폰·스피커 직접 청취, 전투/BGM과 함께 듣는 최종 믹스, 패키지 빌드의 실제 오디오 재생은 별도 수용 확인 범위다. 이번 제작 범위는 세 UI 이벤트이며 희귀도별 획득음과 장비 창의 전체 버튼 사운드 세트는 포함하지 않는다.
