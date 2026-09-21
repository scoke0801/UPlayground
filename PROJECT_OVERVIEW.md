# UPlayground 프로젝트 분석 문서

## 1. 프로젝트 개요

**프로젝트명**: UPlayground  
**엔진 버전**: Unreal Engine 5.8  
**프로그래밍 언어**: C++20  
**프로젝트 타입**: 쿼터뷰 액션 파밍 ARPG (Gameplay Ability System 기반, 파밍 루프 개발 중)

---

## 2. 핵심 플러그인

### 엔진 플러그인
- **GameplayAbilities**: GAS(Gameplay Ability System) 기반 어빌리티 시스템
- **EnhancedInput**: 향상된 입력 시스템
- **CommonUI**: UI 시스템
- **GameFeatures**: 게임 기능 모듈화
- **ModularGameplay**: 모듈화된 게임플레이
- **MotionWarping**: 모션 워핑 기능
- **AnimationWarping**: 애니메이션 워핑
- **AnimationLocomotionLibrary**: 이동 애니메이션 라이브러리
- **SkeletalMeshModelingTools**: 스켈레탈 메시 모델링 도구

### 커스텀 플러그인
- **PGBlueprintUtil**: 블루프린트 유틸리티

---

## 3. 모듈 구조

프로젝트는 8개의 주요 모듈로 구성되어 있습니다.

```
UPlayground (메인 모듈)
├── PGAbilitySystem (어빌리티 시스템)
├── PGActor (액터 및 캐릭터)
├── PGAI (인공지능)
├── PGData (데이터 관리)
├── PGMessage (메시지 시스템)
├── PGShared (공유 리소스)
└── PGUI (사용자 인터페이스)
```

---

## 4. 모듈 상세 설명

### 4.1 UPlayground (메인 모듈)

**역할**: 프로젝트의 메인 모듈로 게임 로직의 진입점

**주요 디렉토리**:
- `AnimInstances`: 애니메이션 인스턴스
- `AnimNotify`: 애니메이션 노티파이
- `Cheat`: 치트 기능
- `GameMode`: 게임 모드
- `Utils`: 유틸리티 함수

**의존성**:
```cpp
PublicDependencyModuleNames: 
- Core, CoreUObject, Engine, InputCore
- EnhancedInput, GameplayTags, DeveloperSettings
- UMG, Niagara, CommonUI

PrivateDependencyModuleNames:
- GameplayTasks, GameFeatures, GameplayAbilities
- ModularGameplay, AnimGraphRuntime, AIModule
- PGData, PGAbilitySystem, PGActor, PGShared
```

---

### 4.2 PGAbilitySystem (어빌리티 시스템)

**역할**: Gameplay Ability System 기반의 스킬 및 능력치 관리

**핵심 클래스**:
- `UPGAbilitySystemComponent`: 커스텀 어빌리티 시스템 컴포넌트
- `UPGAtrributeSet`: 캐릭터 능력치(HP, MP 등) 관리

**주요 디렉토리**:
- `Abilities`: 게임 어빌리티 정의

**특징**:
- GAS를 활용한 스킬 시스템
- 네트워크 복제 지원
- 능력치 변경 이벤트 처리

---

### 4.3 PGActor (액터 및 캐릭터)

**역할**: 게임 내 모든 액터와 캐릭터 관리

#### 주요 구조

```
PGActor/
├── Characters/
│   ├── PGCharacterBase (베이스 캐릭터)
│   ├── Player/
│   │   └── PGCharacterPlayer (플레이어 캐릭터)
│   └── NonPlayer/
│       └── Enemy/
│           └── PGCharacterEnemy (적 캐릭터)
├── Components/
│   ├── Combat/ (전투 컴포넌트)
│   │   ├── PGPawnCombatComponent
│   │   ├── PGPlayerCombatComponent
│   │   ├── PGEnemyCombatComponent
│   │   └── PGSkillMontageController
│   ├── Stat/ (스탯 컴포넌트)
│   │   ├── PGStatComponent
│   │   ├── PGPlayerStatComponent
│   │   └── PGEnemyStatComponent
│   └── Input/
│       └── PGInputComponent
├── Controllers/
│   ├── PGPlayerController
│   └── PGAIController
├── Projectile/ (투사체)
│   ├── PGProjectileBase
│   └── Pool/ (오브젝트 풀링)
│       ├── PGProjectileManager
│       ├── PGProjectilePool
│       └── PGPooledProjectile
├── AreaOfEffect/ (범위 공격)
│   └── PGAreaOfEffectBase
├── Weapon/ (무기)
│   ├── PGWeaponBase
│   └── PGPlayerWeapon
├── Effects/
│   └── Decal/
│       ├── PGDecalActor
│       └── PGSkillIndicator (스킬 인디케이터)
└── Handler/
    └── Skill/ (스킬 핸들러)
        ├── PGSkillHandler
        ├── PGPlayerSkillHandler
        └── PGEnemySkillHandler
```

