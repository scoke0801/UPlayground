#include "PGBlueprintSearchTool.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Async/Async.h"
#include "PGBlueprintUtil.h"

UPGBlueprintSearchTool::UPGBlueprintSearchTool()
{
	bIsSearching = false;
	bCancelRequested = false;
}

TArray<FPGBlueprintSearchResult> UPGBlueprintSearchTool::SearchBlueprints(const FPGBlueprintSearchCriteria& SearchCriteria)
{
	if (!SearchCriteria.IsValid())
	{
		UE_LOG(LogPGBlueprintUtil, Warning, TEXT("SearchBlueprints: Invalid search criteria"));
		return TArray<FPGBlueprintSearchResult>();
	}

	UE_LOG(LogPGBlueprintUtil, Log, TEXT("SearchBlueprints: Starting search for parent class: %s"), 
		   SearchCriteria.ParentClass ? *SearchCriteria.ParentClass->GetName() : TEXT("None"));

	TArray<FPGBlueprintSearchResult> Results;

	// AssetRegistry 가져오기
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// 블루프린트 에셋 필터 설정
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	// 검색 범위에 따른 패키지 경로 설정
	switch (SearchCriteria.SearchScope)
	{
	case EPGSearchScope::ProjectOnly:
		Filter.PackagePaths.Add("/Game");
		break;
	case EPGSearchScope::IncludePlugins:
		Filter.PackagePaths.Add("/Game");
		// 플러그인 경로들도 추가
		Filter.PackagePaths.Add("/Plugins");
		break;
	case EPGSearchScope::IncludeEngine:
		// 모든 경로 포함 (필터 없음)
		break;
	}

	// 에셋 검색
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	UE_LOG(LogPGBlueprintUtil, Log, TEXT("SearchBlueprints: Found %d total blueprint assets"), AssetDataList.Num());

	// 진행률 추적
	int32 ProcessedCount = 0;
	int32 TotalCount = AssetDataList.Num();

	// 각 에셋 검토
	for (const FAssetData& AssetData : AssetDataList)
	{
		// 취소 요청 확인
		if (bCancelRequested)
		{
			UE_LOG(LogPGBlueprintUtil, Log, TEXT("SearchBlueprints: Search cancelled by user"));
			break;
		}

		// 검색 조건과 일치하는지 확인
		if (DoesAssetMatchCriteria(AssetData, SearchCriteria))
		{
			FPGBlueprintSearchResult Result = FPGBlueprintSearchResult::FromAssetData(AssetData, SearchCriteria.ParentClass);
			Results.Add(Result);
		}

		ProcessedCount++;
		if (ProcessedCount % 50 == 0) // 50개마다 진행률 업데이트
		{
			float Progress = static_cast<float>(ProcessedCount) / TotalCount;
			OnSearchProgressUpdated.Broadcast(Progress);
		}
	}

	// 이름 순으로 기본 정렬
	Results.Sort([](const FPGBlueprintSearchResult& A, const FPGBlueprintSearchResult& B)
	{
		return A.BlueprintName < B.BlueprintName;
	});

	UE_LOG(LogPGBlueprintUtil, Log, TEXT("SearchBlueprints: Search completed. Found %d matching blueprints"), Results.Num());

	return Results;
}

void UPGBlueprintSearchTool::SearchBlueprintsAsync(const FPGBlueprintSearchCriteria& SearchCriteria)
{
	if (bIsSearching)
	{
		UE_LOG(LogPGBlueprintUtil, Warning, TEXT("SearchBlueprintsAsync: Search already in progress"));
		return;
	}

	bIsSearching = true;
	bCancelRequested = false;

	// 게임 스레드에서 AssetRegistry 접근하여 에셋 목록 가져오기
	GetAssetListOnGameThread(SearchCriteria);
}

void UPGBlueprintSearchTool::CancelSearch()
{
	if (bIsSearching)
	{
		bCancelRequested = true;
		UE_LOG(LogPGBlueprintUtil, Log, TEXT("CancelSearch: Search cancellation requested"));
	}
}

