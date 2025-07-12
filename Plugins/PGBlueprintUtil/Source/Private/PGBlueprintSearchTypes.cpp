#include "PGBlueprintSearchTypes.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"

FPGBlueprintSearchResult FPGBlueprintSearchResult::FromAssetData(const FAssetData& InAssetData, UClass* InParentClass)
{
	FPGBlueprintSearchResult Result;
	
#if WITH_EDITORONLY_DATA
	Result.AssetData = InAssetData;
#endif
	
	Result.BlueprintAssetPath = FSoftObjectPath(InAssetData.GetSoftObjectPath());
	Result.BlueprintName = InAssetData.AssetName.ToString();
	Result.AssetPath = InAssetData.PackageName.ToString();
	
	// 부모 클래스 이름 설정
	if (InParentClass)
	{
		Result.ParentClassName = InParentClass->GetName();
		Result.BlueprintType = InParentClass->GetClassPathName().ToString();
	}
	
	// 파일 크기 가져오기
	FAssetTagValueRef FileSizeRef = InAssetData.TagsAndValues.FindTag("FileSize");
	if (FileSizeRef.IsSet())
	{
		LexFromString(Result.FileSize, *FileSizeRef.AsString());
	}
	else
	{
		// AssetRegistry에 없으면 직접 파일 크기 확인
		FString PackageFilename;
		if (FPackageName::DoesPackageExist(InAssetData.PackageName.ToString(), &PackageFilename))
		{
			Result.FileSize = IFileManager::Get().FileSize(*PackageFilename);
		}
	}
	
	// 마지막 수정 시간 가져오기
	FAssetTagValueRef LastModifiedRef = InAssetData.TagsAndValues.FindTag("LastModified");
	if (LastModifiedRef.IsSet())
	{
		FDateTime::Parse(LastModifiedRef.AsString(), Result.LastModified);
	}
	
	// 생성 시간 가져오기
	FAssetTagValueRef CreationTimeRef = InAssetData.TagsAndValues.FindTag("CreationTime");
	if (CreationTimeRef.IsSet())
	{
		FDateTime::Parse(CreationTimeRef.AsString(), Result.CreationTime);
	}
	
	return Result;
}

bool FPGBlueprintSearchResult::OpenBlueprint() const
{
	if (!IsValid())
	{
		return false;
	}
	
	// 에셋 에디터 서브시스템을 통해 블루프린트 열기
	if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
	{
		AssetEditorSubsystem->OpenEditorForAsset(BlueprintAssetPath.ToString());
		return true;
	}
	
	return false;
}

void FPGBlueprintSearchResult::ShowInContentBrowser() const
{
	if (!IsValid())
	{
		return;
	}
	
	// 콘텐츠 브라우저에서 에셋 보기
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> AssetsToShow;
	
#if WITH_EDITORONLY_DATA
	AssetsToShow.Add(AssetData);
#else
	// AssetData가 없는 경우 경로로부터 FAssetData 생성
	FAssetData TempAssetData;
	TempAssetData.PackageName = FName(*AssetPath);
	TempAssetData.AssetName = FName(*BlueprintName);
	AssetsToShow.Add(TempAssetData);
#endif
	
	ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToShow);
}

#endif // WITH_EDITOR