#### 핵심 클래스

**APGCharacterBase**
```cpp
- IAbilitySystemInterface 구현
- UPGAbilitySystemComponent 보유
- UMotionWarpingComponent로 모션 워핑 지원
- FPGSkillHandler를 통한 스킬 관리
- CharacterTID로 캐릭터 식별
```

**컴포넌트 시스템**
- **전투 컴포넌트**: 전투 로직 처리
- **스탯 컴포넌트**: HP, MP 등 능력치 관리
- **입력 컴포넌트**: 향상된 입력 시스템

**프로젝타일 시스템**
- 오브젝트 풀링 패턴 적용
- `PGProjectileManager`: 전역 투사체 관리자
- `PGProjectilePool`: 타입별 풀 관리
- `PGPooledProjectile`: 풀링된 투사체

---

### 4.4 PGData (데이터 관리)

**역할**: 게임 데이터 테이블 관리 및 로딩 최적화

#### 핵심 클래스: UPGDataTableManager

**특징**:
1. **GameInstanceSubsystem** 기반 싱글톤
2. **AssetRegistry**를 통한 자동 데이터 테이블 스캔
3. **지연 로딩** 및 **LRU 캐시** 시스템
4. **SearchKey 메타데이터** 기반 빠른 검색
5. **동적 타입 기반 API**

**주요 기능**:

```cpp
// 자동 타입 추론 API
UDataTable* LoadDataTable<T>();
T* GetRowData<T>(int64 SearchKey);
T* GetRowDataByName<T>(const FName& RowName);
TArray<T*> GetAllRowData<T>();
void UnloadDataTable<T>();

// 매크로 접근
#define PGData() UPGDataTableManager::Get()

// 사용 예시
auto* SkillData = PGData()->GetRowData<FPGSkillDataRow>(1001);
```

**캐시 시스템**:
- 최대 50개 테이블 캐시
- 100MB 메모리 제한
- 600초(10분)마다 자동 정리
- LRU 알고리즘 적용

**데이터 테이블 구조**:
```
DataTable/
├── ActorAssetPath/
├── AreaOfEffect/
├── AssetPath/
├── Projectile/
└── Skill/
```

---

### 4.5 PGShared (공유 리소스)

**역할**: 프로젝트 전체에서 사용되는 공용 타입 및 상수 정의

#### 디렉토리 구조

```
Shared/
├── Debug/
│   └── PGDebugHelper.h (디버그 헬퍼)
├── Define/
│   └── PGSkillDefine.h (스킬 관련 상수)
├── Enum/
│   ├── PGEnumTypes.h (일반 열거형)
│   ├── PGEnumDamageTypes.h (데미지 타입)
│   ├── PGMessageTypes.h (메시지 타입)
│   ├── PGProjectileEnumType.h (투사체 타입)
│   ├── PGSkillEnumTypes.h (스킬 타입)
│   └── PGStatEnumTypes.h (스탯 타입)
├── Structure/
│   ├── PGDamageFloaterCurveData (데미지 플로터 커브)
│   └── PlayerStructTypes (플레이어 구조체)
├── Message/
│   ├── Base/
│   │   ├── PGMessageEventDataBase
│   │   └── PGMessageEventDataTemplate
│   └── Stat/
│       └── PGStatUpdateEventData
└── Tag/
    ├── PGGamePlayTags (게임플레이 태그)
    ├── PGGamePlayInputTags (입력 태그)
    ├── PGGamePlayEventTags (이벤트 태그)
    └── PGGamePlayStatusTags (상태 태그)
```

**특징**:
- 모든 모듈에서 참조 가능한 공통 타입
- 게임플레이 태그 중앙 관리
- 메시지 이벤트 데이터 템플릿 제공

---

### 4.6 PGUI (사용자 인터페이스)

