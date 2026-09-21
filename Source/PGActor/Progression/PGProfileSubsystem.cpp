#include "PGProfileSubsystem.h"
#include "PGData/DataAsset/Progression/PGProgressionData.h"
#include "PGData/PGDataTableManager.h"
#include "PGData/DataTable/Skill/PGSkillDataRow.h"
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
    return Prefix + FString::FromInt(Index);
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
    if (!ValidateCatalog(Error)) { bReadOnly = true; Status = Error; UE_LOG(LogTemp, Error, TEXT("PG profile: %s"), *Error); return; }
    for (const auto& Build : Catalog->Builds)
        for (const auto& Entry : Build.Skills)
        {
            const auto* Row = GetGameInstance()->GetSubsystem<UPGDataTableManager>()->GetRowData<FPGSkillDataRow>(Entry.SkillId);
            if (Row) if (UObject* Asset = Row->MontagePath.TryLoad()) PreparedSkillAssets.AddUnique(Asset);
        }
    LoadProfile();
}
void UPGProfileSubsystem::LoadProfile()
{
    Profile = NewObject<UPGProfileSave>(this);
    ActiveSlot = -1;
    bool bAnyFile = false;
    bool bUnsupported = false;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        bAnyFile |= UGameplayStatics::DoesSaveGameExist(SlotName(Index), 0);
        auto* Loaded = Cast<UPGProfileSave>(UGameplayStatics::LoadGameFromSlot(SlotName(Index), 0));
        if (Loaded && Loaded->Version != 1) bUnsupported = true;
        if (Validate(Loaded) && (ActiveSlot < 0 || Loaded->Revision > Profile->Revision)) { Profile = Loaded; ActiveSlot = Index; }
    }
    bReadOnly = bUnsupported || (bAnyFile && ActiveSlot < 0);
    Status = bReadOnly ? TEXT("저장 파일 손상/버전 불일치: 원본 보존, 저장 중단") : TEXT("획득/장착 즉시 저장 · I 가방 · E 근접 획득");
    if (Profile->BuildId.IsNone()) Profile->BuildId = Catalog->Builds[0].Id;
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
    TSet<FName> Builds;
    auto* Data = GetGameInstance()->GetSubsystem<UPGDataTableManager>();
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
    return true;
}
bool UPGProfileSubsystem::Commit(UPGProfileSave* Candidate)
{
    if (bReadOnly || bInjectSaveFailure || !Validate(Candidate)) { Status = TEXT("저장 실패: 변경을 적용하지 않았습니다"); return false; }
    Candidate->Revision = Profile->Revision + 1;
    const int32 Next = ActiveSlot == 0 ? 1 : 0;
    if (!UGameplayStatics::SaveGameToSlot(Candidate, SlotName(Next), 0)) { Status = TEXT("저장 실패: 이전 정상 저장 유지"); return false; }
    // Alternating slots retain the previous committed snapshot if the newest write is interrupted.
    Profile = Candidate; ActiveSlot = Next;
    Status = TEXT("저장 완료");
    RefreshPlayer();
    return true;
}
bool UPGProfileSubsystem::TryPickup(const FPGItemInstance& Item)
{
    if (!Catalog || !Catalog->FindItem(Item.DefinitionId) || !Item.Guid.IsValid()) return false;
    if (Profile->Items.ContainsByPredicate([&](const auto& I){ return I.Guid == Item.Guid; })) return false;
    if (Profile->Items.Num() >= Catalog->BagCapacity) { Status = TEXT("가방이 가득 찼습니다. 아이템은 바닥에 남습니다."); return false; }
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this); Next->Items.Add(Item); return Commit(Next);
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
    const auto* Build = Catalog ? Catalog->Builds.FindByPredicate([&](const auto& B){ return B.Id == Id; }) : nullptr;
    if (!Build || Profile->ClearedStages < Build->RequiredClears) { Status = TEXT("빌드 해금 조건을 만족하지 못했습니다"); return false; }
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this); Next->BuildId = Id; return Commit(Next);
}
bool UPGProfileSubsystem::CommitReward(FGuid Token, int32 NextStage, EPGStatType Stat, int32 Amount, EPGCombatPerk Perk, int32 PerkPercent)
{
    if (!Token.IsValid() || Token == Profile->LastReward || NextStage <= 1 || Amount < 0) return false;
    if (PerkPercent < 0 || PerkPercent > 100 || Perk >= EPGCombatPerk::Max || (Perk == EPGCombatPerk::None && PerkPercent != 0)) return false;
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this);
    if (Perk != EPGCombatPerk::None) Next->CombatPerks.FindOrAdd(Perk) = FMath::Min(100, Next->CombatPerks.FindRef(Perk) + PerkPercent);
    Next->LastReward = Token; Next->Checkpoint = NextStage; ++Next->ClearedStages;
    if (Amount > 0) Next->RewardBonuses.FindOrAdd(Stat) += Amount;
    return Commit(Next);
}
bool UPGProfileSubsystem::BeginNewRun()
{
    auto* Next = DuplicateObject<UPGProfileSave>(Profile, this);
    Next->Checkpoint = 1; Next->RewardBonuses.Reset(); Next->CombatPerks.Reset(); Next->LastReward.Invalidate(); return Commit(Next);
}
bool UPGProfileSubsystem::RestorePlayer(APGCharacterPlayer* Player)
{
    if (!Catalog || !Player || !Player->GetSkillHandler() || !Player->GetPGAbilitySystemComponent()) return false;
    auto* ASC = Player->GetPGAbilitySystemComponent();
    TMap<EPGStatType, int32> Bonuses = Profile->RewardBonuses;
    for (const auto& Item : Profile->Items)
    {
        const auto* Def = Catalog->FindItem(Item.DefinitionId);
        const auto* Equipped = Def ? Profile->Equipment.Find(Def->Slot) : nullptr;
        if (Equipped && *Equipped == Item.Guid) for (auto Pair : Item.Options) Bonuses.FindOrAdd(Pair.Key) += Pair.Value;
    }
    ASC->SetProfileBonuses(Bonuses);
    ASC->SetCombatPerks(Profile->CombatPerks);
    const auto* Build = Catalog->Builds.FindByPredicate([&](const auto& B){ return B.Id == Profile->BuildId && B.RequiredClears <= Profile->ClearedStages; });
    if (!Build) { Build = &Catalog->Builds[0]; Status = TEXT("이전 빌드 ID를 찾지 못해 기본 빌드를 적용했습니다"); }
    auto* Handler = Player->GetSkillHandler();
    // Keep cooldown history on equipment changes; rebuild only when mapping changes.
    for (const auto& Entry : Build->Skills)
    {
        auto* Skill = Handler->GetSkillData(Entry.Slot);
        if (!Skill || Skill->SkillId != Entry.SkillId)
        {
            const float LastUse = Skill ? Skill->LastSkillUsedTime : 0.f;
            Handler->RemoveSkill(Entry.Slot); Handler->AddSkill(Entry.Slot, Entry.SkillId);
            if (auto* Replacement = Handler->GetSkillData(Entry.Slot)) Replacement->LastSkillUsedTime = LastUse;
        }
        Skill = Handler->GetSkillData(Entry.Slot);
        const auto* Row = GetGameInstance()->GetSubsystem<UPGDataTableManager>()->GetRowData<FPGSkillDataRow>(Entry.SkillId);
        if (Skill && Row) Skill->CoolTime = (Entry.CooldownSeconds >= 0.f ? Entry.CooldownSeconds : Row->SkillCoolTime) * Entry.CooldownScale;
    }
    TArray<EPGSkillSlot> Remove;
    for (auto Pair : Handler->GetAllSkillData()) if (!Build->Skills.ContainsByPredicate([&](const auto& E){ return E.Slot == Pair.Key; })) Remove.Add(Pair.Key);
    for (auto Slot : Remove) Handler->RemoveSkill(Slot);
    if (auto* Messages = UPGMessageManager::Get(Player)) Messages->SendMessage(EPGPlayerMessageType::LoadoutChanged, nullptr);
    return true;
}
void UPGProfileSubsystem::RefreshPlayer()
{
    if (!GetWorld()) return;
    if (auto* Player = Cast<APGCharacterPlayer>(UGameplayStatics::GetPlayerPawn(this, 0))) RestorePlayer(Player);
}
bool UPGProfileSubsystem::RollDrop(FRandomStream& Random, FPGItemInstance& Out) const
{
    if (!Catalog || Random.FRand() >= Catalog->DropChance) return false;
    float Total = 0; for (auto& Item : Catalog->Items) Total += Item.DropWeight;
    float Roll = Random.FRand() * Total;
    for (auto& Def : Catalog->Items)
    {
        if (Def.DropWeight <= 0) continue;
        Roll -= Def.DropWeight;
        if (Roll > 0) continue;
        Out.Guid = FGuid::NewGuid(); Out.DefinitionId = Def.Id; Out.Options = Def.BaseOptions;
        // Stable stat ordering makes rolls reproducible independently of TMap layout.
        TArray<EPGStatType> Keys; Out.Options.GetKeys(Keys); Keys.Sort();
        for (auto Key : Keys) Out.Options[Key] += Random.RandRange(0, Def.RollBonus);
        return true;
    }
    return false;
}