bool UPGBlueprintSearchTool::DoesAssetMatchCriteria(const FAssetData& AssetData, const FPGBlueprintSearchCriteria& SearchCriteria) const
{
	// 에셋 경로가 검색 범위에 포함되는지 확인
	if (!IsAssetInSearchScope(AssetData.PackageName, SearchCriteria.SearchScope))
	{
		return false;
	}

	// 블루프린트 부모 클래스 가져오기
	UClass* BlueprintParentClass = GetBlueprintParentClass(AssetData);
	if (!BlueprintParentClass)
	{
		return false;
	}

	// Deprecated 클래스 확인
	if (!SearchCriteria.bIncludeDeprecated && BlueprintParentClass->HasAnyClassFlags(CLASS_Deprecated))
	{
		return false;
	}

	// 추상 클래스 확인
	if (!SearchCriteria.bIncludeAbstractClasses && BlueprintParentClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return false;
	}

	// 부모 클래스 일치 확인
	if (SearchCriteria.bExactMatchOnly)
	{
		// 정확한 일치만
		return BlueprintParentClass == SearchCriteria.ParentClass;
	}
	else
	{
		// 상속 계층 포함
		return BlueprintParentClass->IsChildOf(SearchCriteria.ParentClass);
	}
}

UClass* UPGBlueprintSearchTool::GetBlueprintParentClass(const FAssetData& AssetData) const
{
	// Blueprint의 ParentClass 태그에서 부모 클래스 정보 가져오기
	FAssetTagValueRef ParentClassRef = AssetData.TagsAndValues.FindTag("ParentClass");
	if (!ParentClassRef.IsSet() || ParentClassRef.AsString().IsEmpty())
	{
		return nullptr;
	}

	// 클래스 경로에서 실제 클래스 찾기
	UClass* ParentClass = FindObject<UClass>(nullptr, *ParentClassRef.AsString());
	return ParentClass;
}

bool UPGBlueprintSearchTool::IsAssetInSearchScope(const FName& PackageName, EPGSearchScope SearchScope) const
{
	FString PackageNameStr = PackageName.ToString();

	switch (SearchScope)
	{
	case EPGSearchScope::ProjectOnly:
		return PackageNameStr.StartsWith(TEXT("/Game/"));
		
	case EPGSearchScope::IncludePlugins:
		return PackageNameStr.StartsWith(TEXT("/Game/")) || 
			   PackageNameStr.StartsWith(TEXT("/Plugins/")) ||
			   PackageNameStr.Contains(TEXT("Plugins"));
		
	case EPGSearchScope::IncludeEngine:
		return true; // 모든 경로 포함
		
	default:
		return false;
	}
}

void UPGBlueprintSearchTool::GetAssetListOnGameThread(const FPGBlueprintSearchCriteria& SearchCriteria)
{
	// 게임 스레드에서 AssetRegistry 접근
	check(IsInGameThread());

	// AssetRegistry 가져오기
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// 블루프린트 에셋 필터 설정
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	// 검색 범위에 따른 패키지 경로 설정
	switch (SearchCriteria.SearchScope)
	{
	case EPGSearchScope::ProjectOnly:
		Filter.PackagePaths.Add("/Game");
		break;
	case EPGSearchScope::IncludePlugins:
		Filter.PackagePaths.Add("/Game");
		// 플러그인 경로들도 추가
		Filter.PackagePaths.Add("/Plugins");
		break;
	case EPGSearchScope::IncludeEngine:
		// 모든 경로 포함 (필터 없음)
		break;
	}

	// 에셋 검색
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	UE_LOG(LogPGBlueprintUtil, Log, TEXT("GetAssetListOnGameThread: Found %d total blueprint assets"), AssetDataList.Num());

	// 백그라운드 스레드에서 필터링 수행
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, SearchCriteria, AssetDataList]()
	{
		PerformAsyncSearch(SearchCriteria, AssetDataList);
	});
}

