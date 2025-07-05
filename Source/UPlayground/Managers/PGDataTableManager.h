
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PGDataTableManager.generated.h"

class FAssetRegistryModule;

// 데이터 테이블 캐시 엔트리
USTRUCT()
struct FPGDataTableCacheEntry
{
    GENERATED_BODY()

    // 캐시된 데이터 테이블
    UPROPERTY()
    TObjectPtr<UDataTable> DataTable;

    // 마지막 접근 시간
    double LastAccessTime;

    // 메모리 사용량 (바이트)
    int32 MemoryUsage;

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

    // 행 구조체 타입
    UPROPERTY()
    FString RowStructName;

    // 예상 메모리 사용량
    UPROPERTY()
    int32 EstimatedMemoryUsage;

    FPGDataTableInfo()
        : EstimatedMemoryUsage(0)
    {
    }
};


/**
 * 데이터 테이블 관리자
 * - AssetRegistry를 통한 데이터 테이블 스캔
 * - 지연 로딩 및 LRU 캐시 시스템
 * - 메모리 사용량 모니터링 및 자동 언로드
 * - LRU 대신 스테이트 전환 등 시점에 명시적으로 메모리를 제거해주는 게 좋을 수 있음..
 */
UCLASS()
class UPLAYGROUND_API UPGDataTableManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

protected:
    // 에셋 레지스트리 모듈
    FAssetRegistryModule* AssetRegistryModule;

    // 스캔된 데이터 테이블 정보
    UPROPERTY()
    TArray<FPGDataTableInfo> DataTableInfos;

    // 로드된 데이터 테이블 캐시 (LRU) - 테이블 이름을 키로 사용
    UPROPERTY()
    TMap<FName, FPGDataTableCacheEntry> LoadedDataTables;

    // 최대 캐시 크기
    UPROPERTY(EditAnywhere, Category = "Cache Settings")
    int32 MaxCacheSize;

    // 최대 메모리 사용량 (바이트)
    UPROPERTY(EditAnywhere, Category = "Cache Settings")
    int32 MaxMemoryUsage;

    // 캐시 정리 주기 (초)
    UPROPERTY(EditAnywhere, Category = "Cache Settings")
    float CleanupInterval;

    // 자동 정리 활성화
    UPROPERTY(EditAnywhere, Category = "Cache Settings")
    bool bAutoCleanup;

    // 캐시 정리 타이머
    FTimerHandle CleanupTimerHandle;
    
public:
    UPGDataTableManager();

    // USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 데이터 테이블 스캔
    void ScanDataTables();

    // 데이터 테이블 로드 (지연 로딩) - 테이블 이름으로
    UDataTable* LoadDataTable(const FName& TableName);

    // 특정 행 데이터 가져오기 - 테이블 이름으로
    uint8* GetRowData(const FName& TableName, const FName& RowName);

    // 템플릿 버전 행 데이터 가져오기 - 테이블 이름으로
    template<typename T>
    T* GetRowData(const FName& TableName, const FName& RowName);

    // 필드값으로 행 데이터 가져오기
    template<typename T, typename FieldType>
    T* GetRowDataByField(const FName& TableName, const FieldType& FieldValue, 
                         FieldType T::*FieldPtr);

    // 모든 행 데이터 가져오기
    template<typename T>
    TArray<T*> GetAllRowData(const FName& TableName);

    // 데이터 테이블 언로드 - 테이블 이름으로
    void UnloadDataTable(const FName& TableName);

    // 캐시 정리 (LRU 기반)
    void CleanupCache();

    // 수동 메모리 정리
    void ForceCleanupMemory();

    // 캐시 상태 정보
    void GetCacheInfo(int32& LoadedCount, int32& TotalMemoryUsage) const;

    // 모든 데이터 테이블 정보 가져오기
    const TArray<FPGDataTableInfo>& GetAllDataTableInfo() const { return DataTableInfos; }

    // 설정
    void SetMaxCacheSize(int32 NewMaxCacheSize) { MaxCacheSize = NewMaxCacheSize; }
    void SetMaxMemoryUsage(int32 NewMaxMemoryUsage) { MaxMemoryUsage = NewMaxMemoryUsage; }


private:
    // 데이터 테이블 정보 수집
    void CollectDataTableInfo(const FAssetData& AssetData);

    // 테이블 이름으로 에셋 경로 찾기
    FSoftObjectPath FindAssetPathByName(const FName& TableName) const;

    // 메모리 사용량 확인
    int32 GetCurrentMemoryUsage() const;

    // LRU 기반 캐시 제거
    void RemoveOldestCacheEntry();

    // 캐시 엔트리 제거
    void RemoveCacheEntry(const FName& TableName);

    // 자동 정리 시작
    void StartAutoCleanup();

    // 자동 정리 중지
    void StopAutoCleanup();
};


template<typename T>
T* UPGDataTableManager::GetRowData(const FName& TableName, const FName& RowName)
{
    UDataTable* DataTable = LoadDataTable(TableName);
    if (!DataTable)
    {
        return nullptr;
    }

    return DataTable->FindRow<T>(RowName, TEXT("DataTableManager"));
}

template<typename T, typename FieldType>
T* UPGDataTableManager::GetRowDataByField(const FName& TableName,
    const FieldType& FieldValue,  FieldType T::*FieldPtr)
{
    UDataTable* DataTable = LoadDataTable(TableName);
    if (!DataTable)
    {
        return nullptr;
    }

    // 모든 행을 순회하면서 필드값 비교
    TArray<FName> RowNames = DataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        T* RowData = DataTable->FindRow<T>(RowName, TEXT("DataTableManager"));
        if (RowData && (RowData->*FieldPtr) == FieldValue)
        {
            return RowData;
        }
    }

    return nullptr;
}

template<typename T>
TArray<T*> UPGDataTableManager::GetAllRowData(const FName& TableName)
{
    TArray<T*> Results;
    
    UDataTable* DataTable = LoadDataTable(TableName);
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
