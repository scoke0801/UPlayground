#include "PGProfileSubsystem.h"
#include "PGData/DataAsset/Character/PGCharacterAppearance.h"
#include "PGActor/Components/Rendering/PGCharacterAppearanceComponent.h"
#include "PGRunTelemetrySubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGData/DataAsset/Progression/PGConsumableData.h"
#include "PGData/DataAsset/Progression/PGLootRules.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
#include "PGData/DataTable/Reward/PGRewardStatDataRow.h"
#include "PGActor/Characters/Player/PGCharacterPlayer.h"
#include "PGActor/Handler/Skill/PGSkillHandler.h"
#include "PGAbilitySystem/PGAbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PGMessage/Managaer/PGMessageManager.h"
#include "PGShared/Shared/Enum/PGMessageTypes.h"
#include "PGActor/Manager/PGStageManager.h"
#include "PGActor/Components/Combat/PGPlayerAttackComponent.h"
#include "PGData/DataAsset/Combat/PGPlayerSkillProfile.h"
#include "Animation/AnimMontage.h"
#include "EngineUtils.h"
#include "PGShared/Shared/Tag/PGGamePlayTags.h"

UPGProfileSubsystem* UPGProfileSubsystem::Get(const UObject* Context)
{
    const UWorld* World = Context ? Context->GetWorld() : nullptr;
    return World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UPGProfileSubsystem>() : nullptr;
}
FString UPGProfileSubsystem::SlotName(int32 Index) const
{
    if (!TestSlotPrefix.IsEmpty()) return TestSlotPrefix + FString::FromInt(Index);
    // Independent PIE sessions must never write the shipping profile.
    const UWorld* World = GetWorld();
    const FString Prefix = World && World->WorldType == EWorldType::PIE
        ? FString::Printf(TEXT("PGProfile_PIE%d_"), World->GetPackage()->GetPIEInstanceID()) : TEXT("PGProfile_");
    return Prefix + (Catalog && Catalog->bRoguelikeRuns ? TEXT("Rogue_") : TEXT("")) + FString::FromInt(Index);
}
void UPGProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
    FString TestName;
    if (FParse::Value(FCommandLine::Get(), TEXT("PGTestProfile="), TestName))
    {
        TestName = FPaths::MakeValidFileName(TestName);
        TestSlotPrefix = TEXT("PGTest_") + TestName.Left(40) + TEXT("_");
        int32 RetryCount = -1;
        if (FParse::Value(FCommandLine::Get(), TEXT("PGRetryProbe="), RetryCount)) RetryProbeRemaining = FMath::Clamp(RetryCount, 0, 20);
    }
