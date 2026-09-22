# 스킬 아이콘 — 모션 기반 ImageGen 제작

## 조사 범위

`InspectSkillMotions.py`로 현재 DT_Skill, 몽타주 트랙, 의존 에셋과 진행 빌드를 조회했다. 결과는 `Saved/SkillMotionAudit.json`이다. `PreviewSkillMotions.py`는 StageDevMap의 별도 PIE에서 해당 몽타주의 원본 AnimSequence를 캐릭터에 적용하고 20/40/60% 지점을 캡처한다. 맵/캐릭터 에셋을 저장하지 않는다.

모션 프레임은 `Saved/Screenshots/WindowsEditor/SkillMotion_<SkillID>_<index>.png`, 순서와 시각은 `Saved/SkillMotionFrames.json`이다. 9개 애니메이션(기본공격 3타 포함), 27장. 이것은 원본 애니메이션 자세 검증이며 무기 장착, 몽타주 블렌딩, 판정, 루트 모션 이동과 이펙트의 전체 전투 재생 검증은 아니다.

## 아이콘 연결

| ID | 아이콘 | 실제 연결/제작 근거 |
|---|---|---|
| 100 / 101 / 102 | T_Skill_Normal | Warrior_Attack 1/2/3의 지상 검 콤보. 검과 단일 민트 베기 궤적 |
| 110 | T_Skill_Jump | Air_Attack_Combo, 2.33초. 공중에서 몸을 접고 회전하는 자세. 도약 인물과 세로 궤적 |
| 111 | T_Skill_Dash | Attack_Combo_01, 3.03초. 전진 콤보. 앞으로 기울인 인물·검과 코럴 속도선 |
| 112 | T_Skill_Spin | Attack_Combo_02, 3.22초. 몸을 돌리는 공격. 앰버 원형 베기 궤적 |
| 113 | T_Skill_Chain | Attack_Combo_03, 4.05초. 긴 연속 공격. 라일락 다중 교차 궤적 |
| 114 | T_Skill_Aura | 기본공격과 같은 Warrior_Attack_1, SpawnProjectile/SpawnSkillIndicator 참조. 검에서 분리된 청색 검기 |
| 115 | T_Skill_Ultimate | **몽타주 미지정.** 실제 동작을 확정하지 않는 수직 검·광휘의 임시 콘셉트 아이콘 |
| 10000 | T_Skill_Roll | AS_Roll, 1.17초, MotionWarping 참조. 몸을 둥글게 접은 회피 실루엣 |

색상은 아이콘 식별용 아트 디렉션이며 실제 피해 속성을 의미하지 않는다. 일반 점프 20000은 하단 8개 슬롯의 대상이 아니다.

## 제작 및 적용

- 모든 PNG는 ImageGen 생성 원본이다. 외부 게임 에셋을 복사하지 않았다.
- 기본공격 그림을 화풍 기준 이미지로 사용해 나머지 7개를 생성했다. 아이보리 실루엣, 인디고 음영, 굵은 셀 셰이딩, 투명 배경으로 통일한다.
- 원본: `Tools/Art/SkillIcons/T_Skill_*.png`.
- 런타임: `/Game/UI/SkillIcons/T_Skill_*`.
- 가져오기: `ConfigureSkillIcons.py`. UI 압축, sRGB, 최대 런타임 256px. 원본 고해상도 유지.
- 수정은 10개 플레이어 행의 SkillIconPath만 적용한다. 기본공격 3타는 하나의 아이콘을 공유한다. 적 스킬, 모션, 판정, 쿨다운과 빌드 슬롯 순서는 변경하지 않는다.
- 원본 테이블을 `Saved/Backups/SkillIcons/<실행시각>`에 백업하고, 저장 전에 JSON roundtrip 일치 검사를 수행한다.
- 메인 HUD의 아이콘 내부 여백을 줄여 기존 슬롯 크기에서 그림이 더 크게 표시되도록 한다.
## 검증 결과

- UE 5.8 Development Editor 빌드 성공: Saved/Logs/SkillIconHUDBuild.log.
- 아이콘 8종 가져오기 및 플레이어 10개 행 갱신 성공. JSON 전체 비교로 아이콘 경로 외 데이터 보존 확인: Saved/SkillIconImport.json, Saved/Logs/SkillIconImport.log.
- 1920×1080 실제 StageDevMap HUD에서 8종 아이콘, 투명 배경, 빌드 순서, 회피 그림을 확인했다: Saved/Screenshots/WindowsEditor/PGMainHUD00004.png. 실행 로그: Saved/Logs/SkillIconPreview.log.
- Content는 별도 저장소이므로 DataCenter/DataTables/Skill/DT_Skill.uasset와 UI/SkillIcons의 8개 uasset을 함께 관리해야 한다.
- 궁극기 모션 미지정은 기존 상태이며 이번 작업에서 임의로 채우지 않았다.
