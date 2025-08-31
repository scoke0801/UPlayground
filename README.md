https://www.notion.so/2465e2a36249804aaf1ec0551054cbdd

## 1. 게임 개요

**장르**: 3D 탑 등반 로그라이크 액션 RPG

**시점**: 3인칭

**플랫폼**: PC (언리얼 엔진 5)

**아트 스타일**: Stylized

### 핵심 컨셉

플레이어는 무한히 높아지는 신비한 탑을 등반하며, 각 층마다 다른 적과 도전 과제를 마주합니다. 죽으면 처음부터 시작하지만, 영구 업그레이드와 새로운 능력을 획득하여 점진적으로 발전합니다.

## 2. 핵심 시스템 설계

### 2.1 플레이어 캐릭터 시스템

- **3인칭 컨트롤러**: 부드러운 이동, 점프, 회피, 공격
- **장비 교체**: 구매한 캐릭터 에셋 기반, 원거리 & 근거리 기능 구현
    - https://www.fab.com/ko/listings/fb5bc083-114e-4e30-a9d5-2828bafb2701

![image.png](attachment:31721c0d-02da-4c89-aff0-e4e795977c6d:image.png)

### 2.2 어빌리티 시스템 (UE5 GAS 활용)

```
GameplayAbilitySystem 기반 구조:
├── Abilities (스킬)
│   ├── Active Skills (액티브 스킬)
│   ├── Passive Skills (패시브 능력)
│   └── Ultimate Skills (궁극기)
├── Attributes (능력치)
│   ├── Health/Mana
│   ├── Damage/Defense
│   └── Speed/CooldownReduction
└── Effects (상태효과)
    ├── Buffs/Debuffs
    ├── DoT Effects
    └── Crowd Control

```

### 2.3 데이터 기반 시스템

**Excel → DataTable 워크플로우:**

- 캐릭터 스탯 (CharacterStatsDataTable)
- 스킬 데이터 (SkillDataTable)
- 아이템 데이터 (ItemDataTable)
- 층별 정보 (FloorDataTable)
- 적 정보 (EnemyDataTable)

## 3. 게임플레이 루프

### 3.1 탑 등반 구조

- **층 구성**: 5개 구역당 1개 보스층 (1-4층: 일반, 5층: 보스)
- **층별 목표**: 모든 적 처치, 퍼즐 해결, 보스 격파
- **진행 방식**: 층별 클리어 → 다음 층 해금

### 3.2 로그라이크 요소

- **사망 시**: 탑 1층으로 리스폰
- **영구 진행**: 메타 화폐로 영구 업그레이드 구매
- **랜덤 요소**: 층 레이아웃, 적 배치, 아이템 드랍

### 3.3 진행 시스템

- **경험치 & 레벨**: 런 동안만 유지되는 임시 성장
- **스킬 트리**: 레벨업 시 선택하는 임시 능력
- **영구 업그레이드**: 메타 화폐로 구매하는 영구 능력

## 4. 기술 아키텍처

### 4.1 모듈 구조

```
Core Systems:
├── PlayerController (입력 처리)
├── GameplayAbilitySystem (스킬/능력)
├── DataManager (데이터테이블 관리)
├── MessageManager (이벤트 관리)
├── FloorManager (층 생성/관리)
└── SaveManager (진행상황 저장)

```

### 4.2 데이터 플로우

```
Excel Files → Import → DataTables → Runtime Access
                                 ↓
MessageManager ← GameSystems → DataManager

```

### 4.3 핵심 컴포넌트

- **AbilitySystemComponent**: GAS 기반 능력 관리
- **InventoryComponent**: 아이템 관리
- **StatsComponent**: 능력치 관리
- **~~FloorProgressComponent**: 층별 진행상황~~

## 5. 아트 & 에셋

### 5.1 캐릭터

- **플레이어**: 구매한 Stylized 캐릭터 사용
- **적**: 비슷한 스타일의 Stylized 에셋 확보
- **애니메이션**: 전투, 이동, 스킬 사용 애니메이션

### 5.2 환경

- **탑 구조**: 모듈식 환경 에셋으로 층별 다양성
- **조명**: 층별 분위기에 맞는 라이팅
- **파티클**: 스킬 이펙트, 환경 효과
