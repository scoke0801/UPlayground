#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PGBlueprintSearchTypes.h"
#include "PGBlueprintSearchTool.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSearchCompleted, const TArray<FPGBlueprintSearchResult>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSearchProgressUpdated, float);

/**
 * 블루프린트 검색 도구
 * AssetRegistry를 기반으로 블루프린트를 검색하고 필터링합니다.
 */
UCLASS(BlueprintType)
class PGBLUEPRINTUTIL_API UPGBlueprintSearchTool : public UObject
{
	GENERATED_BODY()

public:
	UPGBlueprintSearchTool();

	/**
	 * 블루프린트 검색 실행
	 * @param SearchCriteria 검색 조건
	 * @return 검색된 블루프린트 결과들
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Search")
	TArray<FPGBlueprintSearchResult> SearchBlueprints(const FPGBlueprintSearchCriteria& SearchCriteria);

	/**
	 * 비동기 블루프린트 검색 실행
	 * @param SearchCriteria 검색 조건
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Search")
	void SearchBlueprintsAsync(const FPGBlueprintSearchCriteria& SearchCriteria);

	/**
	 * 현재 검색을 취소합니다
	 */
	UFUNCTION(BlueprintCallable, Category = "Blueprint Search")
	void CancelSearch();

	/**
	 * 검색이 진행 중인지 확인
	 */
	UFUNCTION(BlueprintPure, Category = "Blueprint Search")
	bool IsSearchInProgress() const { return bIsSearching; }

	/** 검색 완료 델리게이트 */
	FOnSearchCompleted OnSearchCompleted;

	/** 검색 진행률 업데이트 델리게이트 */
	FOnSearchProgressUpdated OnSearchProgressUpdated;

protected:
	/**
	 * 에셋 데이터가 검색 조건에 맞는지 확인
	 * @param AssetData 확인할 에셋 데이터
	 * @param SearchCriteria 검색 조건
	 * @return 조건에 맞으면 true
	 */
	bool DoesAssetMatchCriteria(const FAssetData& AssetData, const FPGBlueprintSearchCriteria& SearchCriteria) const;

	/**
	 * 에셋 데이터가 검색 조건에 맞는지 확인 (스레드 안전)
	 * @param AssetData 확인할 에셋 데이터
	 * @param SearchCriteria 검색 조건
	 * @return 조건에 맞으면 true
	 */
	bool DoesAssetMatchCriteriaThreadSafe(const FAssetData& AssetData, const FPGBlueprintSearchCriteria& SearchCriteria) const;

private:
	/** 현재 검색 진행 상태 */
	UPROPERTY(Transient)
	bool bIsSearching = false;

	/** 검색 취소 플래그 */
	bool bCancelRequested = false;

#if WITH_EDITOR
	/**
	 * 블루프린트의 부모 클래스를 가져옵니다
	 * @param AssetData 블루프린트 에셋 데이터
	 * @return 부모 클래스 (찾을 수 없으면 nullptr)
	 */
	UClass* GetBlueprintParentClass(const FAssetData& AssetData) const;

	/**
	 * 에셋 경로가 검색 범위에 포함되는지 확인
	 * @param PackageName 패키지 이름
	 * @param SearchScope 검색 범위
	 * @return 범위에 포함되면 true
	 */
	bool IsAssetInSearchScope(const FName& PackageName, EPGSearchScope SearchScope) const;

	/** 게임 스레드에서 에셋 목록 가져오기 */
	void GetAssetListOnGameThread(const FPGBlueprintSearchCriteria& SearchCriteria);

	/** 비동기 검색 작업 */
	void PerformAsyncSearch(FPGBlueprintSearchCriteria SearchCriteria, TArray<FAssetData> AssetDataList);
#endif
};
