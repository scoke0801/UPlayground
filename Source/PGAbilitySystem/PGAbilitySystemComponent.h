

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
// Message manager is only required by the implementation.
#include "PGShared/Shared/Message/Base/PGMessageEventDataBase.h"
#include "PGShared/Shared/Structure/PlayerStructTypes.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
#include "PGShared/Shared/Enum/PGRewardTypes.h"
#include "PGAbilitySystemComponent.generated.h"

/**
 * 플레이그라운드 프로젝트의 커스텀 어빌리티 시스템 컴포넌트
 * 플레이어의 입력 처리와 무기 어빌리티 관리를 담당
 */
UCLASS()
class PGABILITYSYSTEM_API UPGAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

private:
	friend class FPGInputBufferTest;
    friend class FPGRoguelikeCombatTest;
    FDelegateHandle DelegateHandle;
    UPROPERTY()
    TObjectPtr<class UPGAtrributeSet> CombatAttributes;
    FActiveGameplayEffectHandle EquipmentEffect;
    FActiveGameplayEffectHandle ProfileEffect;
    bool bStatsInitialized = false;
    TMap<EPGCombatPerk, int32> CombatPerks;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> PreparedBuildEffects;
    TWeakObjectPtr<UPGAbilitySystemComponent> BleedSource;
    FTimerHandle BleedTimer;
    float BleedDamage = 0.f;
    int32 BleedRemaining = 0;
    int32 BleedStacks = 0;
    int32 FrenzyStacks = 0;
    double FrenzyUntil = 0.;
    double NextShockAt = 0.;
    double NextFrenzyVFXAt = 0.;
    bool bHeavySkill = false;
    void TickBleed();
    void AddBleed(UPGAbilitySystemComponent* Source, float Damage);
    void Pulse(const FVector& Center, float Damage, float Radius, bool bSpread);
    void PlayBuildVFX(const TSoftObjectPtr<class UNiagaraSystem>& Effect, const FVector& Location);
    double RecoveryExpiresAt = 0.;
    float RecoveryDamageBonus = 0.f;
    FGameplayTag BufferedInput;
    double BufferExpiresAt = 0.;
    FTimerHandle InputBufferTimer;
    bool TryInput(const FGameplayTag& Tag);
    void RetryBufferedInput();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
public:
    float ReceiveProcDamage(UPGAbilitySystemComponent* Source, float Damage);
    float GetFrenzyRate() const;
    void SetHeavySkill(bool bHeavy) { bHeavySkill = bHeavy; }
    UPGAbilitySystemComponent();
    virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
    UPROPERTY(EditDefaultsOnly, Category="PG|Combat")
    TObjectPtr<class UPGCombatTuningData> CombatTuning;
    UPROPERTY(EditDefaultsOnly, Category="PG|Input", meta=(ClampMin="0", ClampMax="0.5"))
    float InputBufferSeconds = 0.12f;
    void InitializeCombatStats(const TMap<EPGStatType, int32>& Stats);
    static FGameplayAttribute AttributeForStat(EPGStatType Type);
    float GetCombatStat(EPGStatType Type) const;
    float GetHealth() const;
    float ReceiveCombatHit(UPGAbilitySystemComponent* Source, EPGDamageType& OutType);
    void SetCombatPerks(const TMap<EPGCombatPerk, int32>& Perks);
    int32 GetPerkPercent(EPGCombatPerk Perk) const;
    void OpenRecoveryWindow(float Duration, float Bonus);
    void CloseRecoveryWindow();
    bool IsRecoveryExposed() const;
    float RestoreHealth(float Amount);
    bool ApplyStatBonus(EPGStatType Type, float Amount);
    void SetProfileBonuses(const TMap<EPGStatType, int32>& Bonuses);
    void SetEquipmentBonuses(const TMap<EPGStatType, int32>& Bonuses);
    void ClearBufferedInput();
    virtual int32 HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload) override;
	/**
	 * 어빌리티 입력이 눌렸을 때 호출되는 함수
	 * @param InInputTag 입력된 게임플레이 태그
	 */
	void OnAbilityInputPressed(const FGameplayTag& InInputTag);
	
	/**
	 * 어빌리티 입력이 해제되었을 때 호출되는 함수
	 * @param InInputTag 해제된 게임플레이 태그
	 */
	void OnAbilityInputReleased(const FGameplayTag& InInputTag);

	/**
	 * 플레이어 무기 어빌리티들을 부여하는 함수
	 * @param InDefaultWeaponAbilities 부여할 기본 무기 어빌리티 배열
	 * @param ApplyLevel 적용할 레벨
	 * @param OutGrandesdAbilitySpecHandles 부여된 어빌리티 스펙 핸들 배열 (출력)
	 */
	UFUNCTION(BlueprintCallable, Category = "PG|Ability", meta = (ApplyLevel ="1"))
	void GrantPlayerWeaponAbilities(const TArray<FPGPlayerAbilitySet>& InDefaultWeaponAbilities, int32 ApplyLevel,
		TArray<FGameplayAbilitySpecHandle>& OutGrandesdAbilitySpecHandles);

	/**
	 * 부여된 플레이어 어빌리티들을 제거하는 함수
	 * @param InSpecHandlesToRemove 제거할 어빌리티 스펙 핸들 배열
	 */
	UFUNCTION(BlueprintCallable, Category = "PG|Ability", meta = (ApplyLevel ="1"))
	void RemoveGrantedPlayerAbilities(UPARAM(ref) TArray<FGameplayAbilitySpecHandle>& InSpecHandlesToRemove);

	UFUNCTION(BlueprintCallable, Category = "PG|Ability")
	bool TryActivateAbilityByTag(FGameplayTag AbilityTagToActivate);

private:
	void OnClickedSkillButton(const IPGEventData* InData);
};
