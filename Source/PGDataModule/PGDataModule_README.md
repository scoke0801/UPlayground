# PGDataModule - MMORPG 데이터 관리 시스템

## 📋 개요

PGDataModule은 UPlayground MMORPG 프로젝트를 위한 포괄적인 데이터 관리 시스템입니다. 
다양한 데이터 소스(JSON, Excel, CSV)로부터 데이터를 읽어와 강타입 데이터테이블을 구성하고, 
효율적인 데이터 접근과 캐싱을 제공하는 독립적인 모듈입니다.

## 🎯 구현 목표

### 1단계: 핵심 인프라스트럭처 구축
- [x] **모듈 설정 및 기본 구조 생성**
  - PGDataModule 생성 및 빌드 설정
  - 기본 헤더/소스 파일 구조 확립
  - 모듈 의존성 설정

- [ ] **데이터 매니저 서브시스템**
  - `UPGDataManagerSubsystem` 게임 인스턴스 서브시스템 구현
  - 전체 데이터 생명주기 관리
  - 다른 매니저 시스템과의 독립적 동작

- [ ] **데이터 레지스트리 시스템**
  - 데이터 테이블 메타데이터 중앙 관리
  - 동적 테이블 등록/조회 기능
  - 스키마 및 캐싱 정책 관리

### 2단계: 데이터 로딩 시스템
- [ ] **데이터 소스 추상화**
  - `IPGDataLoader` 인터페이스 정의
  - 다양한 데이터 형식 지원을 위한 추상화 계층

- [ ] **구체적 로더 구현**
  - `UPGJSONLoader`: JSON 파일 로딩
  - `UPGExcelLoader`: Excel 파일 로딩 (UE5 내장 기능 활용)
  - `UPGCSVLoader`: CSV 파일 로딩

- [ ] **비동기 로딩 시스템**
  - 백그라운드 데이터 로딩
  - 로딩 진행률 및 상태 관리
  - 대용량 데이터 처리 최적화

### 3단계: 강타입 데이터 테이블 시스템
- [ ] **베이스 데이터 테이블**
  - `UPGBaseDataTable` 추상 기본 클래스
  - 공통 데이터 테이블 인터페이스

- [ ] **템플릿 기반 타입 안전성**
  - `UPGTypedDataTable<T>` 템플릿 클래스
  - 컴파일 타임 타입 검증
  - 강타입 데이터 접근 API

- [ ] **데이터 행 구조체**
  - `FPGDataRowBase` 기본 데이터 행 구조체
  - 자동 ID 관리 및 검증
  - 메타데이터 지원

### 4단계: 스키마 및 검증 시스템
- [ ] **데이터 스키마 정의**
  - `UPGDataSchema` 스키마 정의 클래스
  - 필드 타입, 제약조건, 기본값 정의
  - 버전 관리 및 마이그레이션 지원

- [ ] **유효성 검증**
  - `UPGSchemaValidator` 검증 시스템
  - 로딩 시점 데이터 검증
  - 런타임 데이터 무결성 확인

### 5단계: 고성능 캐싱 시스템
- [ ] **계층화된 캐싱**
  - L1 캐시: 현재 사용 중인 데이터 (항상 메모리 유지)
  - L2 캐시: 최근 사용 데이터 (LRU 알고리즘)
  - L3 캐시: 압축된 데이터 (필요시 압축 해제)

- [ ] **인덱싱 시스템**
  - 자주 사용되는 검색 조건에 대한 인덱스 구성
  - 복합 인덱스 지원
  - 동적 인덱스 생성/삭제

- [ ] **메모리 관리**
  - 메모리 풀링 시스템
  - 자동 가비지 컬렉션
  - 메모리 사용량 모니터링

### 6단계: 데이터 접근 API
- [ ] **기본 접근 패턴**
  - ID 기반 직접 접근 (O(1) 성능)
  - 범위 기반 접근
  - 조건부 필터링

- [ ] **고급 쿼리 시스템**
  - 람다 기반 쿼리
  - 술어 객체를 활용한 복잡한 검색
  - 결과 캐싱 및 최적화

- [ ] **배치 처리**
  - 다중 데이터 동시 접근
  - 배치 업데이트 및 검증
  - 트랜잭션 지원

### 7단계: 에러 처리 및 복구
- [ ] **단계별 에러 처리**
  - 로딩 단계: 파일 접근 실패, 파싱 에러
  - 검증 단계: 스키마 불일치, 타입 에러
  - 런타임 단계: 메모리 부족, 캐시 실패

- [ ] **복구 메커니즘**
  - 폴백 데이터 시스템
  - 부분 로딩 지원
  - 핫 리로딩 기능

### 8단계: 개발자 도구 및 유틸리티
- [ ] **데이터 에디터 도구**
  - 언리얼 에디터 플러그인
  - 시각적 데이터 편집 인터페이스
  - 실시간 검증 및 미리보기

- [ ] **성능 프로파일러**
  - 데이터 접근 패턴 분석
  - 캐시 히트율 모니터링
  - 메모리 사용량 추적

