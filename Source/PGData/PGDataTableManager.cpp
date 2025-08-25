#include "PGDataTableManager.h"

#include "AssetRegistry/AssetRegistryModule.h"

TWeakObjectPtr<UPGDataTableManager> UPGDataTableManager::WeakThis = nullptr;

UPGDataTableManager::UPGDataTableManager()
    : AssetRegistryModule(nullptr)
{
}

UPGDataTableManager* UPGDataTableManager::Get()
{
    if (WeakThis.IsValid())
    {
        return WeakThis.Get();
    }
    return nullptr;
}

void UPGDataTableManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    WeakThis = MakeWeakObjectPtr(this);
    
    // AssetRegistry 모듈 가져오기
    AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    // 데이터 테이블 스캔
    ScanDataTables();

    // 자동 정리 시작.( TODO. State 등 변환 시점마다 필요하다면 정리 하도록 하면 어떨까? )
    StartAutoCleanup();
}

void UPGDataTableManager::Deinitialize()
{
    // 자동 정리 중지
    StopAutoCleanup();

    // 모든 캐시 정리
    LoadedDataTables.Empty();
    DataTableInfos.Empty();
    
    // 구조체 타입 매핑 정리
    StructTypeToTableNameMap.Empty();

    AssetRegistryModule = nullptr;

    WeakThis = nullptr;
    
    Super::Deinitialize();
}

void UPGDataTableManager::ScanDataTables()
{
    if (!AssetRegistryModule)
    {
        UE_LOG(LogTemp, Error, TEXT("AssetRegistryModule이 null입니다!"));
        return;
    }

    // 기존 정보 초기화
    DataTableInfos.Empty();
    StructTypeToTableNameMap.Empty();

    IAssetRegistry& AssetRegistry = AssetRegistryModule->Get();
    /* 
     * AssetRegistry에서 모든 데이터 테이블 에셋 검색
     * 경로는 DataTables 하위 폴더로만 한정.
     */
    FARFilter Filter;
    Filter.ClassPaths.Add(UDataTable::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(TEXT("/Game/DataCenter/DataTables"));
    Filter.bRecursiveClasses = true;
    Filter.bRecursivePaths = true;    

    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);
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
    if (AssetData.GetTagValue(TEXT("RowStructure"), RowStructName))
    {
        Info.RowStructName = RowStructName;
        
        // 구조체 타입 이름으로 실제 UScriptStruct 찾기
        Info.RowStructType = FindStructTypeByName(RowStructName);
        
        if (Info.RowStructType)
        {
            // 구조체 타입 매핑 등록 (테이블명은 에셋 이름 사용)
            FName TableName = FName(*Info.AssetName);
            StructTypeToTableNameMap.Add(Info.RowStructType, TableName);
            UE_LOG(LogTemp, Warning, TEXT("✓ 구조체 매핑 성공: %s -> %s"), 
                *Info.RowStructType->GetName(), *TableName.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("✗ UScriptStruct를 찾을 수 없음: %s"), *RowStructName);
        }
    }
    
    // 예상 메모리 사용량
    Info.EstimatedMemoryUsage = 1024; // 기본값

    DataTableInfos.Add(Info);
    UE_LOG(LogTemp, Warning, TEXT("데이터 테이블 정보 추가 완료.\n"));
}

void UPGDataTableManager::CleanupCache()
{
    // 메모리 사용량 기준으로 정리
    while (GetCurrentMemoryUsage() > MAX_MEMORY_USAGE && LoadedDataTables.Num() > 0)
    {
        RemoveOldestCacheEntry();
    }

    // 캐시 크기 기준으로 정리
    while (LoadedDataTables.Num() > MAX_CACHE_SIZE)
    {
        RemoveOldestCacheEntry();
    }
}