void UPGBlueprintSearchTool::PerformAsyncSearch(FPGBlueprintSearchCriteria SearchCriteria, TArray<FAssetData> AssetDataList)
{
	// 백그라운드에서 필터링 수행
	TArray<FAssetData> MatchingAssets;

	// 진행률 추적
	int32 ProcessedCount = 0;
	int32 TotalCount = AssetDataList.Num();

	// 각 에셋 검토
	for (const FAssetData& AssetData : AssetDataList)
	{
		// 취소 요청 확인
		if (bCancelRequested)
		{
			UE_LOG(LogPGBlueprintUtil, Log, TEXT("PerformAsyncSearch: Search cancelled by user"));
			break;
		}

		// 검색 조건과 일치하는지 확인 (스레드 안전)
		if (DoesAssetMatchCriteriaThreadSafe(AssetData, SearchCriteria))
		{
			MatchingAssets.Add(AssetData);
		}

		ProcessedCount++;
		if (ProcessedCount % 50 == 0) // 50개마다 진행률 업데이트
		{
			float Progress = static_cast<float>(ProcessedCount) / TotalCount;
			// 게임 스레드에서 진행률 업데이트
			AsyncTask(ENamedThreads::GameThread, [this, Progress]()
			{
				OnSearchProgressUpdated.Broadcast(Progress);
			});
		}
	}

	// 게임 스레드에서 최종 결과 생성 및 완료 처리
	AsyncTask(ENamedThreads::GameThread, [this, SearchCriteria, MatchingAssets]()
	{
		TArray<FPGBlueprintSearchResult> Results;
		
		// 게임 스레드에서 안전하게 결과 생성
		for (const FAssetData& AssetData : MatchingAssets)
		{
			FPGBlueprintSearchResult Result = FPGBlueprintSearchResult::FromAssetData(AssetData, SearchCriteria.ParentClass);
			Results.Add(Result);
		}

		// 이름 순으로 기본 정렬
		Results.Sort([](const FPGBlueprintSearchResult& A, const FPGBlueprintSearchResult& B)
		{
			return A.BlueprintName < B.BlueprintName;
		});

		bIsSearching = false;
		bCancelRequested = false;
		OnSearchCompleted.Broadcast(Results);
		UE_LOG(LogPGBlueprintUtil, Log, TEXT("PerformAsyncSearch: Search completed. Found %d matching blueprints"), Results.Num());
	});
}

bool UPGBlueprintSearchTool::DoesAssetMatchCriteriaThreadSafe(const FAssetData& AssetData, const FPGBlueprintSearchCriteria& SearchCriteria) const
{
	// 에셋 경로가 검색 범위에 포함되는지 확인
	if (!IsAssetInSearchScope(AssetData.PackageName, SearchCriteria.SearchScope))
	{
		return false;
	}

	// ParentClass 태그에서 부모 클래스 정보 가져오기 (스레드 안전)
	FAssetTagValueRef ParentClassRef = AssetData.TagsAndValues.FindTag("ParentClass");
	if (!ParentClassRef.IsSet() || ParentClassRef.AsString().IsEmpty())
	{
		return false;
	}

	// 부모 클래스 이름 비교 (스레드 안전)
	FString ParentClassPath = ParentClassRef.AsString();
	FString TargetClassName = SearchCriteria.ParentClass->GetPathName();

	if (SearchCriteria.bExactMatchOnly)
	{
		// 정확한 일치만
		return ParentClassPath == TargetClassName;
	}
	else
	{
		// 상속 계층 포함 - 클래스 이름이 포함되어 있는지 확인
		// 이는 완전하지 않지만 스레드 안전한 방법입니다
		FString TargetClassSimpleName = SearchCriteria.ParentClass->GetName();
		return ParentClassPath.Contains(TargetClassSimpleName) || ParentClassPath == TargetClassName;
	}
}

#endif // WITH_EDITOR