- [ ] **디버깅 도구**
  - 데이터 상태 시각화
  - 로딩 과정 추적
  - 에러 로그 및 스택 추적

### 9단계: 확장성 및 최적화
- [ ] **플러그인 아키텍처**
  - 커스텀 로더 플러그인 지원
  - 데이터 변환기 확장
  - 캐싱 전략 커스터마이징

- [ ] **다국어 지원**
  - 로컬라이제이션 테이블 연동
  - 문자열 데이터 자동 번역
  - 지역별 데이터 변형 지원

- [ ] **클라우드 연동**
  - 원격 데이터 소스 지원
  - 실시간 데이터 동기화
  - CDN 기반 데이터 배포

## 🏗️ 아키텍처 설계

### 모듈 구조
```
PGDataModule/
├── Core/                    # 핵심 시스템
│   ├── DataManager         # 중앙 관리자
│   ├── DataRegistry        # 테이블 등록소
│   └── DataCache          # 캐싱 시스템
├── Loaders/                # 데이터 로더
│   ├── JSONLoader
│   ├── ExcelLoader
│   └── CSVLoader
├── Tables/                 # 데이터 테이블
│   ├── BaseDataTable
│   └── TypedDataTable<T>
├── Schema/                 # 스키마 시스템
│   ├── DataSchema
│   └── SchemaValidator
├── Utils/                  # 유틸리티
│   ├── DataConverter
│   └── SerializationHelper
└── Editor/                 # 에디터 도구
    ├── DataEditorWidget
    └── ProfilerWidget
```

### 클래스 계층 구조
```cpp
// 핵심 인터페이스
IPGDataLoader (인터페이스)
IPGSavableInterface (인터페이스)

// 매니저 시스템
UPGDataManagerSubsystem : UGameInstanceSubsystem
UPGDataRegistry : UObject
UPGDataCache : UObject

// 데이터 테이블
UPGBaseDataTable : UObject
UPGTypedDataTable<T> : UPGBaseDataTable

// 로더 시스템
UPGJSONLoader : UObject, IPGDataLoader
UPGExcelLoader : UObject, IPGDataLoader
UPGCSVLoader : UObject, IPGDataLoader

// 스키마 시스템
UPGDataSchema : UObject
UPGSchemaValidator : UObject
```

## 🎮 MMORPG 활용 예시

### 몬스터 데이터 관리
```cpp
// 몬스터 데이터 구조체 정의
USTRUCT(BlueprintType)
struct FPGMonsterData : public FPGDataRowBase
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString MonsterName;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Level;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float HP;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<int32> DropItemIDs;
};

// 사용법
auto MonsterTable = DataManager->GetTable<FPGMonsterData>("MonsterData");
auto Dragon = MonsterTable->GetByID(1001);  // ID로 직접 접근
auto HighLevelMonsters = MonsterTable->Query([](const FPGMonsterData& Data) {
    return Data.Level >= 50;
}); // 레벨 50 이상 몬스터 검색
```

### 아이템 데이터 관리
```cpp
// 런타임에 동적으로 아이템 데이터 로딩
DataManager->LoadTableAsync<FPGItemData>("Items/Weapons.json")
    .Then([](UPGTypedDataTable<FPGItemData>* Table) {
        // 로딩 완료 후 처리
        auto LegendaryWeapons = Table->QueryByIndex("Rarity", EPGItemRarity::Legendary);
    });
```

## 📈 성능 목표

- **로딩 성능**: 1만 행 데이터를 1초 이내 로딩
- **메모리 효율성**: 사용하지 않는 데이터 자동 압축으로 메모리 사용량 50% 절약
- **접근 성능**: ID 기반 접근 O(1), 인덱스 기반 접근 O(log n)
- **캐시 효율성**: 90% 이상 캐시 히트율 달성

## 🔧 기술 요구사항

- **언리얼 엔진**: 5.4 이상
- **C++ 표준**: C++20
- **메모리**: 최소 8GB RAM 권장
- **플랫폼**: Windows, Mac, Linux 지원

## 📝 개발 단계별 마일스톤

### Phase 1: Foundation (Week 1-2)
- 모듈 기본 구조 및 빌드 설정
- 데이터 매니저 서브시스템 기본 틀
- 간단한 JSON 로더 프로토타입

### Phase 2: Core Systems (Week 3-4)
- 강타입 데이터 테이블 시스템
- 기본 캐싱 메커니즘
- Excel/CSV 로더 구현

### Phase 3: Advanced Features (Week 5-6)
- 스키마 시스템 및 검증
- 고급 쿼리 및 인덱싱
- 에러 처리 및 복구

### Phase 4: Tools & Optimization (Week 7-8)
- 에디터 도구 개발
- 성능 최적화
- 문서화 및 테스트

## 🧪 테스트 전략

- **단위 테스트**: 각 컴포넌트별 독립적 테스트
- **통합 테스트**: 시스템 간 연동 테스트
- **성능 테스트**: 대용량 데이터 로딩 및 접근 성능 측정
- **스트레스 테스트**: 극한 상황에서의 안정성 검증

---

*이 문서는 PGDataModule의 개발 진행에 따라 지속적으로 업데이트됩니다.*