void UPGDataTableManager::ForceCleanupMemory()
{
    // 모든 캐시 제거 (구조체 타입 매핑은 유지)
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

const UScriptStruct* UPGDataTableManager::FindStructTypeByName(const FString& StructTypeName) const
{
    if (StructTypeName.IsEmpty())
    {
        return nullptr;
    }

    // 구조체 타입 이름에서 경로 정보 추출
    // 예: "Script/PGData.PGSkillDataRow" -> "PGSkillDataRow"
    FString StructName = StructTypeName;
    int32 LastDotIndex;
    if (StructName.FindLastChar('.', LastDotIndex))
    {
        StructName = StructName.RightChop(LastDotIndex + 1);
    }

    // UScriptStruct 찾기 - 여러 방법 시도
    const UScriptStruct* FoundStruct = nullptr;

    // 방법 1: StaticFindObject 사용
    FoundStruct = FindObject<UScriptStruct>(nullptr, *StructTypeName);
    if (FoundStruct)
    {
        return FoundStruct;
    }

    // 방법 2: 패키지 경로 없이 구조체 이름만으로 찾기
    FoundStruct = FindObject<UScriptStruct>(nullptr, *StructName);
    if (FoundStruct)
    {
        return FoundStruct;
    }

    // 방법 3: 일반적인 패키지 경로들을 시도
    TArray<FString> CommonPaths = {
        FString::Printf(TEXT("/Script/PGData.%s"), *StructName),
        FString::Printf(TEXT("/Script/UPlayground.%s"), *StructName),
        FString::Printf(TEXT("/Script/Engine.%s"), *StructName)
    };

    for (const FString& Path : CommonPaths)
    {
        FoundStruct = FindObject<UScriptStruct>(nullptr, *Path);
        if (FoundStruct)
        {
            return FoundStruct;
        }
    }

    return nullptr;
}

FName UPGDataTableManager::FindTableNameByStructType(const UScriptStruct* StructType) const
{
    if (const FName* FoundTableName = StructTypeToTableNameMap.Find(StructType))
    {
        return *FoundTableName;
    }
    return FName();
}

void UPGDataTableManager::StartAutoCleanup()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            CleanupTimerHandle,
            this,
            &UPGDataTableManager::CleanupCache,
            CACHE_CLEANUP_INTERVAL,
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

void UPGDataTableManager::BuildSearchKeyIndex(FPGDataTableCacheEntry& CacheEntry)
{
    if (!CacheEntry.DataTable)
    {
        return;
    }

    const UScriptStruct* RowStruct = CacheEntry.DataTable->GetRowStruct();
    if (!RowStruct)
    {
        return;
    }

    // SearchKey 프로퍼티 찾기
    const FProperty* SearchKeyProperty = FindSearchKeyProperty(RowStruct);
    if (!SearchKeyProperty)
    {
        // SearchKey 메타데이터가 없으면 인덱스 생성하지 않음
        return;
    }

    // SearchKey 프로퍼티가 정수 타입인지 검증
    if (!IsIntegerProperty(SearchKeyProperty))
    {
        return;
    }

    // 인덱스 구조체 초기화
    FPGSearchKeyIndex SearchKeyIndex;
    SearchKeyIndex.SearchKeyProperty = SearchKeyProperty;

    // 모든 행을 순회하며 인덱스 생성
    TArray<FName> RowNames = CacheEntry.DataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        uint8* RowData = CacheEntry.DataTable->FindRowUnchecked(RowName);
        if (!RowData)
        {
            continue;
        }

        // 프로퍼티 값 추출
        const void* ValuePtr = SearchKeyProperty->ContainerPtrToValuePtr<void>(RowData);
        int64 SearchKeyValue = 0;
        
        if (PropertyToInteger(SearchKeyProperty, ValuePtr, SearchKeyValue))
        {
            SearchKeyIndex.IndexMap.Add(SearchKeyValue, RowName);
        }
    }

    // 인덱스가 성공적으로 생성되면 유효성 표시
    if (SearchKeyIndex.IndexMap.Num() > 0)
    {
        CacheEntry.SearchKeyIndex = SearchKeyIndex;
    }
}

const FProperty* UPGDataTableManager::FindSearchKeyProperty(const UScriptStruct* RowStruct)
{
    if (!RowStruct)
    {
        return nullptr;
    }

    // 모든 프로퍼티를 순회하며 SearchKey 메타데이터 찾기
    for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
    {
        const FProperty* Property = *It;
        if (!Property)
        {
            continue;
        }

        // SearchKey 메타데이터 확인
        if (Property->HasMetaData(TEXT("SearchKey")))
        {
            const FString& SearchKeyValue = Property->GetMetaData(TEXT("SearchKey"));
            if (SearchKeyValue == TEXT("true") || SearchKeyValue == TEXT("True") || SearchKeyValue == TEXT("1"))
            {
                return Property;
            }
        }
    }

    return nullptr;
}

bool UPGDataTableManager::PropertyToInteger(const FProperty* Property, const void* ValuePtr, int64& OutValue)
{
    if (!Property || !ValuePtr)
    {
        return false;
    }

    if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
    {
        OutValue = NumericProperty->GetSignedIntPropertyValue(ValuePtr);
        return true;
    }

    if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
    {
        if (const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty())
        {
            OutValue = UnderlyingProperty->GetSignedIntPropertyValue(ValuePtr);
            return true;
        }
    }
    return false;
}

bool UPGDataTableManager::IsIntegerProperty(const FProperty* Property)
{
    if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
    {
        return NumericProperty->IsInteger();    
    }

    if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
    {
        if (const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty())
        {
            return true;
        }
    }
    
    return false;
}
