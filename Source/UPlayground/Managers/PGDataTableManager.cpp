

#include "Managements/PGDataTableManager.h"

#include "AssetRegistry/AssetRegistryModule.h"


UPGDataTableManager::UPGDataTableManager()
    : AssetRegistryModule(nullptr)
    , MaxCacheSize(50)
    , MaxMemoryUsage(100 * 1024 * 1024) // 100MB
    , CleanupInterval(60.0f) // 60초
    , bAutoCleanup(true)
{
}

void UPGDataTableManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // AssetRegistry 모듈 가져오기
    AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    // 데이터 테이블 스캔
    ScanDataTables();

    // 자동 정리 시작
    if (bAutoCleanup)
    {
        StartAutoCleanup();
    }
}

void UPGDataTableManager::Deinitialize()
{
    // 자동 정리 중지
    StopAutoCleanup();

    // 모든 캐시 정리
    LoadedDataTables.Empty();
    DataTableInfos.Empty();

    AssetRegistryModule = nullptr;

    Super::Deinitialize();
}

void UPGDataTableManager::ScanDataTables()
{
    if (!AssetRegistryModule)
    {
        return;
    }

    // 기존 정보 초기화
    DataTableInfos.Empty();

    // AssetRegistry에서 모든 데이터 테이블 에셋 검색
    FARFilter Filter;
    Filter.ClassPaths.Add(UDataTable::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;

    TArray<FAssetData> AssetDataList;
    AssetRegistryModule->Get().GetAssets(Filter, AssetDataList);

    // 각 에셋 정보 수집
    for (const FAssetData& AssetData : AssetDataList)
    {
        CollectDataTableInfo(AssetData);
    }
}

void UPGDataTableManager::CollectDataTableInfo(const FAssetData& AssetData)
{
    FPGDataTableInfo Info;
    Info.AssetPath = AssetData.GetSoftObjectPath();
    Info.AssetName = AssetData.AssetName.ToString();

    // 행 구조체 타입 정보 가져오기
    FString RowStructName;
    if (AssetData.GetTagValue(GET_MEMBER_NAME_CHECKED(UDataTable, RowStruct), RowStructName))
    {
        Info.RowStructName = RowStructName;
    }

    // 예상 메모리 사용량 (대략적으로 계산)
    int32 ApproximateSize = 0;
    if (AssetData.GetTagValue(GET_MEMBER_NAME_CHECKED(UDataTable, ImportKeyField), ApproximateSize))
    {
        Info.EstimatedMemoryUsage = ApproximateSize;
    }

    DataTableInfos.Add(Info);
}

UDataTable* UPGDataTableManager::LoadDataTable(const FName& TableName)
{
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
    LoadedDataTables.Add(TableName, NewEntry);

    // 캐시 크기 확인 및 정리
    if (LoadedDataTables.Num() > MaxCacheSize || GetCurrentMemoryUsage() > MaxMemoryUsage)
    {
        CleanupCache();
    }

    return DataTable;
}

uint8* UPGDataTableManager::GetRowData(const FName& TableName, const FName& RowName)
{
    UDataTable* DataTable = LoadDataTable(TableName);
    if (!DataTable)
    {
        return nullptr;
    }

    return DataTable->FindRowUnchecked(RowName);
}

void UPGDataTableManager::UnloadDataTable(const FName& TableName)
{
    if (LoadedDataTables.Contains(TableName))
    {
        RemoveCacheEntry(TableName);
    }
}

void UPGDataTableManager::CleanupCache()
{
    // 메모리 사용량 기준으로 정리
    while (GetCurrentMemoryUsage() > MaxMemoryUsage && LoadedDataTables.Num() > 0)
    {
        RemoveOldestCacheEntry();
    }

    // 캐시 크기 기준으로 정리
    while (LoadedDataTables.Num() > MaxCacheSize)
    {
        RemoveOldestCacheEntry();
    }
}

void UPGDataTableManager::ForceCleanupMemory()
{
    // 모든 캐시 제거
    LoadedDataTables.Empty();
    
    // 가비지 컬렉션 강제 실행
    if (GEngine)
    {
        GEngine->ForceGarbageCollection(true);
    }
}

void UPGDataTableManager::GetCacheInfo(int32& LoadedCount, int32& TotalMemoryUsage) const
{
    LoadedCount = LoadedDataTables.Num();
    TotalMemoryUsage = GetCurrentMemoryUsage();
}

FSoftObjectPath UPGDataTableManager::FindAssetPathByName(const FName& TableName) const
{
    FString TableNameString = TableName.ToString();
    
    for (const FPGDataTableInfo& Info : DataTableInfos)
    {
        if (Info.AssetName == TableNameString)
        {
            return Info.AssetPath;
        }
    }

    return FSoftObjectPath();
}

int32 UPGDataTableManager::GetCurrentMemoryUsage() const
{
    int32 TotalMemory = 0;
    for (const auto& Pair : LoadedDataTables)
    {
        TotalMemory += Pair.Value.MemoryUsage;
    }
    return TotalMemory;
}

void UPGDataTableManager::RemoveOldestCacheEntry()
{
    if (LoadedDataTables.Num() == 0)
    {
        return;
    }

    // 가장 오래된 항목 찾기 (LRU)
    FName OldestTableName;
    double OldestTime = DBL_MAX;

    for (const auto& Pair : LoadedDataTables)
    {
        if (Pair.Value.LastAccessTime < OldestTime)
        {
            OldestTime = Pair.Value.LastAccessTime;
            OldestTableName = Pair.Key;
        }
    }

    // 제거
    if (!OldestTableName.IsNone())
    {
        RemoveCacheEntry(OldestTableName);
    }
}

void UPGDataTableManager::RemoveCacheEntry(const FName& TableName)
{
    if (FPGDataTableCacheEntry* CacheEntry = LoadedDataTables.Find(TableName))
    {
        LoadedDataTables.Remove(TableName);
    }
}

void UPGDataTableManager::StartAutoCleanup()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            CleanupTimerHandle,
            this,
            &UPGDataTableManager::CleanupCache,
            CleanupInterval,
            true
        );
    }
}

void UPGDataTableManager::StopAutoCleanup()
{
    if (GetWorld() && CleanupTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(CleanupTimerHandle);
    }
}
