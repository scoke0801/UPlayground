#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "PGBlueprintSearchTypes.generated.h"

/**
 * 검색 범위 타입
 */
UENUM(BlueprintType)
enum class EPGSearchScope : uint8
{
	ProjectOnly		UMETA(DisplayName = "프로젝트만"),
	IncludePlugins	UMETA(DisplayName = "플러그인 포함"),
	IncludeEngine	UMETA(DisplayName = "엔진 포함")
};

/**
 * 파일 크기 필터 타입
 */
UENUM(BlueprintType)
enum class EPGFileSizeFilter : uint8
{
	All			UMETA(DisplayName = "모든 크기"),
	Small		UMETA(DisplayName = "작은 파일 (< 1MB)"),
	Medium		UMETA(DisplayName = "중간 파일 (1MB - 10MB)"),
	Large		UMETA(DisplayName = "큰 파일 (> 10MB)"),
	Custom		UMETA(DisplayName = "사용자 정의")
};

/**
 * 수정일 필터 타입
 */
UENUM(BlueprintType)
enum class EPGDateFilter : uint8
{
	All			UMETA(DisplayName = "모든 날짜"),
	Today		UMETA(DisplayName = "오늘"),
	LastWeek	UMETA(DisplayName = "지난 주"),
	LastMonth	UMETA(DisplayName = "지난 달"),
	LastYear	UMETA(DisplayName = "지난 해"),
	Custom		UMETA(DisplayName = "사용자 정의")
};

/**
 * 정렬 기준
 */
UENUM(BlueprintType)
enum class EPGSortCriteria : uint8
{
	Name			UMETA(DisplayName = "이름"),
	ParentClass		UMETA(DisplayName = "부모 클래스"),
	Path			UMETA(DisplayName = "경로"),
	Size			UMETA(DisplayName = "크기"),
	LastModified	UMETA(DisplayName = "수정일")
};

/**
 * 정렬 방향
 */
UENUM(BlueprintType)
enum class EPGSortDirection : uint8
{
	Ascending	UMETA(DisplayName = "오름차순"),
	Descending	UMETA(DisplayName = "내림차순")
};

/**
 * 고급 필터 설정
 */
USTRUCT(BlueprintType)
struct PGBLUEPRINTUTIL_API FPGAdvancedFilter
{
	GENERATED_BODY()

	/** 파일 크기 필터 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Filter")
	EPGFileSizeFilter FileSizeFilter = EPGFileSizeFilter::All;

	/** 사용자 정의 최소 크기 (바이트) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Filter")
	int64 CustomMinSize = 0;

	/** 사용자 정의 최대 크기 (바이트) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Filter")
	int64 CustomMaxSize = 0;

	/** 수정일 필터 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Filter")
	EPGDateFilter DateFilter = EPGDateFilter::All;

	/** 사용자 정의 시작 날짜 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Filter")
	FDateTime CustomStartDate;

	/** 사용자 정의 종료 날짜 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Filter")
	FDateTime CustomEndDate;

	/** 정렬 기준 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Filter")
	EPGSortCriteria SortCriteria = EPGSortCriteria::Name;

	/** 정렬 방향 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Filter")
	EPGSortDirection SortDirection = EPGSortDirection::Ascending;

	/** 경로 필터 (정규식 지원) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Filter")
	FString PathFilter;

	/** 이름 필터 (정규식 지원) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Filter")
	FString NameFilter;

	FPGAdvancedFilter()
	{
		CustomStartDate = FDateTime::MinValue();
		CustomEndDate = FDateTime::MaxValue();
	}

	/** 필터가 활성화되어 있는지 확인 */
	bool HasActiveFilters() const
	{
		return FileSizeFilter != EPGFileSizeFilter::All ||
			   DateFilter != EPGDateFilter::All ||
			   !PathFilter.IsEmpty() ||
			   !NameFilter.IsEmpty();
	}
};

/**
 * 블루프린트 검색 조건
 */
USTRUCT(BlueprintType)
struct PGBLUEPRINTUTIL_API FPGBlueprintSearchCriteria
{
	GENERATED_BODY()

