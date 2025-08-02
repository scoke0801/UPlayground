// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PGMessageDispatcher.generated.h"

class FPGMessageParam
{
public:
	virtual ~FPGMessageParam() {}
};

DECLARE_MULTICAST_DELEGATE_OneParam(FPGMessageDelegate, FPGMessageParam*);


/**
 * 
 */
UCLASS()
class PGMESSAGE_API UPGMessageDispatcher : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
	static TWeakObjectPtr<UPGMessageDispatcher> WeakThis;

public:
	UPGMessageDispatcher();
	
	static UPGMessageDispatcher* Get();
    
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	// 편리한 템플릿 메서드 추가
	template<typename ObjectType>
	void AddUObject(ObjectType* Object, void (ObjectType::*Function)(FPGMessageParam*))
	{
		if (Object && Function)
		{
			FPGMessageDelegate Delegate;
			Delegate.AddUObject(Object, Function);
		}
	}
};
