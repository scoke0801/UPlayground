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
#endif
};