**역할**: 게임 UI 시스템 관리

#### 구조

```
PGUI/
├── Component/
│   └── Base/
│       ├── PGButton
│       └── PGWidgetComponentBase
├── Widget/
│   ├── Base/
│   │   └── PGWidgetBase
│   ├── Billboard/ (월드 공간 UI)
│   │   ├── PGUIEnemyNamePlate (적 네임플레이트)
│   │   └── PGUIPlayerHpBar (플레이어 HP바)
│   ├── DamageFloater/
│   │   └── PGUIDamageFloater (데미지 플로터)
│   └── HUD/
│       ├── PGUIHudPlayerInfo (플레이어 정보)
│       └── Skill/
│           ├── PGUIHudSkill (스킬 HUD)
│           └── PGUISkillSlot (스킬 슬롯)
└── Manager/
    └── PGDamageFloaterManager (데미지 플로터 관리자)
```

**UI 타입**:
1. **빌보드 UI**: 월드 공간에 표시되는 UI
2. **HUD**: 스크린 공간 UI
3. **데미지 플로터**: 데미지 표시 UI

---

### 4.7 PGAI (인공지능)

**역할**: 비헤이비어 트리 기반 AI 시스템

**구조**:
```
PGAI/
├── Decorator/ (비헤이비어 트리 데코레이터)
├── Service/ (비헤이비어 트리 서비스)
└── Task/ (비헤이비어 트리 태스크)
```

**특징**:
- UE5 비헤이비어 트리 시스템 활용
- 적 AI 행동 패턴 정의
- AIModule 의존성

---

### 4.8 PGMessage (메시지 시스템)

**역할**: 게임 내 이벤트 및 메시지 전달 시스템

**구조**:
```
PGMessage/
└── Manager/ (메시지 관리자)
```

**특징**:
- 이벤트 기반 통신
- 모듈 간 느슨한 결합
- `PGShared`의 메시지 타입 활용

---

## 5. 주요 시스템 아키텍처

### 5.1 캐릭터 시스템

```
APGCharacterBase (기본 캐릭터)
├── Components
│   ├── UPGAbilitySystemComponent (GAS)
│   ├── UPGStatComponent (능력치)
│   ├── UPGPawnCombatComponent (전투)
│   ├── UMotionWarpingComponent (모션 워핑)
│   └── UPGSkillMontageController (스킬 몽타주)
├── FPGSkillHandler (스킬 핸들러)
└── UPGDataAsset_StartUpDataBase (시작 데이터)
```

### 5.2 스킬 시스템

**플로우**:
1. 입력 수신 (`PGInputComponent`)
2. 스킬 핸들러 처리 (`PGSkillHandler`)
3. GAS 어빌리티 활성화 (`PGAbilitySystemComponent`)
4. 스킬 몽타주 재생 (`PGSkillMontageController`)
5. 투사체/범위 공격 생성 (`PGProjectileBase` / `PGAreaOfEffectBase`)
6. 데미지 적용 및 UI 표시 (`PGDamageFloater`)

### 5.3 데이터 플로우

```
DataTable 스캔 (AssetRegistry)
    ↓
PGDataTableManager 캐싱
    ↓
타입 기반 자동 로딩
    ↓
SearchKey 인덱스 검색
    ↓
FTableRowBase 데이터 반환
    ↓
LRU 캐시 관리
```

### 5.4 컴포넌트 기반 아키텍처

**특징**:
- 기능별로 분리된 컴포넌트
- 재사용 가능한 모듈화
- 상속보다 조합 우선

**주요 컴포넌트**:
- **PGPawnExtensionComponentBase**: 모든 Pawn 확장 컴포넌트의 베이스
- **Combat Component**: 전투 로직
- **Stat Component**: 능력치 관리
- **Input Component**: 입력 처리

---

## 6. 프로젝트 맵

**개발용 맵**:
- `DummyDevMap`: 더미 개발 맵
- `EmptyDevMap`: 빈 개발 맵
- `EnemyDevMap`: 적 테스트 맵
- `FeatureDevMap`: 기능 테스트 맵
- `StageDevMap`: 스테이지 진행 테스트 맵

기본 시작 맵은 Config/DefaultEngine.ini의 EmptyDevMap이다. StageDevMap의 BP/GameMode 설정은 실행 검증 결과와 함께 관리한다.

---

## 7. 주요 기술 스택

