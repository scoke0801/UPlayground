# 로그라이크 액션 파밍 MVP 구현 보고서

## 실행

- UE 5.8에서 `/Game/Maps/RogueArena`를 열고 Play.
- 시작 준비 화면에서 `장비 · 빌드`로 검술을 선택하고 `준비 완료`를 누른다.
- WASD 이동, 마우스 조준, 기존 스킬 입력, I 장비, E 근처 드랍 획득.
- 일반 구간 5개는 각각 3웨이브. 2·4구간은 정예가 있으며 강화 선택을 두 번 한다. 보스 전 총 선택 7회.
- 선택 이후 제한 시간 없이 장비를 정비하고 준비 완료로 다음 구간에 진입한다.
- 6구간 황혼의 기사 처치로 승리. 사망/승리 후 새 도전은 장비·강화·체크포인트를 초기화한다.
- 도중 종료 후 이어하기는 저장된 구간의 시작부터다. 체력·적 위치를 프레임 단위로 복원하는 저장은 아니다.

## 구현 범위

| 단계 | 결과 |
|---|---|
| 런 수명 | 시작 준비, 수동 정비, 6구간 완료, 재도전, 런 전용 저장 네임스페이스 |
| 빌드 | 출혈·출혈 증폭·처치 확산·액티브 폭발 / 충격파·반경·지연 메아리·마무리 / 격분·지속·회복·피해 감소 |
| 보상 | 전용 12개 + 공용 6개, 선행 조건, 선택 횟수 상한, 선택한 계열 후보 가중치 |
| 장비 | 일반·마법 장비, 전설 3개, 장착 효과 교체, 비교·버리기, 시작 무기 |
| 영구 기록 | 최고 구간·누적 승리·누적 구간 클리어. 3회 구간 클리어로 두 번째 검술 해금 |
| 적 | 기존 스켈레톤 리그를 사용한 추격자·사수·철갑, 정예 2종, 단일 페이즈 보스 |
| 표현 | ImageGen 빌드 아이콘 아틀라스, 기존 카툰풍 스킬 아이콘, 신규 석재·민트/라벤더 발광 아레나 머티리얼과 장식 |

지속 피해와 충격파의 추가 피해는 직접 적중 이벤트를 재발동하지 않는다. 출혈·격분 중첩과 발동 빈도에 상한이 있으며, 사망한 피해 원천의 DOT/추가 파동은 중단한다. 효과 수치는 `DA_PGCombatTuning`, 강화는 `DT_StatReward`, 전투 배치는 `DT_StageData`, 장비와 로드아웃은 `DA_PGProgression`에서 조정한다.

기존 저장 슬롯 `PGProfile_*`는 보존하고 신규 모드는 `PGProfile_Rogue_*`를 사용한다. PIE와 자동 검증은 별도 슬롯이다. 에셋 원본은 `Saved/Backups/RoguelikeMVP`에 최초 1회 백업한다.

## 제작 도구

- `Tools/Validation/ConfigureRoguelikeMVP.py`: 빌드·적·장비·스테이지 에셋 구성. 기존 전투 표현 제작기를 먼저 실행하므로 재실행 시 기존 밸런스도 지정 프리셋으로 갱신된다.
- `Tools/Validation/ConfigureRogueArena.py`: 테스트 지형을 복제하여 전용 아레나와 머티리얼·장식을 제작.
- `Tools/Validation/ValidateRoguelikeMVP.py`: 저장된 에셋과 빌드 그래프·몽타주·튜닝 참조 검사.
- `Tools/Validation/PreviewRoguelikeMVP.py`: 격리 PIE에서 6구간 진행·보상 UI 캡처.
- `Tools/Art/RoguelikeMVP/T_BuildAtlas.png`: 생성한 원본 이미지. 왼쪽부터 출혈·충격파·광전사, UI에서 UV로 분할 표시.
- `Tools/Art/RoguelikeMVP/Preview_Arena.png`, `Preview_Rewards.png`: 실제 렌더링 확인 이미지.

## 검증 근거

- UE 5.8 Development Editor 빌드 통과: `Saved/Logs/RogueFinalBuild.log`.
- 자동 테스트 12개 성공(경고 포함 2개), 실패 0: `Saved/Automation/RoguelikeMVPFinal/index.json`.
- 디스크 에셋 참조 검사 통과: `Saved/RoguelikeMVP/validation.json`, `Saved/Logs/RogueFinalValidate.log`.
- 최종 `RogueArena` 렌더링 PIE에서 16개 웨이브, 7회 보상 선택, 6구간 완료: `Saved/Logs/RogueFinalPreview.log`의 `PGRogue PREVIEW COMPLETE`.
- 카드의 아이콘·설명·3택 표시 확인: `Saved/Screenshots/WindowsEditor/ScreenShot00076.png`.
- 시작 준비·아레나 장식·4개 액션·격분 HUD 최종 확인: `Tools/Art/RoguelikeMVP/Preview_Arena.png`.

