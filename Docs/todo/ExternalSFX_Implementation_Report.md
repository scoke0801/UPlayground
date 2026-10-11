# 신규 SFX 교체 기록

2026-10-10 적용 완료. 제공된 48 kHz 모노 24비트 WAV 51개를 /Game/ExternalAssets/Sound/SFX에 임포트했다. 기존 전투·드랍·가디언·회복·UI·던전 문 SoundWave 16개의 내부 음원을 신규 원본으로 교체하고, 플레이어 스킬 프로필 11개에 휘두르기음을 따로 연결했다.

기존 의미별 패키지 이름은 데이터·블루프린트 참조를 보존하기 위해 유지한다. 패키지 안의 임시 음원은 교체되었고, Tools/Art의 임시 WAV 16개와 합성 생성 코드는 제거했다. 복구용 원본은 Saved/Backups에만 남겨 게임 콘텐츠에서 제외했다. 실행 중이던 에디터의 파일 잠금은 에디터 종료 후 해소되었다.

## 데이터와 재현

- 매핑 원본: Tools/Validation/Data/ExternalSFX.json. replacements는 의미별 16개, player_swings는 스킬 ID별 11개다.
- 적용: UnrealEditor-Cmd.exe UPlayground.uproject -run=pythonscript -script=<프로젝트 절대 경로>/Tools/Validation/ConfigureExternalSFX.py -nullrhi -unattended -nop4 -nosound -EnablePlugins=PythonScriptPlugin -DisablePlugins=RiderLink -ddc=InstalledNoZenLocalFallback
- 새 프로세스 읽기 검증: 동일 명령에 -PGSFXValidate 추가.
- UI 에셋 검사: ValidateUISFX.py를 Python commandlet으로 실행. 실제 오디오 출력 검사: Unreal 번들 Python으로 Tools/Validation/RunUISFX.py 실행.
- ConfigureCombatCycleAssets, ConfigureGuardianPresentation, ConfigureConsumables, ConfigureDungeonP2, BuildUISFX의 합성 경로를 제거했다. P0/P1/MotionExpansion 저작 도구도 같은 스킬별 SFX 매핑을 사용한다.
- 일반 SFX는 볼륨 .65, 동시 8음·최소 재생 간격 .025초를 적용했다. UI는 전용 버스와 사운드별 1음 제한을 유지한다. 원본 WAV는 변경하지 않았다.

## 교체 목록

| 연결 에셋 | 신규 WAV |
|---|---|
| /Game/DataCenter/CombatCycle/S_Normal | Sword_Hit_Designed_01.wav |
| /Game/DataCenter/CombatCycle/S_Heavy | Hit_Flesh_Heavy_01.wav |
| /Game/DataCenter/CombatCycle/S_Critical | Blade_Flesh_Tear_01.wav |
| /Game/DataCenter/CombatCycle/S_Common | Dash_Glove_Short_01.wav |
| /Game/DataCenter/CombatCycle/S_Magic | Blade_Ring_Short_01.wav |
| /Game/DataCenter/CombatCycle/S_Rare | Skill_Ice_Burst_01.wav |
| /Game/Art/Guardian/S_PG_GuardClang | Blade_Parry_Shield_01.wav |
| /Game/Art/Guardian/S_PG_GuardWindup | Sword_Swing_Heavy_03.wav |
| /Game/Art/Guardian/S_PG_GuardLock | Blade_Metal_Hit_01.wav |
| /Game/Art/Guardian/S_PG_GuardRecover | Dash_Strap_Short_01.wav |
| /Game/Art/Guardian/S_PG_GuardSlam | Collision_Metal_Plate_01.wav |
| /Game/Art/Consumables/S_MoonlightHealing | Skill_Air_Burst_01.wav |
| /Game/DataCenter/Audio/UI/S_PG_UI_RewardOpen | Dash_Air_Short_01.wav |
| /Game/DataCenter/Audio/UI/S_PG_UI_RewardConfirm | Blade_Ring_Short_01.wav |
| /Game/DataCenter/Audio/UI/S_PG_UI_Victory | Skill_Ice_Burst_01.wav |
| /Game/Dungeons/ForestSanctuary/Audio/PG_DungeonSeal | Collision_Rock_Heavy_01.wav |

## 검증 결과

- 적용 및 새 프로세스 재로드 종료 코드 0. 신규 에셋 51개, 의미별 에셋 16개, 스킬 프로필 11개 PASS.
- 전체 프로젝트 소유 SoundWave 목록은 매핑의 16개와 일치했다. 신규 원본 경로·길이·채널·스킬 참조를 확인했다.
- 원본 51개 모두 비무음·샘플 클리핑 없음. AnalyzeGuardianAudio의 16/24비트 지원과 기존 회귀 검사 6개 PASS.
- UI 버스·설정·동시 발음·쿠킹 참조 검사 PASS.
- 실제 UI 출력 7개(open, confirm, victory, pending, status, burst, paused) 모두 PASS. 재생 파형 상관계수는 0.999998 이상, 클리핑 없음, 대기/상태 채널 무음 확인.
- 초기 UI 검사는 이전 스테레오 기준의 이득 때문에 실패했다. UE 5.8 AudioMixerChannelMaps.cpp → SignalProcessing/Private/ChannelMap.cpp의 Linear 모노 업믹스를 확인해 채널별 .5 계수를 반영하고 검증 프로세스에 Linear를 명시한 뒤 다시 통과했다. 게임의 전역 오디오 설정을 바꾸지 않았다.
- 수정 Python 구문 검사와 git diff --check 통과. C++ 변경이 없어 재빌드는 수행하지 않았다.

## 근거와 복구

- Saved/QA/ExternalSFX/apply.json, validate.json, source-audio.json, removed-temporary-wavs.json
- Saved/QA/UISFX_Assets.json
- Saved/QA/UI_SFX_20261010T125752Z_5fd9ac5c/report.json 및 Audio/*.wav (최종 PASS, 종료 코드 0)
- 에셋 백업: C:/UsingProject/UnrealProject/UPlayground/Saved/Backups/ExternalSFX/20261010T125418835540Z
- 임시 원본 백업: Saved/Backups/ExternalSFX/RemovedTemporaryWavs
- 기존 저작 도구 백업: Saved/Backups/ExternalSFXTools

## 남은 청취 검수

직접 청취와 다수 적 전투에서의 상대 음량·마스킹 검수는 수행하지 않았다. UI는 제공 목록의 공기·금속 링·빙결 버스트를 사용한다. 신규 원본 전체를 임포트했지만 게임에서 51개 모두를 동시에 사용하도록 기능을 추가한 것은 아니다. 저장된 데이터 연결과 UI 실제 출력 검증 범위에서 적용을 완료했다.