#endif
    Collection.InitializeDependency<UPGDataTableManager>();
    Catalog = LoadObject<UPGProgressionData>(nullptr, TEXT("/Game/DataCenter/Progression/DA_PGProgression.DA_PGProgression"));
    Profile = NewObject<UPGProfileSave>(this);
    FString Error;
    if (!ValidateCatalog(Error)) { bReadOnly = true; Status = TEXT("장비와 성장 정보를 불러오지 못했습니다. 게임을 다시 실행해 주세요."); UE_LOG(LogTemp, Error, TEXT("PG profile: %s"), *Error); return; }
    for (const auto& Build : Catalog->Builds)
        for (const auto& Entry : Build.Skills)
        {
            const auto* Row = GetGameInstance()->GetSubsystem<UPGDataTableManager>()->GetRowData<FPGSkillDataRow>(Entry.SkillId);
            if (Row) if (UObject* Asset = Row->MontagePath.TryLoad()) PreparedSkillAssets.AddUnique(Asset);
        }
    LoadProfile();
    if (!bReadOnly && Catalog->bRoguelikeRuns && (ActiveSlot < 0 || (Profile->bRunEnded && !Profile->BossReward.Guid.IsValid())))
        if (!BeginNewRun()) bReadOnly = true;
    if (!bReadOnly && !EnsureRunSeed()) bReadOnly = true;
}
void UPGProfileSubsystem::LoadProfile()
{
    Profile = NewObject<UPGProfileSave>(this);
    ActiveSlot = -1;
    bool bAnyFile = false;
    bool bUnsupported = false;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        if (!UGameplayStatics::DoesSaveGameExist(SlotName(Index), 0)) continue;
        bAnyFile = true;
        auto* Loaded = Cast<UPGProfileSave>(UGameplayStatics::LoadGameFromSlot(SlotName(Index), 0));
        if (Loaded && Loaded->Version != 1) bUnsupported = true;
        if (Validate(Loaded) && (ActiveSlot < 0 || Loaded->Revision > Profile->Revision)) { Profile = Loaded; ActiveSlot = Index; }
    }
    bReadOnly = bUnsupported || (bAnyFile && ActiveSlot < 0);
    Status = bReadOnly ? TEXT("저장 파일 손상/버전 불일치: 원본 보존, 저장 중단") : TEXT("획득/장착 즉시 저장 · I 가방 · E 근접 획득");
    if (Profile->BuildId.IsNone()) Profile->BuildId = Catalog->Builds[0].Id;
    // Only a genuinely new profile gets the new authored default. Old empty v1 saves retain their preset.
    if (!bAnyFile && !bReadOnly) Profile->CustomActiveSkills = Catalog->DefaultActiveSkills;
}
bool UPGProfileSubsystem::RecoverSave()
{
    if (!bReadOnly || !Catalog) return false;
    const FString BackupId = TEXT("_Recovery_") + FGuid::NewGuid().ToString();
    for (int32 Index = 0; Index < 2; ++Index)
    {
        if (!UGameplayStatics::DoesSaveGameExist(SlotName(Index), 0)) continue;
        TArray<uint8> Bytes;
        if (!UGameplayStatics::LoadDataFromSlot(Bytes, SlotName(Index), 0) || !UGameplayStatics::SaveDataToSlot(Bytes, SlotName(Index) + BackupId, 0))
        { Status = TEXT("원본 백업 실패: 복구 중단"); return false; }
    }
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this);
    bReadOnly = false;
    if (!Commit(Next)) { bReadOnly = true; return false; }
    const int32 Other = 1 - ActiveSlot;
    if (UGameplayStatics::DoesSaveGameExist(SlotName(Other), 0)) UGameplayStatics::DeleteGameInSlot(SlotName(Other), 0);
    Status = TEXT("원본을 별도 백업하고 마지막 유효 프로필로 복구했습니다");
    return true;
}
bool UPGProfileSubsystem::ValidateCatalog(FString& Error) const
{
    if (Catalog && Catalog->HealingPotion && !Catalog->HealingPotion->IsValidDefinition())
    { Error = TEXT("회복약 설정 범위 오류"); return false; }
    if (!Catalog || Catalog->Items.IsEmpty() || Catalog->Builds.IsEmpty() || Catalog->BagCapacity < 1 || Catalog->BagCapacity > 256 ||
        !FMath::IsFinite(Catalog->PickupRadius) || Catalog->PickupRadius <= 0 || !FMath::IsFinite(Catalog->DropChance) || Catalog->DropChance < 0 || Catalog->DropChance > 1)
    { Error = TEXT("파밍 설정 누락/범위 오류"); return false; }
    TSet<int32> Ids;
    float Weight = 0;
    for (const auto& Item : Catalog->Items)
    {
        if (Item.Id <= 0 || Ids.Contains(Item.Id) || Item.DisplayName.IsEmpty() || !FMath::IsFinite(Item.DropWeight) || Item.DropWeight < 0 || Item.RollBonus < 0 || Item.RollBonus > 10000 || Item.BaseOptions.IsEmpty())
        { Error = TEXT("아이템 정의 오류/중복 ID"); return false; }
        for (auto Pair : Item.BaseOptions) if (Pair.Key <= EPGStatType::None || Pair.Key >= EPGStatType::Max || Pair.Value < 0 || Pair.Value > 100000)
        { Error = TEXT("아이템 옵션 오류"); return false; }
        Ids.Add(Item.Id); Weight += Item.DropWeight;
    }
    if (Weight <= 0 || !FMath::IsFinite(Weight)) { Error = TEXT("드랍 가중치 오류"); return false; }
    if (!PGLootRules::ValidatePools(*Catalog, Error)) return false;
    TSet<FName> Builds;
    auto* Data = GetGameInstance()->GetSubsystem<UPGDataTableManager>();
    TSet<int32> Selectable;
    for (int32 Id : Catalog->SelectableActiveSkills)
    {
        const auto* Row=Data ? Data->GetRowData<FPGSkillDataRow>(Id) : nullptr;
        const auto* SkillProfile=Row ? Row->PlayerProfile.LoadSynchronous() : nullptr;
        FString ProfileError;
        if (Selectable.Contains(Id) || !SkillProfile || !SkillProfile->Validate(Id,ProfileError) || !Cast<UAnimMontage>(Row->MontagePath.TryLoad()))
        { Error=TEXT("자유 장착 스킬 정의 누락/중복/프로필 오류"); return false; }
        Selectable.Add(Id);
    }
    if (!Catalog->DefaultActiveSkills.IsEmpty() && !Catalog->IsValidActiveSelection(Catalog->DefaultActiveSkills))
    { Error=TEXT("기본 액티브 장착 정의 오류"); return false; }
    for (const auto& Build : Catalog->Builds)
    {
        if (Build.Id.IsNone() || Builds.Contains(Build.Id) || Build.Skills.IsEmpty() || Build.RequiredClears < 0) { Error = TEXT("빌드 정의 오류"); return false; }
        Builds.Add(Build.Id);
        TSet<EPGSkillSlot> Slots;
        for (const auto& Skill : Build.Skills)
        {
            if (Slots.Contains(Skill.Slot) || !FMath::IsFinite(Skill.CooldownSeconds) || Skill.CooldownSeconds < -1.f || Skill.CooldownSeconds > 300.f || !FMath::IsFinite(Skill.CooldownScale) || Skill.CooldownScale < 0.1f || Skill.CooldownScale > 10.f || !Data->GetRowData<FPGSkillDataRow>(Skill.SkillId))
            { Error = TEXT("로드아웃 스킬/슬롯/쿨다운 오류"); return false; }
            Slots.Add(Skill.Slot);
        }
    }
    return true;
}
bool UPGProfileSubsystem::Validate(const UPGProfileSave* Candidate) const
{
    if (!Candidate || Candidate->Version != 1 || Candidate->Revision < 0 || Candidate->Checkpoint < 1 || Candidate->ClearedStages < 0 || Candidate->Items.Num() > 256) return false;
    if (!Candidate->CustomActiveSkills.IsEmpty())
    {
        if (!Catalog || !Catalog->IsValidActiveSelection(Candidate->CustomActiveSkills, true)) return false;
    }
    TSet<FGuid> Ids;
    for (const auto& Item : Candidate->Items)
    {
        if (!Item.Guid.IsValid() || Ids.Contains(Item.Guid) || Item.DefinitionId <= 0) return false;
        Ids.Add(Item.Guid);
        for (auto Pair : Item.Options) if (Pair.Key <= EPGStatType::None || Pair.Key >= EPGStatType::Max || Pair.Value < 0 || Pair.Value > 110000) return false;
    }
    for (auto Pair : Candidate->Equipment)
    {
        if (!Ids.Contains(Pair.Value) || Pair.Key > EPGEquipmentSlot::Accessory) return false;
        const auto* Instance = Candidate->Items.FindByPredicate([&](const auto& I){ return I.Guid == Pair.Value; });
        const auto* Def = Catalog ? Catalog->FindItem(Instance->DefinitionId) : nullptr;
        if (Def && Def->Slot != Pair.Key) return false;
    }
    for (auto Pair : Candidate->RewardBonuses) if (Pair.Key <= EPGStatType::None || Pair.Key >= EPGStatType::Max || Pair.Value < 0 || Pair.Value > 1000000) return false;
    for (auto Pair : Candidate->CombatPerks)
        if (Pair.Key <= EPGCombatPerk::None || Pair.Key >= EPGCombatPerk::Max || Pair.Value < 0 || Pair.Value > 100) return false;
    if (Candidate->CompletedRuns < 0 || Candidate->BestStage < 0 || Candidate->RunSeed < 0) return false;
    for (const auto& Id : Candidate->ClaimedLoot) if (!Id.IsValid()) return false;
    const auto& Boss = Candidate->BossReward;
    if (Boss.Guid.IsValid())
    {
        if (!Candidate->bRunEnded || Boss.DefinitionId <= 0 || !Candidate->ClaimedLoot.Contains(Boss.Guid) || Ids.Contains(Boss.Guid)) return false;
        for (auto Pair : Boss.Options) if (Pair.Key <= EPGStatType::None || Pair.Key >= EPGStatType::Max || Pair.Value < 0 || Pair.Value > 110000) return false;
    }
    for (auto Pair : Candidate->SelectedRewards) if (Pair.Key <= 0 || Pair.Value < 1 || Pair.Value > 100) return false;
    for (auto Pair : Candidate->StageRewardCounts) if (Pair.Key < 1 || Pair.Value < 1 || Pair.Value > 3) return false;
    return true;
}
bool UPGProfileSubsystem::Commit(UPGProfileSave* Candidate)
{
    if (bCommitting || bReadOnly || bInjectSaveFailure || !Validate(Candidate)) { Status = TEXT("저장 실패: 변경을 적용하지 않았습니다"); return false; }
    TGuardValue<bool> Committing(bCommitting, true);
    Candidate->Revision = Profile->Revision + 1;
    const int32 Next = ActiveSlot == 0 ? 1 : 0;
    if (!UGameplayStatics::SaveGameToSlot(Candidate, SlotName(Next), 0)) { Status = TEXT("저장 실패: 이전 정상 저장 유지"); return false; }
    // Alternating slots retain the previous committed snapshot if the newest write is interrupted.
    Profile = Candidate; ActiveSlot = Next;
    Status = TEXT("저장 완료");
    RefreshPlayer();
    OnProfileChanged.Broadcast();
    return true;
}
bool UPGProfileSubsystem::TryPickup(const FPGItemInstance& Item)
{
    if (!Catalog || !Catalog->FindItem(Item.DefinitionId) || !Item.Guid.IsValid()) return false;
    if (HasClaimedLoot(Item.Guid)) return false;
    if (Profile->Items.Num() >= Catalog->BagCapacity) { Status = TEXT("가방이 가득 찼습니다. 아이템은 바닥에 남습니다."); return false; }
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this);
    Next->Items.Add(Item); Next->ClaimedLoot.Add(Item.Guid);
    return Commit(Next);
}
bool UPGProfileSubsystem::HasClaimedLoot(FGuid Guid) const
{
    return Profile && (Profile->ClaimedLoot.Contains(Guid) || Profile->Items.ContainsByPredicate([&](const auto& Item){ return Item.Guid == Guid; }));
}
bool UPGProfileSubsystem::QueueBossReward(const FPGItemInstance& Item)
{
    if (!Profile || Profile->bRunEnded || !Catalog || !Catalog->bRoguelikeRuns || !Catalog->FindItem(Item.DefinitionId) || !Item.Guid.IsValid() || HasClaimedLoot(Item.Guid)) return false;
    if (PendingBossReward.Guid.IsValid()) return PendingBossReward.Guid == Item.Guid;
    PendingBossReward = Item;
    return true;
}
bool UPGProfileSubsystem::Equip(FGuid Guid)
{
    const auto* Item = Profile->Items.FindByPredicate([&](const auto& I){ return I.Guid == Guid; });
    const auto* Def = Item && Catalog ? Catalog->FindItem(Item->DefinitionId) : nullptr;
    if (!Def) { Status = TEXT("알 수 없는 아이템: 보관 중, 장착 불가"); return false; }
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this); Next->Equipment.Add(Def->Slot, Guid); return Commit(Next);
}
bool UPGProfileSubsystem::Unequip(EPGEquipmentSlot Slot)
{
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this); Next->Equipment.Remove(Slot); return Commit(Next);
}
bool UPGProfileSubsystem::Discard(FGuid Guid)
{
    for (auto Pair : Profile->Equipment) if (Pair.Value == Guid) { Status = TEXT("장착 중인 아이템은 먼저 해제하세요"); return false; }
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this);
    if (Next->Items.RemoveAll([&](const auto& I){ return I.Guid == Guid; }) != 1) return false;
    return Commit(Next);
}
bool UPGProfileSubsystem::SelectBuild(FName Id)
{
    if (!CanChangeSkills(Status)) return false;
    if (Catalog && Catalog->bRoguelikeRuns && (Profile->Checkpoint > 1 || !Profile->SelectedRewards.IsEmpty()))
    { Status = TEXT("검술은 도전 시작 전에 선택할 수 있습니다"); return false; }
    const auto* Build = Catalog ? Catalog->Builds.FindByPredicate([&](const auto& B){ return B.Id == Id; }) : nullptr;
    if (!Build || Profile->ClearedStages < Build->RequiredClears) { Status = TEXT("빌드 해금 조건을 만족하지 못했습니다"); return false; }
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this); Next->BuildId = Id; Next->CustomActiveSkills.Reset(); return Commit(Next);
}
bool UPGProfileSubsystem::SelectCharacter(FName Id)
{
    if (!CanChangeSkills(Status)) return false;
    auto* Player = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    UPGCharacterAppearance* Selected = nullptr;
    for (const auto& Reference : Catalog->PlayableCharacters)
        if (auto* Appearance = Reference.LoadSynchronous(); Appearance && Appearance->Id == Id)
        { Selected = Appearance; break; }
    if (!Player || !Selected || !Player->AppearanceComponent->CanApply(Selected))
    { Status = TEXT("캐릭터 데이터를 불러오지 못했습니다. 기존 선택을 유지합니다"); return false; }
    if (Profile->CharacterId == Id) return true;
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this);
    Next->CharacterId = Id;
    return Commit(Next);
}
bool UPGProfileSubsystem::CanChangeSkills(FString& Reason) const
{
    Reason = TEXT("전투 준비 또는 웨이브 정비 중에 변경할 수 있습니다");
    if (!Profile || !Catalog || bReadOnly || bCommitting) { Reason = TEXT("저장 처리 중이거나 저장이 차단되었습니다"); return false; }
    if (!GetWorld()) return false;
    auto* Player = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    auto* ASC = Player ? Player->GetPGAbilitySystemComponent() : nullptr;
    if (!ASC || ASC->GetHealth() <= 0 || ASC->IsProcessingDamage() || Player->GetPlayerAttackComponent()->IsRunning()) return false;
    FGameplayTagContainer Actions;
    Actions.AddTag(PGGamePlayTags::Player_Ability_Attack); Actions.AddTag(PGGamePlayTags::Player_Ability_Roll);
    Actions.AddTag(PGGamePlayTags::Player_Ability_Equip_Weapon); Actions.AddTag(PGGamePlayTags::Player_Ability_UnEquip_Weapon);
    for (const auto& Spec : ASC->GetActivatableAbilities())
        if (Spec.IsActive() && Spec.Ability && Spec.Ability->GetAssetTags().HasAny(Actions)) return false;
    for (TActorIterator<APGStageManager> It(GetWorld()); It; ++It)
    {
        if (It->CanEditSkillLoadout())
        { Reason.Reset(); return true; }
    }
    return false;
}
bool UPGProfileSubsystem::SelectActiveSkills(const TArray<int32>& Skills)
{
    if (!CanChangeSkills(Status)) return false;
    if (!Catalog->IsValidActiveSelection(Skills)) { Status = TEXT("서로 다른 액티브 스킬 4개를 선택하세요"); return false; }
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this);
    Next->CustomActiveSkills = Skills;
    if (!Validate(Next)) return false;
    auto* Tables = UPGDataTableManager::Get(this);
    for (int32 Id : Next->CustomActiveSkills)
    {
        const auto* Row = Tables ? Tables->GetRowData<FPGSkillDataRow>(Id) : nullptr;
        const auto* SkillProfile = Row ? Row->PlayerProfile.LoadSynchronous() : nullptr;
        auto* Montage = Row ? Cast<UAnimMontage>(Row->MontagePath.TryLoad()) : nullptr;
        FString Error;
        if (!SkillProfile || !Montage || !SkillProfile->Validate(Id, Error))
        { Status = TEXT("스킬 데이터를 불러오지 못했습니다. 기존 장착을 유지합니다"); return false; }
    }
    return Commit(Next);
}
bool UPGProfileSubsystem::CommitReward(FGuid Token, int32 NextStage, EPGStatType Stat, int32 Amount, EPGCombatPerk Perk, int32 PerkPercent, int32 RewardId, bool bAdvance)
{
    if (!Token.IsValid() || Token == Profile->LastReward || NextStage <= 1 || Amount < 0 || Amount > 100000 || Profile->StageRewardCounts.FindRef(NextStage - 1) >= 3) return false;
    if (PerkPercent < 0 || PerkPercent > 100 || Perk >= EPGCombatPerk::Max || (Perk == EPGCombatPerk::None && PerkPercent != 0)) return false;
    if (RewardId > 0)
    {
        auto* Tables = GetGameInstance()->GetSubsystem<UPGDataTableManager>();
        const auto* Reward = Tables ? Tables->GetRowData<FPGRewardStatDataRow>(RewardId) : nullptr;
        if (!Reward || !IsRewardEligible(*Reward) || Reward->StatType != Stat || Reward->Amount != Amount ||
            Reward->Perk != Perk || Reward->PerkPercent != PerkPercent) return false;
    }
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this);
    if (Perk != EPGCombatPerk::None) Next->CombatPerks.FindOrAdd(Perk) = FMath::Min(100, Next->CombatPerks.FindRef(Perk) + PerkPercent);
    Next->LastReward = Token;
    ++Next->StageRewardCounts.FindOrAdd(NextStage - 1);
    if (bAdvance) { Next->Checkpoint = NextStage; ++Next->ClearedStages; }
    if (RewardId > 0) ++Next->SelectedRewards.FindOrAdd(RewardId);
    if (Amount > 0) Next->RewardBonuses.FindOrAdd(Stat) += Amount;
    return Commit(Next);
}
bool UPGProfileSubsystem::BeginNewRun(int32 RequestedSeed)
{
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this);
    Next->Checkpoint = 1; Next->RewardBonuses.Reset(); Next->CombatPerks.Reset(); Next->LastReward.Invalidate();
    Next->SelectedRewards.Reset(); Next->bRunEnded = false;
    Next->StageRewardCounts.Reset();
    Next->RunId = FGuid::NewGuid(); Next->ClaimedLoot.Reset(); Next->BossReward = FPGItemInstance();
    Next->RunSeed = RequestedSeed > 0 ? RequestedSeed : FMath::RandRange(1, MAX_int32);
    Next->bAssistedRun = false;
