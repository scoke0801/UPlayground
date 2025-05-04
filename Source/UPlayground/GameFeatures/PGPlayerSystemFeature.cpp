#include "PGPlayerSystemFeature.h"
#include "../Managers/PGManagerSubsystem.h"
#include "../Managers/Character/PGCharacterManager.h"
#include "PGActionManager.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

UPGPlayerSystemFeature::UPGPlayerSystemFeature()
{
}

void UPGPlayerSystemFeature::OnGameFeatureActivating()
{
    Super::OnGameFeatureActivating();
    
    if (UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this))
    {
        ManagerSubsystem = GameInstance->GetSubsystem<UPGManagerSubsystem>();
        
        if (ManagerSubsystem)
        {
            // 캐릭터 매니저 등록
            UPGCharacterManager* CharacterManager = NewObject<UPGCharacterManager>(this, UPGCharacterManager::StaticClass());
            if (CharacterManager)
            {
                ManagerSubsystem->RegisterManager(CharacterManager);
                RegisteredManagers.Add(CharacterManager);
            }
            
            // 액션 매니저 등록
            UPGActionManager* ActionManager = NewObject<UPGActionManager>(this, UPGActionManager::StaticClass());
            if (ActionManager)
            {
                ManagerSubsystem->RegisterManager(ActionManager);
                RegisteredManagers.Add(ActionManager);
            }
            
            UE_LOG(LogTemp, Log, TEXT("PGPlayerSystemFeature Activated - Managers Registered"));
        }
    }
}

void UPGPlayerSystemFeature::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
    // 등록된 매니저들 해제
    if (ManagerSubsystem)
    {
        for (UPGBaseManager* Manager : RegisteredManagers)
        {
            if (Manager)
            {
                ManagerSubsystem->UnregisterManager(Manager->GetManagerName());
            }
        }
    }
    
    RegisteredManagers.Empty();
    ManagerSubsystem = nullptr;
    
    UE_LOG(LogTemp, Log, TEXT("PGPlayerSystemFeature Deactivated"));
    
    Super::OnGameFeatureDeactivating(Context);
}