## 플레이 품질의 한계

진행 검증은 자동 처치와 체력 보정으로 수행했으며 실제 15~20분 난이도나 세 빌드의 DPS 균형을 증명하지 않는다. 수치는 초기 튜닝이다. 적·보스는 기존 모델/애니메이션을 재사용한 MVP 변형이며, 독자적인 보스 모델·다중 페이즈는 포함하지 않는다. 장비 정리는 기존 버리기를 사용하며 분해 재화·제작 경제는 넣지 않았다. 패키지 배포 빌드와 장시간 성능 검증은 별도다.

## 최종 확인

지형·조명·시작 캐릭터와 4개 스킬 슬롯, 격분 표시를 렌더링에서 확인했다. 최초 복제 실험의 월드 파티션 액터 누락은 `NewLevelFromTemplate` 방식으로 해결했으며 사용하지 않는 실험 맵은 `Saved/Backups/RoguelikeMVP/DiscardedPrototype`으로 옮겼다. 사용자 에디터 종료 후 장비 아이콘 패널까지 저장했다. 최종 아레나 장식은 플레이어 시작점이 아닌 바닥 중심에 정렬했다.

## 2026-10-02 임시 환경 모델 교체

- RogueArena의 바닥 1개, 벽 5개, 받침대·크리스털·띠 장식 각 8개를 Blender에서 제작한 5종 메시로 교체했다. 석재 타일과 중앙 문양, 부벽과 금속 장식이 있는 성벽, 단차 받침대, 크리스털 군집, 수평 룬 장식으로 구성한다. 민트·보라색은 기존 배치 규칙을 유지한다.
- 기존 액터 63개의 식별자와 위치를 보존했다. 바닥·벽의 충돌 지오메트리와 트레이스 정책은 기존 `SM_Cube`에서 복사하고 직렬화 데이터 일치 여부를 검사했다. 거의 같은 위치에 겹친 `SM_Cube20/21` 중 20은 충돌을 유지하고 렌더링만 숨겼다. 표시되는 환경 액터는 29개다.
- 기존 제작기에서 비충돌을 의도했던 24개 장식이 저장된 맵에서는 BlockAll이었던 점을 확인하여 `NoCollision` 프로파일로 저장했다. 띠 장식 8개는 같은 위치에서 수평 룬으로 교체했다. 캐릭터, 전투 데이터, 조명은 변경하지 않았다.
- Blender 원본·FBX 5개·팔레트/메시 명세·제작 스크립트는 `Tools/Art/RogueEnvironment`에, 메시 5개와 머티리얼 11개는 `/Game/Environment/RogueArena`에 있다. UV0는 미터 단위의 평면 투영이며 현재 동적 조명용이다. 베이크용 라이트맵 UV와 개별 LOD는 포함하지 않는다.
- `ReplaceRogueEnvironment.py`는 기본적으로 가져오기와 단위/피벗/충돌 검증을 수행하고, `-PGApplyEnvironment`를 추가하면 원본 맵과 월드 파티션 외부 액터를 백업한 뒤 교체·저장한다. `-PGUseImportedEnvironment`는 FBX 재가져오기를 생략한다. 기존 `ConfigureRogueArena.py`는 새 환경 키트가 연결된 맵의 임시 모델 복원을 차단한다.
- 원본 백업: `Saved/Backups/RogueEnvironment/20261002T141147211849Z`. 교체 내역: `Saved/RogueEnvironment/replacement.json`. `ValidateRogueEnvironment.py` 재로드 검사: 30개 교체, 액터 식별자·충돌 보존, 표시 중인 임시 메시 0개로 PASS(`Saved/RogueEnvironment/validation.json`).
- 렌더링 독립 실행 내비게이션 검사: `Saved/QA/20261002T141323Z_c5ac4fc3_navigation/report.json` PASS. 2·8·16초의 내비게이션 준비/플레이어 투영과 적의 완전한 이동 경로를 확인했다. 체력 보정을 사용한 자동 검사이며 직접 조작 검증은 아니다. 실제 전투 렌더는 `Tools/Art/RogueEnvironment/Preview_Unreal.png`, Blender 전체 조감도는 `Preview_Blender.png`다.
