#pragma once

#include "CoreMinimal.h"
#include "PGDataTableManager.generated.h"

class FAssetRegistryModule;

// 검색 키 인덱스 정보
USTRUCT()
struct FPGSearchKeyIndex
{
    GENERATED_BODY()

public:
    // 검색 키 필드의 프로퍼티 정보
    const FProperty* SearchKeyProperty;

    // 인덱스 맵 (정수 검색 키 값 -> RowName)
    TMap<int64, FName> IndexMap;

    FPGSearchKeyIndex()
        : SearchKeyProperty(nullptr)
    {
    }
};

// 데이터 테이블 캐시 엔트리
USTRUCT()
struct FPGDataTableCacheEntry
{
    GENERATED_BODY()

    // 캐시된 데이터 테이블
    UPROPERTY()
    TObjectPtr<UDataTable> DataTable;

    // 마지막 접근 시간
    float LastAccessTime;

    // 메모리 사용량 (바이트)
    int32 MemoryUsage;

    // 검색 키 인덱스
    TOptional<FPGSearchKeyIndex> SearchKeyIndex;

    FPGDataTableCacheEntry()
        : DataTable(nullptr)
        , LastAccessTime(0.0)
        , MemoryUsage(0)
    {
    }

    FPGDataTableCacheEntry(UDataTable* InDataTable)
        : DataTable(InDataTable)
        , LastAccessTime(FPlatformTime::Seconds())
        , MemoryUsage(0)
    {
        if (DataTable)
        {
            MemoryUsage = DataTable->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal);
        }
    }

    void UpdateAccessTime()
    {
        LastAccessTime = FPlatformTime::Seconds();
    }
};

// 데이터 테이블 정보
USTRUCT()
struct FPGDataTableInfo
{
    GENERATED_BODY()

    // 에셋 경로
    UPROPERTY()
    FSoftObjectPath AssetPath;

    // 에셋 이름
    UPROPERTY()
    FString AssetName;

    // 행 구조체 타입 이름 (문자열)
    UPROPERTY()
    FString RowStructName;

    // 행 구조체 타입 (실제 타입)
    const UScriptStruct* RowStructType;

    // 예상 메모리 사용량
    UPROPERTY()
    int32 EstimatedMemoryUsage;

    FPGDataTableInfo()
        : RowStructType(nullptr)
        , EstimatedMemoryUsage(0)
    {
    }
};

/**
 * 데이터 테이블 관리자
 * - AssetRegistry를 통한 데이터 테이블 스캔( 경로는 데이터테이블 폴더 하위로만 한정하여 관리
 * - 지연 로딩 및 LRU 캐시 시스템
 * - SearchKey(정수타입 변수) 메타데이터 기반 검색 / 기본 RowName 기반 검색 지원
 * - 메모리 사용량 모니터링 및 자동 언로드
 * - 동적 타입 기반 테이블 찾기 (구조체 타입 -> 테이블명 매핑)
 */
UCLASS()
class PGDATA_API UPGDataTableManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

private:
    // 캐시 설정 상수
    static constexpr int32 MAX_CACHE_SIZE = 50;
    static constexpr int32 MAX_MEMORY_USAGE = 100 * 1024 * 1024;  // 100MB
    static constexpr float CACHE_CLEANUP_INTERVAL = 600.0f; // 600 초

private:
    // 에셋 레지스트리 모듈
    FAssetRegistryModule* AssetRegistryModule;

    // 스캔된 데이터 테이블 정보
    UPROPERTY()
    TArray<FPGDataTableInfo> DataTableInfos;

    // 로드된 데이터 테이블 캐시 (LRU) - 테이블 이름을 키로 사용
    UPROPERTY()
    TMap<FName, FPGDataTableCacheEntry> LoadedDataTables;
    
    // 구조체 타입 -> 테이블명 매핑 (동적 타입 기반 테이블 찾기)
    TMap<const UScriptStruct*, FName> StructTypeToTableNameMap;
    
    // 캐시 정리 타이머
    FTimerHandle CleanupTimerHandle;

    static TWeakObjectPtr<UPGDataTableManager> WeakThis;
    
public:
    UPGDataTableManager();

    static UPGDataTableManager* Get();
    
    // USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

public:
    // === 동적 타입 기반 API ===
    // 타입에서 자동으로 테이블을 찾아서 로딩
    template<typename T>
    UDataTable* LoadDataTable();
    
    // 타입에서 자동으로 테이블을 찾아서 SearchKey로 데이터 가져오기
    template<typename T>
    T* GetRowData(int64 SearchKey);
    
    // 타입에서 자동으로 테이블을 찾아서 RowName으로 데이터 가져오기
    template<typename T>
    T* GetRowDataByName(const FName& RowName);
    
    // 타입에서 자동으로 테이블을 찾아서 모든 데이터 가져오기
    template<typename T>
    TArray<T*> GetAllRowData();
    
    // 데이터 테이블 언로드 - 타입으로
    template<typename T>
    void UnloadDataTable();
    
    // 모든 데이터 테이블 정보 가져오기
    const TArray<FPGDataTableInfo>& GetAllDataTableInfo() const { return DataTableInfos; }

public:
    // 캐시 관련
    // 캐시 정리 (LRU 기반)
    void CleanupCache();

    // 수동 메모리 정리
    void ForceCleanupMemory();

    // 캐시 상태 정보
    void GetCacheInfo(int32& LoadedCount, int32& TotalMemoryUsage) const;

private:
    // 타이머 관련
    // 자동 정리 시작( LRU - 타이머구조 )
    void StartAutoCleanup();

    // 자동 정리 중지
    void StopAutoCleanup();
    
