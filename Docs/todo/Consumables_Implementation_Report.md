# 월빛 회복약 구현 기록

2026-10-05. [소모품 기획](Consumables_Design.md)의 1단계인 공통 회복약과 정비 보급을 적용했다. 체감 난이도와 최종 수치는 직접 플레이로 조정한다.

## 사용과 보급

| 항목 | 적용 내용 |
|---|---|
| 사용 | Q 또는 체력바 옆 회복약 버튼 |
| 효과 | 사용 시점 최대 체력의 40% 즉시 회복, 최대치 초과분 소멸 |
| 수량 | 시작 3개, 성공 시 1개 소비 |
| 대기시간 | 성공부터 게임 내 시간 8초, 일시정지 중 정지 |
| 동작 | 이동·공격·대시·피격 경직 유지, 회복 때문에 행동을 취소하지 않음 |
| 사용 거절 | 체력 최대·사망·수량 0·대기시간·정비/준비·모달 창·입력 차단 |
| 정비 | 일반 구간 클리어 시 체력 최대·회복약 3개·대기시간 해제 |
| 다음 구간 | 정비 중 장비·강화로 증가한 최대 체력을 반영해 다시 완전 회복 |
| 같은 구간의 웨이브 | 수량·체력·대기시간 유지 |

회복약은 장비 가방과 영구 재고에서 분리했다. 기존 구간 시작 체크포인트를 다시 시작할 때 전투 자원도 초기화하며, 프로필 저장이나 장비 갱신 자체는 재보급하지 않는다. 기본 입력은 `Started`를 사용하므로 Q를 대기시간보다 오래 눌러도 자동으로 다음 약을 사용하지 않는다.

## HUD 크기 버그 수정

회복약 사용·거절 안내가 나타날 때 하단 HUD의 원하는 크기가 바뀌던 경로를 제거했다. 전투 HUD는 논리 크기 560×194, 안내 줄은 높이 20을 항상 확보한다. `SScaleBox`는 축소만 허용하고, 버튼 자체는 90×64로 고정한다. 안내가 사라져도 슬롯 높이가 유지된다.

실제 Slate 버튼의 누름·해제 경로를 호출해 거절 안내를 띄우고, 이후 체력 저하·회복·쿨다운·수량 소진·정비·다음 구간 동안 HUD 위치/크기와 버튼 크기를 매 프레임 비교했다. 1280×720 렌더에서 HUD 372.960×129.204, 버튼 59.940×42.624 픽셀을 유지했다. 이 검사는 격리 실행의 자동 입력 검사이며 사용자의 에디터 화면에서 직접 마우스를 조작한 검사는 아니다.

병 아이콘, 키, 수량, 원형 대기시간, 남은 초, 사용 불가 사유를 표시한다. 체력 35% 이하에서 사용 가능할 때 병을 강조하고 한 번 안내한다. 성공 시 Niagara 회복 입자·음향·실제 회복량 표시를 연결했다.

## 편집 위치

- `/Game/DataCenter/Consumables/DA_MoonlightPotion`: 회복 비율, 수량, 대기시간, 정비 정책, 낮은 체력 기준, VFX/SFX와 표시 시간·크기.
- `/Game/DataCenter/Progression/DA_PGProgression.HealingPotion`: 사용할 회복약 정의.
- `/Game/Blueprints/Input/Actions/IA_HealingPotion`: Q 입력. `DA_InputConfig`의 기본 매핑과 네이티브 액션에 연결했다.
- `/Game/Art/Consumables/NS_MoonlightHealing`, `S_MoonlightHealing`: 복제한 회복 입자와 생성한 짧은 음향. 원본 VFX는 유지했다.
- `Tools/Validation/Data/Consumables.json`: 기본 수치 원본. 에디터에서 튜닝한 값을 유지하려면 원본도 갱신한다. `--apply`는 이 값을 다시 적용한다.

`PGPotion 3 0.25`는 개발 빌드에서 체력을 25%, 약을 3개로 맞춘다. 대기시간은 유지하고 도전을 보조 사용으로 기록한다. `PGConsumableProbe`는 `Consumables_`로 시작하는 격리 테스트 프로필에서만 실행한다.

## 검증과 재현

에셋 적용 도구는 Q 중복을 확인하고 기존 패키지를 `Saved/Backups/Consumables`에 백업한다. 다른 프로세스에서 저장 데이터를 다시 읽어 수치·참조·키 연결을 검사한다. 실패한 적용은 실행 프로세스 종료 후 기존 패키지를 복구한다.

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunConsumables.py --apply --render
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Validation/RunConsumables.py --configuration DebugGame --only reload --render
```

| 검사 | 결과와 근거 |
|---|---|
| Development 빌드 | PASS, `Saved/QA/ConsumablesBuild/build5.log` |
| DebugGame 빌드 | PASS, `Saved/QA/ConsumablesBuild/debuggame.log` |
| 에셋 저장·새 프로세스 재로드 | PASS, `Saved/QA/20261005T110327Z_050874_consumables/apply.log`, `reload.log` |
| PG 자동 테스트 | 48개 PASS(40 성공·8 경고 동반 성공), `Saved/QA/20261005T110604Z_48c806_consumables/Automation/index.json` |
| 실제 게임 실행 | PASS, 같은 실행 폴더의 `runtime.log`, `report.json` |
| DebugGame 재로드·실행 | PASS, `Saved/QA/20261005T111033Z_3605c0_consumables/report.json`; 저장 참조·Q 입력·HUD 크기·텔레메트리 재확인 |
| 화면 | `runtime/User/Saved/QA/Consumables/{ready,low_health,healed,empty}.png` 4장 확인 |

회복약 자동 테스트는 실제 GAS 회복, 재진입 방지, 초과 회복, 미소비 조건, 시간/정지, 웨이브 보존, 정비 중복 방지와 다음 구간 최대 체력 반영을 검사한다. 런타임 검사는 실제 Enhanced Input Q, 유지 입력, 공격/대시 지속, 피격 제어 잠금, 인벤토리 일시정지, 프로필 저장, 웨이브/강화 선택/다음 구간, 사망, HUD 크기를 검사한다. 회복 사용 횟수·실제 회복량·초과분·사망 시 잔량/대기시간은 기존 구간 텔레메트리에 추가했다.

최초 검증 중 테스트 월드 컨텍스트 누락과 엔진의 프레임 시간 상한으로 테스트가 실패했다. 월드 컨텍스트를 등록하고 0.1초 프레임 단위로 진행하도록 수정한 뒤 위 최종 검증을 통과했다. 에디터가 기존 입력 패키지를 잠그던 저장 실패는 사용자가 에디터를 닫은 뒤 적용·재로드 성공으로 해소했다.

검증 시 적 스폰을 멈추고 상태를 조성하므로 클리어율·생존 난이도 검증은 아니다. 음향 에셋 연결과 재생 경로는 실행했지만 실제 청취에 따른 음량 조정은 별도다. 웨이브/보스 중 추가 보급, 재생 연고, 수호 향은 구현하지 않았다.
