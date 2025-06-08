# PGBlueprintUtil 에디터 기능 설계서

## 📋 개요

PGBlueprintUtil 모듈에 에디터 전용 기능을 추가하여, 개발자가 특정 부모 클래스를 상속받는 블루프린트들을 쉽게 검색하고 관리할 수 있는 도구를 제공합니다.

## 🎯 목표 기능

### 주요 기능
- **에디터 메뉴 통합**: 블루프린트 드롭다운 메뉴에 "부모 클래스별 블루프린트 검색" 기능 추가
- **부모 클래스 검색**: 사용자가 선택한 부모 클래스를 상속받는 모든 블루프린트 검색
- **검색 결과 표시**: 검색된 블루프린트 목록을 직관적인 UI로 표시
- **빠른 접근**: 검색 결과에서 블루프린트를 더블클릭하여 바로 열기

### 사용 시나리오
1. **캐릭터 블루프린트 관리**: ACharacter를 상속받는 모든 블루프린트 찾기
2. **액터 컴포넌트 검색**: UActorComponent 계열 블루프린트 일괄 검색
3. **게임모드 관리**: AGameModeBase 상속 블루프린트들 관리
4. **커스텀 클래스 검색**: 프로젝트 특정 기본 클래스를 상속받는 블루프린트 찾기

## 🏗️ 아키텍처 설계

### 1. 모듈 구조 확장

```
PGBlueprintUtil/
├── Runtime/                    # 런타임 기능
│   ├── Public/
│   │   └── PGBlueprintUtil.h
│   └── Private/
│       └── PGBlueprintUtil.cpp
└── Editor/                     # 에디터 전용 기능
    ├── Public/
    │   ├── PGBlueprintSearchTool.h
    │   ├── PGBlueprintSearchWidget.h
    │   └── PGBlueprintUtilEditor.h
    └── Private/
        ├── PGBlueprintSearchTool.cpp
        ├── PGBlueprintSearchWidget.cpp
        ├── PGBlueprintUtilEditor.cpp
        └── PGBlueprintUtilEditorCommands.cpp
```

### 2. 핵심 컴포넌트

#### 2.1 PGBlueprintUtilEditor
```cpp
class FPGBlueprintUtilEditorModule : public IModuleInterface
{
    // 에디터 모듈 진입점
    // 메뉴 등록 및 초기화 담당
}
```

#### 2.2 PGBlueprintSearchTool
```cpp
class UPGBlueprintSearchTool : public UObject
{
    // 블루프린트 검색 로직 구현
    // AssetRegistry 기반 검색
    // 부모 클래스 필터링
}
```

#### 2.3 PGBlueprintSearchWidget
```cpp
class SPGBlueprintSearchWidget : public SCompoundWidget
{
    // 검색 UI 위젯
    // 부모 클래스 선택 인터페이스
    // 검색 결과 리스트 표시
}
```

#### 2.4 에디터 명령어 시스템
```cpp
class FPGBlueprintUtilCommands : public TCommands<FPGBlueprintUtilCommands>
{
    // 에디터 명령어 정의
    // 단축키 바인딩
}
```

### 3. 데이터 구조

#### 3.1 검색 조건
```cpp
USTRUCT()
struct FPGBlueprintSearchCriteria
{
    // 부모 클래스 (UClass*)
    TObjectPtr<UClass> ParentClass;
    
    // 검색 범위 (프로젝트/플러그인/엔진)
    ESearchScope SearchScope;
    
    // 필터 옵션
    bool bIncludeAbstractClasses;
    bool bIncludeDeprecated;
    bool bExactMatch; // 정확한 부모 클래스만 vs 상속 계층 포함
};
```

#### 3.2 검색 결과
```cpp
USTRUCT()
struct FPGBlueprintSearchResult
{
    // 블루프린트 에셋 참조
    FSoftObjectPath BlueprintAssetPath;
    
    // 블루프린트 이름
    FString BlueprintName;
    
    // 부모 클래스 이름
    FString ParentClassName;
    
    // 에셋 경로
    FString AssetPath;
    
    // 마지막 수정 시간
    FDateTime LastModified;
    
    // 파일 크기
    int64 FileSize;
};
```

## 🎨 UI/UX 설계

### 1. 메뉴 통합
```
에디터 메뉴바
└── 블루프린트 ▼
    ├── 새 블루프린트...
    ├── 블루프린트 클래스 열기...
    ├── ──────────────────
    └── 🔍 부모 클래스별 블루프린트 검색  ← 새로 추가
```

### 2. 검색 위젯 UI 구성

```
┌─────────────────────────────────────────────────────┐
│ 부모 클래스별 블루프린트 검색                              │
├─────────────────────────────────────────────────────┤
│ 부모 클래스: [Class Picker     ▼] [검색] [초기화]        │
│                                                     │
│ 검색 옵션:                                           │
│ ☑ 추상 클래스 포함  ☑ Deprecated 포함  ☐ 정확한 일치만    │
│                                                     │
│ 검색 범위: ◉ 프로젝트 ○ 플러그인 포함 ○ 엔진 포함        │
├─────────────────────────────────────────────────────┤
│ 검색 결과 (15개 발견)                                 │
├─────────────────────────────────────────────────────┤
│ 이름                  │ 부모 클래스      │ 경로           │
├─────────────────────────────────────────────────────┤
│ 📘 BP_PlayerCharacter │ ACharacter      │ /Game/...    │
│ 📘 BP_EnemyBase      │ ACharacter      │ /Game/...    │
│ 📘 BP_NPCCharacter   │ ACharacter      │ /Game/...    │
│ ...                  │ ...             │ ...          │
└─────────────────────────────────────────────────────┘
```