	/** 부모 클래스 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
	TObjectPtr<UClass> ParentClass = nullptr;

	/** 검색 범위 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search")
	EPGSearchScope SearchScope = EPGSearchScope::ProjectOnly;

	/** 추상 클래스 포함 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bIncludeAbstractClasses = true;

	/** Deprecated 클래스 포함 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bIncludeDeprecated = false;

	/** 정확한 부모 클래스만 검색 (상속 계층 제외) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bExactMatchOnly = false;

	/** 고급 필터 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced")
	FPGAdvancedFilter AdvancedFilter;

	FPGBlueprintSearchCriteria()
	{
	}

	/** 검색 조건이 유효한지 확인 */
	bool IsValid() const
	{
		return ParentClass != nullptr;
	}
};

/**
 * 블루프린트 검색 결과
 */
USTRUCT(BlueprintType)
struct PGBLUEPRINTUTIL_API FPGBlueprintSearchResult
{
	GENERATED_BODY()

	/** 블루프린트 에셋 경로 */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FSoftObjectPath BlueprintAssetPath;

	/** 블루프린트 이름 */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString BlueprintName;

	/** 부모 클래스 이름 */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString ParentClassName;

	/** 에셋 패키지 경로 */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString AssetPath;

	/** 마지막 수정 시간 */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FDateTime LastModified;

	/** 파일 크기 (바이트) */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int64 FileSize = 0;

	/** 생성 시간 */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FDateTime CreationTime;

	/** 블루프린트 타입 (Actor, Component 등) */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString BlueprintType;

#if WITH_EDITORONLY_DATA
	/** 원본 에셋 데이터 */
	FAssetData AssetData;
#endif

	FPGBlueprintSearchResult()
	{
		LastModified = FDateTime::MinValue();
		CreationTime = FDateTime::MinValue();
	}

#if WITH_EDITOR
	/** AssetData로부터 결과 생성 */
	static FPGBlueprintSearchResult FromAssetData(const FAssetData& InAssetData, UClass* InParentClass);

	/** 에셋이 유효한지 확인 */
	bool IsValid() const
	{
		return !BlueprintAssetPath.IsNull() && !BlueprintName.IsEmpty();
	}

	/** 블루프린트를 로드하고 열기 */
	bool OpenBlueprint() const;

	/** 콘텐츠 브라우저에서 보기 */
	void ShowInContentBrowser() const;

	/** 고급 필터 조건에 맞는지 확인 */
	bool MatchesAdvancedFilter(const FPGAdvancedFilter& Filter) const;
#endif
};

/**
 * 검색 히스토리 항목
 */
USTRUCT(BlueprintType)
struct PGBLUEPRINTUTIL_API FPGSearchHistoryItem
{
	GENERATED_BODY()

	/** 검색 시간 */
	UPROPERTY(BlueprintReadOnly, Category = "History")
	FDateTime SearchTime;

	/** 검색 조건 */
	UPROPERTY(BlueprintReadOnly, Category = "History")
	FPGBlueprintSearchCriteria SearchCriteria;

	/** 검색 결과 수 */
	UPROPERTY(BlueprintReadOnly, Category = "History")
	int32 ResultCount = 0;

	/** 검색 소요 시간 (초) */
	UPROPERTY(BlueprintReadOnly, Category = "History")
	float SearchDuration = 0.0f;

	FPGSearchHistoryItem()
	{
		SearchTime = FDateTime::Now();
	}

	/** 히스토리 표시용 텍스트 생성 */
	FString GetDisplayText() const
	{
		return FString::Printf(TEXT("%s - %s (%d개 결과)"),
			*SearchTime.ToString(TEXT("%m/%d %H:%M")),
			SearchCriteria.ParentClass ? *SearchCriteria.ParentClass->GetName() : TEXT("None"),
			ResultCount);
	}
};

/**
 * 북마크 항목
 */
USTRUCT(BlueprintType)
struct PGBLUEPRINTUTIL_API FPGBookmarkItem
{
	GENERATED_BODY()

	/** 북마크 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bookmark")
	FString BookmarkName;

	/** 검색 조건 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bookmark")
	FPGBlueprintSearchCriteria SearchCriteria;

	/** 생성 시간 */
	UPROPERTY(BlueprintReadOnly, Category = "Bookmark")
	FDateTime CreationTime;

	/** 설명 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bookmark")
	FString Description;

	FPGBookmarkItem()
	{
		CreationTime = FDateTime::Now();
	}

	/** 북마크가 유효한지 확인 */
	bool IsValid() const
	{
		return !BookmarkName.IsEmpty() && SearchCriteria.IsValid();
	}
};
