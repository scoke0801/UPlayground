#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

/**
 * PGBlueprintUtil 에디터 명령어 정의
 */
class PGBLUEPRINTUTIL_API FPGBlueprintUtilCommands : public TCommands<FPGBlueprintUtilCommands>
{
public:
	FPGBlueprintUtilCommands()
		: TCommands<FPGBlueprintUtilCommands>(
			TEXT("PGBlueprintUtil"),
			NSLOCTEXT("PGBlueprintUtil", "PGBlueprintUtilCommands", "PG Blueprint Utilities"),
			NAME_None,
			FAppStyle::GetAppStyleSetName()
		)
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	/** 부모 클래스별 블루프린트 검색 명령어 */
	TSharedPtr<FUICommandInfo> OpenBlueprintSearch;
};

#endif // WITH_EDITOR
