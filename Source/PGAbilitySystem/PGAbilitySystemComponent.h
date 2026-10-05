

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
// Message manager is only required by the implementation.
#include "PGShared/Shared/Message/Base/PGMessageEventDataBase.h"
#include "PGShared/Shared/Structure/PlayerStructTypes.h"
#include "PGShared/Shared/Enum/PGStatEnumTypes.h"
#include "PGShared/Shared/Enum/PGEnumDamageTypes.h"
#include "PGShared/Shared/Enum/PGRewardTypes.h"
#include "PGShared/Shared/Enum/PGSkillEnumTypes.h"
#include "PGShared/Shared/Message/Combat/PGBuildCombatState.h"
#include "PGShared/Shared/Combat/PGSkillCastContext.h"
#include "PGShared/Shared/Combat/PGSkillObservation.h"
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
    friend class FPGBuildKeystoneCombatTest;
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
    // Scoped to a synchronous melee event, never retained by a later projectile or another ability.
    float MeleeDamageMultiplier = 1.f;
    TSharedPtr<FPGSkillCastContext> ScopedCast;
    FPGHitProcPolicy ScopedProcPolicy;
    bool bRefundUsed = false;
    EPGSkillSlot ActiveCombatSlot = EPGSkillSlot::NormalAttack;
    TWeakObjectPtr<UPGAbilitySystemComponent> LastBuildTarget;
    TWeakObjectPtr<UPGAbilitySystemComponent> ShockSource;
    uint32 ShockGeneration = 0;
    uint32 ShockSourceGeneration = 0;
    uint32 BleedGeneration = 0;
    uint32 BleedSourceGeneration = 0;
    int32 ShockHits = 0;
    double ShockStackUntil = 0.;
    double WeaknessUntil = 0.;
    double ShockProcUntil = 0.;
    double RefundProcUntil = 0.;
    int32 RefundSkillID = 0;
    double AfterimageProcUntil = 0.;
    FTimerHandle ShockEchoTimer;
    void RegisterShockHit(UPGAbilitySystemComponent* Source);
    void ProcessProfileProcs(UPGAbilitySystemComponent* Source, float Damage, float Applied);
    bool HasShockWeakness() const;
    float GetEffectiveDefense() const;
    void TickBleed();
    void AddBleed(UPGAbilitySystemComponent* Source, float Damage, bool bSnapshot = false);
    void Pulse(const FVector& Center, float Damage, float Radius, bool bSpread, EPGDamageCause Cause = EPGDamageCause::Shockwave);
    void PlayBuildVFX(const TSoftObjectPtr<class UNiagaraSystem>& Effect, const FVector& Location);
    double RecoveryExpiresAt = 0.;
    float RecoveryDamageBonus = 0.f;
    FGameplayTag BufferedInput;
    bool bNormalAttackHeld = false;
    double BufferExpiresAt = 0.;
    double BufferedInputAt = -1.;
    double ObservedInputAt = -1.;
    TSharedPtr<FPGSkillObservation> ActiveObservation;
    TSharedPtr<FPGSkillObservation> ScopedObservation;
    int32 ScopedHitPhase = INDEX_NONE;
    int32 DamageProcessingDepth = 0;
    FTimerHandle InputBufferTimer;
    bool TryInput(const FGameplayTag& Tag, double InputAt = -1.);
    void RetryBufferedInput();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
public:
    bool IsProcessingDamage() const { return DamageProcessingDepth > 0; }
    TSharedPtr<FPGSkillObservation> BeginSkillObservation(int32 SkillID, const TSharedPtr<FPGSkillCastContext>& Context = nullptr);
    void EndSkillObservation(const TSharedPtr<FPGSkillObservation>& Observation, bool bCancelled);
    FString GetObservedCastId() const;
    void RecordObservedHit(const TSharedPtr<FPGSkillObservation>& Observation, AActor* Target, int32 Phase,
        float Damage, const FVector& Origin, const FVector& Forward);
    void ApplyPlayerMeleeHit(class APGCharacterBase* Target, float DamageMultiplier, bool bHeavyImpact);
    void ApplyPlayerProfileHit(class APGCharacterBase* Target, const TSharedPtr<FPGSkillCastContext>& Context,
        int32 PhaseId, float Multiplier, bool bHeavy, const FPGHitProcPolicy& Policy, float HitStopSeconds);
    float ReceiveProcDamage(UPGAbilitySystemComponent* Source, float Damage, EPGDamageCause Cause = EPGDamageCause::External);
    float GetFrenzyRate() const;
    void SetHeavySkill(bool bHeavy) { bHeavySkill = bHeavy; }
    void BeginCombatSkill(EPGSkillSlot Slot);
    void OnDodgeCommitted();
    FPGBuildCombatState GetBuildCombatState() const;
    UPGAbilitySystemComponent();
    virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
    UPROPERTY(EditDefaultsOnly, Category="PG|Combat")
    TObjectPtr<class UPGCombatTuningData> CombatTuning;
    UPROPERTY(EditDefaultsOnly, Category="PG|Input", meta=(ClampMin="0", ClampMax="0.5"))
    float InputBufferSeconds = 0.18f;
    UPROPERTY(EditDefaultsOnly, Category="PG|Input")
    bool bRepeatNormalAttackWhileHeld = true;
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
    void ClearBufferedInput(bool bClearHeldInput = true);
    void OnAbilityInputHeld(const FGameplayTag& InInputTag);
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
