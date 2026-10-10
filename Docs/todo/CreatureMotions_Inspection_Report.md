# 비인간형 몬스터 모션 점검 — 2026-10-09

## 확인 및 수정

- 골렘(15305)의 `BS_Golem`은 Idle/Walk/Run 표본이 있으나 실제 보간 조회가 0개를 반환했다. `sample_data` 지정 후 `ResampleData` 호출이 없었던 것이 원인이다. 해당 에셋만 백업 후 보간 캐시를 복구했다. 생성기에도 재구축과 0/75/150/225/255/300 속도 검사를 추가했다.
- **골렘 전용 모션 교체는 미완료다.** 현재 모델은 `Fantasy_Pack/Characters/Golem/Mesh/SK_Golem`이며 UE4 Mannequin 스켈레톤을 공유한다. 모델 폴더에는 메시/재질/텍스처만 있고 전용 모션은 발견하지 못했다. 기존 이동/피격/사망은 Fantasy Pack 공용 맨손 모션, 공격은 `Anim_Warrior_Attack_5/6`다. 보간 복구는 이 선택을 변경하지 않는다. 사용자가 요구한 모델 전용 원본 위치를 요청했다.
- 스파이더 퀸(15302), 새끼 거미(15301), 리치(15303), 엔트(15304)의 실제 AnimBP 기본값은 각자의 Default/Strafing BlendSpace를 참조한다. 이동 표본은 각 모델의 `Animations` 폴더이며 휴머노이드 모션 오연결은 확인되지 않았다. 공격 생성 원본도 각 모델의 모션이다. 생성기에 출처 경로/보간 검사를 추가했다.

## 검증 근거

- `Saved/CreatureMotions/inventory.json`, `*-defaults.json`, `*-graph.txt`: 실제 저장 에셋 검사. 골렘 수정 후 새 프로세스의 보간 조회는 속도 0/100/255에서 1/2/2개다.
- 백업: `Saved/QA/CreatureBlend_20261009T130836875828/transaction.json`.
- `Saved/CreatureMotions-repair.log`: 6개 속도 보간 검사 통과.
- `Saved/QA/MonsterVariations_20261009T130922761164`: 스파이더 퀸 두 공격의 저장 재로드·GAS 시전·피해·종료 검사와 720p 렌더 2장 통과. 캡처에서 전용 거미 자세를 확인했다.
- `Saved/QA/MonsterVariations_20261009T130955842106`: 골렘 두 공격·피해·종료 검사 통과. 본 위치 변화는 약 113/158cm.
- 최종 출처/보간 검사: `Saved/QA/MonsterVariations_20261009T131135688048`.

## 재현 및 한계

- 읽기 전용 검사: UE Python commandlet으로 `Tools/Validation/InspectCreatureMotions.py` 실행.
- 보간 복구: `RepairCreatureBlend.py` 실행. `-PGCreatureValidate`는 저장 없이 6개 속도만 검사한다. 전체 데이터 재생성 없이 한 에셋만 수정한다.
- 표준 검사: `python Tools/Validation/RunMonsterVariations.py`; 실행/렌더는 `--runtime` 또는 `--render --enemy 15302`.
- 전용 모션 교체, 연속 이동 시 발 미끄러짐, 피격/사망의 시각적 품질 검수는 완료로 간주하지 않는다. 스파이더 퀸에서 사용자가 보았던 이상 동작의 종류를 확인 중이다. C++ 변경이 없어 빌드는 실행하지 않았다.