### C++ 기능
- **C++20** 표준
- **템플릿 메타프로그래밍** (DataTableManager)
- **Smart Pointers** (TObjectPtr, TSoftObjectPtr)
- **RTTI** (Dynamic Type Discovery)

### 디자인 패턴
- **Subsystem Pattern** (DataTableManager)
- **Component Pattern** (Combat, Stat Components)
- **Object Pooling** (Projectile System)
- **Observer Pattern** (Message System)
- **Handler Pattern** (Skill Handler)

### 최적화
- **지연 로딩** (Soft Object References)
- **LRU 캐싱**
- **오브젝트 풀링**
- **메모리 사용량 모니터링**

---

## 8. 코드 컨벤션

### 네이밍
- **클래스**: `PG` 접두사 (예: `APGCharacterBase`)
- **인터페이스**: `I` 접두사
- **열거형**: `EPG` 접두사
- **구조체**: `FPG` 접두사
- **위젯**: `PGUI` 접두사

### 파일 구조
- **헤더 파일 공개**: PublicIncludePaths 활용
- **모듈별 분리**: 기능 단위 모듈화
- **디렉토리 계층**: 역할별 명확한 분류

---

## 9. 개발 시 주의사항

### 데이터 테이블
- **SearchKey 메타데이터** 필수: `UPROPERTY(meta=(SearchKey))`
- **정수 타입 SearchKey**: int32, int64 등
- **FTableRowBase 상속**: 모든 데이터 행

### GAS 사용
- `UPGAbilitySystemComponent`를 통해 접근
- `PossessedBy`에서 초기화
- 네트워크 복제 고려

### 메모리 관리
- 데이터 테이블 자동 언로드 활용
- 대용량 에셋은 TSoftObjectPtr 사용
- 캐시 크기 모니터링

---

## 10. 확장 가능성

### 새로운 캐릭터 추가
1. `APGCharacterBase` 상속
2. 전용 Combat/Stat Component 구현
3. 데이터 테이블에 캐릭터 정보 추가
4. 스킬 핸들러 구현

### 새로운 스킬 추가
1. 스킬 데이터 테이블 추가
2. GameplayAbility 구현 (필요시)
3. 애니메이션 몽타주 추가
4. 투사체/범위 공격 구현

### 새로운 데이터 타입 추가
1. `FTableRowBase` 상속 구조체 정의
2. SearchKey 메타데이터 지정
3. DataTable 폴더에 에셋 생성
4. PGDataTableManager::FindSearchKeyProperty의 native SearchKey 등록과 패키지 조회 확인 후 자동 스캔 및 로딩

---

## 11. 빌드 설정

**Target 파일**:
- `UPlayground.Target.cs`: 게임 빌드
- `UPlaygroundEditor.Target.cs`: 에디터 빌드

**빌드 구성**:
- PCH 사용: UseExplicitOrSharedPCHs
- C++ 표준: C++20
- 디버그 심볼: DebugGame 구성

---

## 12. 버전 관리

**Git 저장소**:
- 메인 프로젝트: `.git`
- Content 서브모듈: `Content/.git`

**주의**:
- Content 폴더는 별도 저장소
- 바이너리 파일 제외 (.gitignore)
- DerivedDataCache 제외

---

## 요약

**UPlayground**는 GAS 기반의 모듈화된 액션 게임 프로젝트입니다.

**핵심 특징**:
- 8개 모듈로 구성된 확장 가능한 아키텍처
- 지연 로딩 및 LRU 캐싱으로 최적화된 데이터 관리
- 컴포넌트 기반 캐릭터 시스템
- 오브젝트 풀링 기반 투사체 시스템
- 타입 안정성이 보장되는 템플릿 API

**개발 방향**:
- 모듈 간 느슨한 결합
- 재사용 가능한 컴포넌트
- 메모리 효율적인 데이터 관리
- 확장 용이한 구조

이 문서를 참고하여 프로젝트의 전체 구조와 각 시스템의 역할을 빠르게 이해할 수 있습니다.
## 2026-09-19 1·2단계 반영

