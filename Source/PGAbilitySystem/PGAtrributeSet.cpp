


#include "AbilitySystem/PGAtrributeSet.h"

#include "GameplayEffectExtension.h"
#include "PGGamePlayTags.h"
#include "Components/UI/PGPawnUIComponent.h"
#include "Components/UI/PGPlayerUIComponent.h"
#include "Helper/Debug/PGDebugHelper.h"
#include "Interfaces/PGPawnUIInterface.h"
#include "Utils/PGFunctionLibrary.h"

/**
 * 기본 생성자
 * 모든 어트리뷰트를 기본값 1.0으로 초기화
 */
UPGAtrributeSet::UPGAtrributeSet()
{
	InitCurrentHealth(1.f);
	InitMaxHealth(1.f);
	InitCurrentRage(1.f);
	InitMaxRage(1.f);
	InitAttackPower(1.f);
	InitDefensePower(1.f);
}

/**
 * 게임플레이 이펙트 실행 후 호출되는 함수
 * 어트리뷰트 값의 유효성 검증 및 클램핑 처리를 수행
 */
void UPGAtrributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	if (false == CachedPawnUIInterface.IsValid())
	{
		CachedPawnUIInterface = TWeakInterfacePtr<IPGPawnUIInterface>(Data.Target.GetAvatarActor());
	}
	check(CachedPawnUIInterface.IsValid());

	UPGPawnUIComponent* PawnUIComponent = CachedPawnUIInterface->GetUIComponent();
	check(PawnUIComponent);
	
	// 현재 체력 어트리뷰트가 변경된 경우, 0과 최대 체력 사이로 클램핑
	if ( Data.EvaluatedData.Attribute == GetCurrentHealthAttribute())
	{
		const float NewCurrentHealth = FMath::Clamp(GetCurrentHealth(), 0.f, GetMaxHealth());

		SetCurrentHealth(NewCurrentHealth);

		PawnUIComponent->OnPercentChanged.Broadcast(GetCurrentHealth() / GetMaxHealth());
	}

	// 현재 분노 어트리뷰트가 변경된 경우, 0과 최대 분노 사이로 클램핑
	if (Data.EvaluatedData.Attribute == GetCurrentRageAttribute())
	{
		const float NewCurrentRage = FMath::Clamp(GetCurrentRage(), 0.f, GetMaxRage());

		SetCurrentRage(NewCurrentRage);

		if (UPGPlayerUIComponent* PlayerUIComponent = CachedPawnUIInterface->GetPlayerUIComponent())
		{
			PlayerUIComponent->OnCurrentRageChanged.Broadcast(GetCurrentRage() / GetMaxRage());
		}
	}

	// 데미지를 받은 경우, 현재 체력에서 데미지만큼 차감
	if (Data.EvaluatedData.Attribute == GetDamageTakenAttribute())
	{
		const float OldHealth = GetCurrentHealth();
		const float DamageDone = GetDamageTaken();

		const float NewCurrentHealth = FMath::Clamp(OldHealth - DamageDone, 0.f, GetMaxHealth());

		SetCurrentHealth(NewCurrentHealth);

		PawnUIComponent->OnPercentChanged.Broadcast(GetCurrentHealth() / GetMaxHealth());
		
		if (GetCurrentHealth() == 0.f)
		{
			UPGFunctionLibrary::AddGameplayTagToActorIfNone(Data.Target.GetAvatarActor(),
				PGGamePlayTags::Shared_Status_Dead);
		}
	}
}