#if !UE_BUILD_SHIPPING
    // Fixed seeds only affect explicitly isolated QA profiles, never ordinary saves.
    int32 FixedSeed = 0;
    if (RequestedSeed <= 0 && !TestSlotPrefix.IsEmpty() && FParse::Value(FCommandLine::Get(), TEXT("PGRunSeed="), FixedSeed) && FixedSeed > 0)
        Next->RunSeed = FixedSeed;
    Next->bAssistedRun = RetryProbeRemaining >= 0;
#endif
    if (Catalog && Catalog->bRoguelikeRuns)
    {
        Next->Items.Reset(); Next->Equipment.Reset();
        for (int32 Id : Catalog->StartingItems)
            if (const auto* Def = Catalog->FindItem(Id))
            {
                FPGItemInstance Item; Item.Guid = FGuid::NewGuid(); Item.DefinitionId = Id; Item.Options = Def->BaseOptions;
                Next->Items.Add(Item); Next->Equipment.Add(Def->Slot, Item.Guid);
            }
    }
    if (!Commit(Next)) return false;
    PendingBossReward = FPGItemInstance();
    return true;
}
bool UPGProfileSubsystem::EnsureRunSeed()
{
    if (!Profile || bReadOnly) return false;
    if (Profile->RunSeed > 0 && Profile->RunId.IsValid()) return true;
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this);
    if (Next->RunSeed <= 0) Next->RunSeed = FMath::RandRange(1, MAX_int32);
    if (!Next->RunId.IsValid()) Next->RunId = FGuid::NewGuid();
    return Commit(Next);
}
bool UPGProfileSubsystem::MarkRunAssisted()
{
    if (!Profile || bReadOnly) return false;
    if (!Profile->bAssistedRun)
    {
        auto* Next = DuplicateObject<UPGProfileSave>(Profile, this);
        Next->bAssistedRun = true;
        if (!Commit(Next)) return false;
    }
    if (auto* Telemetry = UPGRunTelemetrySubsystem::Get(this)) Telemetry->MarkAssisted();
    return true;
}
bool UPGProfileSubsystem::EndRun(bool bVictory, int32 Stage)
{
    if (!Catalog || !Catalog->bRoguelikeRuns || Profile->bRunEnded) return true;
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this);
    Next->bRunEnded = true; Next->BestStage = FMath::Max(Next->BestStage, Stage);
    if (bVictory)
    {
        Next->Checkpoint = Stage;
        ++Next->CompletedRuns;
        if (PendingBossReward.Guid.IsValid())
        {
            Next->BossReward = PendingBossReward;
            Next->ClaimedLoot.Add(PendingBossReward.Guid);
        }
    }
    if (!Commit(Next)) return false;
    if (bVictory && Profile->BossReward.Guid.IsValid())
        UE_LOG(LogTemp, Log, TEXT("PGLoot boss committed item=%d guid=%s wins=%d"), Profile->BossReward.DefinitionId,
            *Profile->BossReward.Guid.ToString(), Profile->CompletedRuns);
    PendingBossReward = FPGItemInstance();
    return true;
}
int32 UPGProfileSubsystem::GetEffectivePerk(EPGCombatPerk Perk) const
{
    int32 Value = Profile ? Profile->CombatPerks.FindRef(Perk) : 0;
    if (Profile && Catalog) for (const auto& Item : Profile->Items)
        if (const auto* Def = Catalog->FindItem(Item.DefinitionId))
            if (Profile->Equipment.FindRef(Def->Slot) == Item.Guid) Value += Def->CombatPerks.FindRef(Perk);
    return FMath::Clamp(Value,0,100);
}
bool UPGProfileSubsystem::IsRewardEligible(const FPGRewardStatDataRow& Reward) const
{
    return Profile && Reward.MeetsRequirements([this](EPGCombatPerk Perk) { return GetEffectivePerk(Perk); }) &&
        (Reward.MaxSelections <= 0 || Profile->SelectedRewards.FindRef(Reward.StatId) < Reward.MaxSelections) &&
        (!Reward.bKeystone || Profile->SelectedRewards.FindRef(Reward.StatId) == 0) &&
        !(Reward.Amount == 0 && Reward.Perk != EPGCombatPerk::None && Profile->CombatPerks.FindRef(Reward.Perk) >= 100);
}
bool UPGProfileSubsystem::ConfigureBuildScenario(const TArray<int32>& RewardIds)
{
#if !UE_BUILD_SHIPPING
    FString TestProfile;
    if (!FParse::Value(FCommandLine::Get(),TEXT("PGTestProfile="),TestProfile) || TestProfile.IsEmpty() || !MarkRunAssisted()) return false;
    auto* Tables = GetGameInstance()->GetSubsystem<UPGDataTableManager>();
    if (!Tables) return false;
    auto* Next = DuplicateObject<UPGProfileSave>(Profile,this);
    Next->CombatPerks.Reset(); Next->SelectedRewards.Reset(); Next->RewardBonuses.Reset();
    Next->Equipment.Reset(); Next->StageRewardCounts.Reset(); Next->LastReward.Invalidate();
    Next->Checkpoint=1; Next->bRunEnded=false;
    for (int32 Id : RewardIds)
    {
        const auto* Row = Tables->GetRowData<FPGRewardStatDataRow>(Id);
        if (!Row || Next->SelectedRewards.Contains(Id) || !Row->MeetsRequirements([&](EPGCombatPerk Perk){return Next->CombatPerks.FindRef(Perk);})) return false;
        Next->CombatPerks.FindOrAdd(Row->Perk) += Row->PerkPercent;
        Next->SelectedRewards.Add(Id,1);
    }
    return Commit(Next);
#else
    return false;
#endif
}
bool UPGProfileSubsystem::RestorePlayer(APGCharacterPlayer* Player)
{
    if (!Catalog || !Player || !Player->GetSkillHandler() || !Player->GetPGAbilitySystemComponent()) return false;
    if (!Profile->CharacterId.IsNone())
    {
        bool bApplied = false;
        for (const auto& Reference : Catalog->PlayableCharacters)
            if (auto* Appearance = Reference.LoadSynchronous(); Appearance && Appearance->Id == Profile->CharacterId)
            { bApplied = Player->AppearanceComponent->ApplyAppearance(Appearance); break; }
        if (!bApplied) Status = TEXT("저장된 캐릭터 외형을 불러오지 못해 현재 외형을 유지합니다");
    }
    auto* ASC = Player->GetPGAbilitySystemComponent();
    TMap<EPGStatType, int32> Bonuses = Profile->RewardBonuses;
    TMap<EPGCombatPerk, int32> Perks = Profile->CombatPerks;
    for (const auto& Item : Profile->Items)
    {
        const auto* Def = Catalog->FindItem(Item.DefinitionId);
        const auto* Equipped = Def ? Profile->Equipment.Find(Def->Slot) : nullptr;
        if (Equipped && *Equipped == Item.Guid)
        {
            for (auto Pair : Item.Options) Bonuses.FindOrAdd(Pair.Key) += Pair.Value;
            for (auto Pair : Def->CombatPerks) Perks.FindOrAdd(Pair.Key) += Pair.Value;
        }
    }
    ASC->SetProfileBonuses(Bonuses);
    ASC->SetCombatPerks(Perks);
    const auto* Build = Catalog->Builds.FindByPredicate([&](const auto& B){ return B.Id == Profile->BuildId && B.RequiredClears <= Profile->ClearedStages; });
    if (!Build) { Build = &Catalog->Builds[0]; Status = TEXT("이전 빌드 ID를 찾지 못해 기본 빌드를 적용했습니다"); }
    auto* Handler = Player->GetSkillHandler();
    TArray<FPGLoadoutEntry> Entries = Build->Skills;
    const auto ActiveSkills = Catalog->ResolveActiveSkills(Profile->CustomActiveSkills, Build->Id);
    for (int32 Index = 0; Index < ActiveSkills.Num(); ++Index)
        {
            const auto Slot = PGPlayerSkillSlots::Get(Index);
            // Preserve authored preset cooldowns, including legacy saves with no custom selection.
            if (Profile->CustomActiveSkills.IsEmpty() && Entries.ContainsByPredicate([&](const auto& E){ return E.Slot == Slot && E.SkillId == ActiveSkills[Index]; })) continue;
            Entries.RemoveAll([Slot](const auto& E){ return E.Slot == Slot; });
            FPGLoadoutEntry Entry; Entry.Slot = Slot; Entry.SkillId = ActiveSkills[Index]; Entries.Add(Entry);
        }
    bool bMappingChanged = false;
    // Capture all old IDs before installing new slots (including swaps).
    for (const auto& Entry : Entries)
        if (const auto* Existing=Handler->GetSkillData(Entry.Slot); Existing && Existing->SkillId!=Entry.SkillId)
        { Handler->RemoveSkill(Entry.Slot); bMappingChanged=true; }
    // Keep cooldown history on equipment changes; rebuild only when mapping changes.
    for (const auto& Entry : Entries)
    {
        auto* Skill = Handler->GetSkillData(Entry.Slot);
        if (!Skill || Skill->SkillId != Entry.SkillId)
        {
            Handler->RemoveSkill(Entry.Slot); Handler->AddSkill(Entry.Slot, Entry.SkillId);
            bMappingChanged = true;
        }
        Skill = Handler->GetSkillData(Entry.Slot);
        const auto* Row = GetGameInstance()->GetSubsystem<UPGDataTableManager>()->GetRowData<FPGSkillDataRow>(Entry.SkillId);
        if (Skill && Row) Skill->CoolTime = (Entry.CooldownSeconds >= 0.f ? Entry.CooldownSeconds : Row->SkillCoolTime) * Entry.CooldownScale * (1.f - FMath::Min(50, Perks.FindRef(EPGCombatPerk::Cooldown)) * .01f);
    }
    TArray<EPGSkillSlot> Remove;
    for (auto Pair : Handler->GetAllSkillData()) if (!Entries.ContainsByPredicate([&](const auto& E){ return E.Slot == Pair.Key; })) Remove.Add(Pair.Key);
    for (auto Slot : Remove) Handler->RemoveSkill(Slot);
    if (bMappingChanged) { Handler->ResetCombo(); Player->GetPlayerAttackComponent()->PrepareLoadout(); }
    if (auto* Messages = UPGMessageManager::Get(Player)) Messages->SendMessage(EPGPlayerMessageType::LoadoutChanged, nullptr);
    return true;
}
void UPGProfileSubsystem::RefreshPlayer()
{
    if (!GetWorld()) return;
    if (auto* Player = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0))) RestorePlayer(Player);
}
bool UPGProfileSubsystem::RollDrop(FRandomStream& Random, FPGItemInstance& Out, FName PoolId) const
{
    return Catalog && PGLootRules::Roll(*Catalog, PoolId, Random, Out);
}
