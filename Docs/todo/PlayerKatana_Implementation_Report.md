# 플레이어 기본 무기 카타나 01

## 적용

- 출처: Unity 프로젝트 `CombatGirlsCharacterPack/School_Katana_Girl/Prefab/Prefab_Parts/Weapon`의 카타나 01 프리팹 3종.
- 실제 메시: `Models/Parts/Weapon/Weapon_Katana01.fbx` (GUID `abd37ddcabfb2464e9f8ce65b1ce4124`).
- 실제 텍스처: `Texture/Weapon/Katana_01.png` (GUID `4385987ef8068464c85aee8fcc38275d`). 원본 FBX·프리팹·재질·텍스처 사본과 SHA256은 `Tools/Art/PlayerKatana/Source`, `manifest.json`에 있다.
- `/Game/Art/PlayerKatana/SM_PG_Katana01_Blade` (575 삼각형), `SM_PG_Katana01_Sheath` (366), `SM_PG_Katana01_All` (941)를 생성했다. 합본은 원본 프리팹의 칼집 위치를 유지한 리소스이며 자동 수납 장비가 아니다.
- 플레이어 `BP_PlayerWeapon_Sword`의 `WeaponMesh`를 칼날로 교체했다. 칼날 길이는 112.07cm, 로컬 Z 범위 -17.98~94.09cm다. 원본 피벗·크기로 기존 손 그립과 연결한다.
- `M_PG_Katana01`은 원본 컬러 텍스처와 거칠기 0.55, 기존 공용 카메라 디더링(CPD 슬롯 7)을 사용한다. 기존 검 전용 머티리얼 오버라이드는 해제한다.
- 무기 ID·어빌리티 데이터·충돌 박스 변환/크기는 적용 전 스냅샷과 대조한다. 기존 공용 검 메시 자체는 수정하지 않는다.

## 재현 및 백업

1. Blender 5.2에서 `Tools/Art/PlayerKatana/PrepareKatana.py` 실행: 원본 복사, 분리 FBX 생성.
2. `python Tools/Validation/RunPlayerKatana.py`: 임포트, 플레이어 BP 저장, 독립 프로세스 재로드, 실장착 그립/동작 캡처.
3. 기존 에셋만 검사하려면 `--verify-only`를 붙인다.

BP 백업은 `Saved/QA/PlayerKatana/Backups/<timestamp>/BP_PlayerWeapon_Sword.uasset`에 있다. 결과·로그·PNG는 `Saved/QA/PlayerKatana`에 저장한다.

## 검증

- UE 5.8.2 임포트 및 새 프로세스 저장 재로드 PASS.
- 플레이어 BP 칼날/재질 참조, 3종 메시 삼각형·크기, 전투 데이터 보존, CPD 7 재질 연결 PASS.
- Bokusei 실장착에서 `PGGripPreview` PASS: 8공격·회피·해제·재장착 11조건, PNG 70장. 런타임 메시가 새 칼날인지 로그로 확인했고, 공격 중 손잡이 텍스처와 손 그립 캡처를 육안 확인했다. 일부 초근접 Idle 방향은 긴 소매에 가려진다.
- 전체 캐릭터 프리뷰도 PASS: `PreviewPlayerKatana.py`로 숲 맵에서 3방향을 캡처했다. `Presentation/Katana_1.png`와 `Katana_2.png`를 육안 확인해 칼날 전체 길이·색상과 손잡이 배치를 확인했다.
- 동작 611회 표본의 최대 손 기준 위치 오차는 0cm, 회전 오차는 0.000002°다. 이는 부착 안정성 검사이며 모든 외형의 손가락 접촉을 보증하는 수치는 아니다.

## 범위

- 손에 장착되는 무기는 칼날이다. 칼집의 허리 부착·발도/납도 연출은 추가하지 않았다.
- 원본 Unity 전용 셰이더 전체를 이식하지 않고 프로젝트 조명과 카메라 페이드에 맞는 재질로 구성했다.
- 다른 외형의 손가락 두께·긴 소매 간섭과 직접 플레이 조작감은 이번 시각 검수 범위에 포함하지 않는다.
- 에셋·도구 폴더는 저장소의 기존 ignore 정책을 따른다.