### 3. 상호작용 설계
- **더블클릭**: 블루프린트 에디터에서 열기
- **우클릭**: 컨텍스트 메뉴 (콘텐츠 브라우저에서 보기, 삭제 등)
- **드래그**: 뷰포트나 다른 위치로 드래그 가능
- **필터링**: 실시간 텍스트 필터링 지원

## 🔧 기술 구현 세부사항

### 1. 에디터 메뉴 확장

```cpp
// ToolMenus 시스템 사용
void RegisterMenuExtensions()
{
    UToolMenus* ToolMenus = UToolMenus::Get();
    UToolMenu* BlueprintMenu = ToolMenus->FindMenu("LevelEditor.MainMenu.Blueprint");
    
    FToolMenuSection& Section = BlueprintMenu->AddSection("PGBlueprintUtils");
    Section.AddMenuEntry(
        FPGBlueprintUtilCommands::Get().OpenBlueprintSearch,
        TAttribute<FText>::Create([](){ return LOCTEXT("SearchByParent", "부모 클래스별 블루프린트 검색"); })
    );
}
```

### 2. AssetRegistry 기반 검색

```cpp
// 블루프린트 검색 구현
TArray<FPGBlueprintSearchResult> SearchBlueprintsByParentClass(UClass* ParentClass)
{
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    
    // 블루프린트 에셋 검색
    FARFilter Filter;
    Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
    Filter.bRecursiveClasses = true;
    
    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);
    
    // 부모 클래스 필터링
    return FilterByParentClass(AssetDataList, ParentClass);
}
```

### 3. Slate UI 구현

```cpp
// 검색 위젯 구현
class SPGBlueprintSearchWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPGBlueprintSearchWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    // UI 컴포넌트들
    TSharedPtr<SClassViewer> ClassPicker;
    TSharedPtr<SListView<TSharedPtr<FPGBlueprintSearchResult>>> ResultListView;
    TSharedPtr<SSearchBox> FilterTextBox;
    
    // 이벤트 핸들러
    void OnSearchClicked();
    void OnClassSelected(UClass* SelectedClass);
    void OnResultDoubleClicked(TSharedPtr<FPGBlueprintSearchResult> Item);
};
```

## 📊 성능 고려사항

### 1. 검색 최적화
- **AssetRegistry 캐싱**: 에셋 정보 미리 로드
- **백그라운드 검색**: UI 블록킹 방지
- **점진적 로딩**: 결과를 청크 단위로 로드
- **메모리 관리**: 검색 결과 LRU 캐시

### 2. UI 반응성
- **가상화 리스트**: 대량 결과 처리
- **지연 로딩**: 썸네일 등은 필요시에만
- **필터링 디바운싱**: 타이핑 중 과도한 검색 방지

## 🧪 테스트 전략

### 1. 단위 테스트
- 검색 로직 정확성 검증
- 필터링 기능 테스트
- 에지 케이스 처리 (빈 결과, 잘못된 클래스 등)

### 2. 통합 테스트
- 에디터 메뉴 통합 확인
- UI 상호작용 테스트
- 성능 벤치마크

### 3. 사용성 테스트
- 실제 프로젝트에서의 사용성 검증
- 개발자 워크플로우 개선 확인

## 📈 확장 가능성

### Phase 1: 기본 기능
- [x] 에디터 메뉴 통합
- [x] 기본 검색 기능
- [x] 간단한 결과 표시

### Phase 2: 고급 기능
- [ ] 고급 필터링 (수정일, 크기, 태그 등)
- [ ] 검색 결과 내보내기 (CSV, JSON)
- [ ] 북마크 기능
- [ ] 검색 히스토리

### Phase 3: 확장 기능
- [ ] 종속성 분석 (어떤 블루프린트가 서로 참조하는지)
- [ ] 일괄 작업 (리네임, 이동, 속성 변경)
- [ ] 통계 및 분석 (사용 빈도, 복잡도 등)
- [ ] 팀 협업 기능 (공유 검색, 주석)

## 🔗 관련 언리얼 시스템

### 필요한 모듈
- **AssetRegistry**: 에셋 검색 및 메타데이터
- **ToolMenus**: 에디터 메뉴 확장
- **Slate**: UI 프레임워크
- **EditorWidgets**: 에디터 전용 위젯
- **ContentBrowser**: 에셋 탐색 연동
- **BlueprintGraph**: 블루프린트 분석

### 참고할 기존 시스템
- **콘텐츠 브라우저 필터**: 검색 및 필터링 로직
- **클래스 뷰어**: 클래스 선택 UI
- **참조 뷰어**: 종속성 분석 UI
- **에셋 도구**: 일괄 작업 패턴

---

*이 설계서는 개발 진행에 따라 지속적으로 업데이트됩니다.*