private:
    // 초기화 관련
    // 데이터 테이블 스캔
    void ScanDataTables();
    
    // 데이터 테이블 정보 수집
    void CollectDataTableInfo(const FAssetData& AssetData);

    // 구조체 타입 이름으로 실제 UScriptStruct 찾기
    const UScriptStruct* FindStructTypeByName(const FString& StructTypeName) const;

    // 테이블 이름으로 에셋 경로 찾기
    FSoftObjectPath FindAssetPathByName(const FName& TableName) const;

    // 구조체 타입으로 테이블명 찾기
    FName FindTableNameByStructType(const UScriptStruct* StructType) const;

private:
    // SearchKey 인덱스 생성
    static void BuildSearchKeyIndex(FPGDataTableCacheEntry& CacheEntry);

    // SearchKey 프로퍼티 찾기( FTableRowBase 하위 클래스에 정의한 메타데이터 )
    static const FProperty* FindSearchKeyProperty(const UScriptStruct* RowStruct);

    // 정수 프로퍼티를 int64로 변환
    static bool PropertyToInteger(const FProperty* Property, const void* ValuePtr, int64& OutValue);

    // SearchKey 프로퍼티가 정수 타입인지 검증
    static bool IsIntegerProperty(const FProperty* Property);
    
private:
    // LRU 정리 관련
    // 메모리 사용량 확인
    int32 GetCurrentMemoryUsage() const;

    // LRU 기반 캐시 제거
    void RemoveOldestCacheEntry();

    // 캐시 엔트리 제거
    void RemoveCacheEntry(const FName& TableName);
};


// === 동적 타입 기반 API 구현 ===

template<typename T>
UDataTable* UPGDataTableManager::LoadDataTable()
{
    static_assert(TIsDerivedFrom<T, FTableRowBase>::IsDerived, "T must be derived from FTableRowBase");
    
    // 구조체 타입으로 테이블명 찾기
    FName TableName = FindTableNameByStructType(T::StaticStruct());
    if (TableName.IsNone())
    {
        return nullptr;
    }
    
    // 이미 로드된 경우 캐시에서 반환
    if (FPGDataTableCacheEntry* CacheEntry = LoadedDataTables.Find(TableName))
    {
        CacheEntry->UpdateAccessTime();
        return CacheEntry->DataTable;
    }

    // 테이블 이름으로 에셋 경로 찾기
    FSoftObjectPath AssetPath = FindAssetPathByName(TableName);
    if (!AssetPath.IsValid())
    {
        return nullptr;
    }

    // 새로 로드
    UDataTable* DataTable = Cast<UDataTable>(AssetPath.TryLoad());
    if (!DataTable)
    {
        return nullptr;
    }

    // 캐시에 추가 (테이블 이름을 키로 사용)
    FPGDataTableCacheEntry NewEntry(DataTable);
    
    // SearchKey 인덱스 생성
    BuildSearchKeyIndex(NewEntry);
    
    LoadedDataTables.Add(TableName, NewEntry);

    // 캐시 크기 확인 및 정리
    if (LoadedDataTables.Num() > MAX_CACHE_SIZE || GetCurrentMemoryUsage() > MAX_MEMORY_USAGE)
    {
        CleanupCache();
    }

    return DataTable;
}

template<typename T>
T* UPGDataTableManager::GetRowData(int64 SearchKey)
{
    static_assert(TIsDerivedFrom<T, FTableRowBase>::IsDerived, "T must be derived from FTableRowBase");
    
    UDataTable* DataTable = LoadDataTable<T>();
    if (!DataTable)
    {
        return nullptr;
    }

    // 구조체 타입으로 테이블명 찾기
    FName TableName = FindTableNameByStructType(T::StaticStruct());
    
    // 캐시에서 인덱스 정보 가져오기
    FPGDataTableCacheEntry* CacheEntry = LoadedDataTables.Find(TableName);
    if (!CacheEntry)
    {
        return nullptr;
    }

    // SearchKey 인덱스가 있는 경우
    if (CacheEntry->SearchKeyIndex.IsSet())
    {
        if (const FName* FoundRowName = CacheEntry->SearchKeyIndex->IndexMap.Find(SearchKey))
        {
            return DataTable->FindRow<T>(*FoundRowName, TEXT("DataTableManager"));
        }
        return nullptr;
    }
    
    return nullptr;
}

template<typename T>
T* UPGDataTableManager::GetRowDataByName(const FName& RowName)
{
    static_assert(TIsDerivedFrom<T, FTableRowBase>::IsDerived, "T must be derived from FTableRowBase");
    
    UDataTable* DataTable = LoadDataTable<T>();
    if (!DataTable)
    {
        return nullptr;
    }

    return DataTable->FindRow<T>(RowName, TEXT("DataTableManager"));
}

template<typename T>
TArray<T*> UPGDataTableManager::GetAllRowData()
{
    static_assert(TIsDerivedFrom<T, FTableRowBase>::IsDerived, "T must be derived from FTableRowBase");
    
    TArray<T*> Results;
    
    UDataTable* DataTable = LoadDataTable<T>();
    if (!DataTable)
    {
        return Results;
    }

    TArray<FName> RowNames = DataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        T* RowData = DataTable->FindRow<T>(RowName, TEXT("DataTableManager"));
        if (RowData)
        {
            Results.Add(RowData);
        }
    }

    return Results;
}

template<typename T>
void UPGDataTableManager::UnloadDataTable()
{
    static_assert(TIsDerivedFrom<T, FTableRowBase>::IsDerived, "T must be derived from FTableRowBase");
    
    // 구조체 타입으로 테이블명 찾기
    FName TableName = FindTableNameByStructType(T::StaticStruct());
    if (!TableName.IsNone() && LoadedDataTables.Contains(TableName))
    {
        RemoveCacheEntry(TableName);
    }
}

#define PGData() UPGDataTableManager::Get()