- 전투 수치 권위는 `UPGAtrributeSet`과 `UPGAbilitySystemComponent`다. `UPGStatComponent`는 초기 데이터와 호환 조회를 연결한다. 피해·회복·스탯 보상은 GameplayEffect로 확정하고 무기 보너스는 교체 가능한 효과로 관리한다.
- 기본 조작은 `PGQuarterViewData`를 사용하는 고정 쿼터뷰, WASD 이동과 지면 조준이다. `PGCombatFeedbackData`와 `PGCombatTuningData`에서 기본 피드백과 피해 수치를 조정한다.
- `APGStageManager`는 유한 배치 스폰, 중복 처치 방지, 토큰 기반 1회 보상 지급, 실패/완료 상태를 관리한다. 현재 보상은 런 내 Stat 보너스이며 파밍 인벤토리·저장은 후속 개발이다.
- 플레이 검증 맵은 `/Game/Maps/StageDevMap`, 튜닝 에셋은 `/Game/DataCenter/Stage12`다. 상세 구현 범위, 빌드/자동 테스트 결과 및 미완료 수용 기준은 [실행 보고서](Docs/todo/Stage12_Implementation_Report.md)를 따른다.

## 2026-09-19 3·4단계 반영

- `PGData/PGProgressionData`는 아이템·가방·드랍·시작 로드아웃·클리어 해금 빌드의 데이터 원본이다. 실제 프리셋은 `/Game/DataCenter/Progression/DA_PGProgression`이다.
- `PGActor/PGProfileSubsystem`이 GameInstance 수명으로 소유 아이템·장착·빌드·체크포인트·런 보상을 저장한다. `PGLootDrop`은 저장에 성공한 획득만 제거한다. `PGUIInventory`는 I 키로 열고, E 키로 근처 아이템을 획득한다.
- 소유 장비/런 보너스는 ASC의 별도 Profile GE로 적용한다. 기존 무기 액터의 GE와 공존한다. 저장은 두 슬롯을 번갈아 사용하며 실패 시 메모리 상태도 확정하지 않는다.
- StageManager는 StagePresentation 메시지와 지급 콜백만 제공하고 PGUIManager가 보상/재시작 창을 소유한다. PGData는 엔진 ASC/Ability 타입을 참조하며 PGAbilitySystem에 의존하지 않는다. PGShared의 커스텀 GAS include 경로도 제거했다.
- 실제 include 및 모듈 선언 목록은 `Tools/Validation/AuditModuleDependencies.py`, 구현/검증/잔여 수용 기준은 [3·4단계 보고서](Docs/todo/Stage34_Implementation_Report.md)를 따른다. 기존 본문의 의존성 예시는 최초 분석 당시 스냅샷이며 실제 Build.cs가 우선한다.

## 2026-09-20 전투 사이클 표현 구현

- PGUIRewardCard/PGUILootLabel이 보상 카드와 드랍 빌보드 표현을 담당한다. 스테이지의 메시지·토큰 지급 경로는 유지한다.
- PGEnemyAbilityAttack은 TelegraphDuration이 있는 스킬을 예고→단일 원형 판정→회복으로 실행한다. 정예 데이터와 예고 반경은 PGSkillDataRow가 소유한다.
- PGCombatFeedbackData에 일반/강타/치명타 프리셋을 추가했다. 신규 표현 에셋은 /Game/DataCenter/CombatCycle에 있다.
- 기존 데이터 에셋 연결은 실행 중인 에디터의 파일 잠금으로 저장 대기 중이다. 적용 상태와 실행 근거는 Docs/todo/CombatCycle_Implementation_Report.md를 따른다.

## 2026-09-20 메인 HUD

- `PGUIMainHUD`는 네이티브 Slate 기반 인게임 메인 화면이다. `APGPlayerController`가 로컬 플레이어에게 기본 생성하며 `bUseLegacyHUD`로 기존 블루프린트 HUD를 선택할 수 있다.
- GAS 생명력/분노, 스킬 핸들러 쿨다운, Enhanced Input 단축키, 스테이지 상태와 기존 인벤토리를 연결한다.
- ImageGen 원본은 `Tools/Art/MainUI`, 런타임 텍스처는 `/Game/UI/Main`에 있다. 적용·검증·잔여 확인은 [메인 HUD 문서](Docs/todo/MainUI_Implementation_Report.md)를 따른다.


- ?? ??? ??: RewardStatDataRow? Perk/PerkPercent/PlaystyleDescription?? ???????? ??? ????. ASC? ?? ??/??? ??? ?? ??? ????, ProfileSubsystem? ? ?? ?????????? ????. UI? ????? ?? ??? ???. ? ??? ??? ?? ????? ?? ?? ??? ???? ??.
