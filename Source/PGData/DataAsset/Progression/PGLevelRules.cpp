#include "PGLevelRules.h"

FPGLevelRules::FPGLevelRules()
{
    InvestmentRules.Add(EPGStatType::Attack, FPGInvestmentRule());
    InvestmentRules.Add(EPGStatType::Defense, FPGInvestmentRule());
    FPGInvestmentRule Health; Health.AmountPerPoint = 20;
    InvestmentRules.Add(EPGStatType::Health, Health);
    for (int32 Level = 1; Level < 20; ++Level)
    {
        FPGLevelStep Step;
        Step.RequiredExperience = 100 * Level;
        Steps.Add(Step);
    }
}
bool FPGLevelRules::IsValid() const
{
    if (InvestmentRules.Num() != 3) return false;
    for (const auto Stat : {EPGStatType::Attack, EPGStatType::Defense, EPGStatType::Health})
    {
        const auto* Rule = InvestmentRules.Find(Stat);
        if (!Rule || Rule->AmountPerPoint < 1 || Rule->AmountPerPoint > 10000 || Rule->MaxPoints < 1 || Rule->MaxPoints > 10000) return false;
    }
    if (Steps.Num() > 999 || StartingLevel < 1 || StartingLevel > Steps.Num() + 1 ||
        StartingPoints < 0 || KillExperience < 0 || RewardExperience < 0) return false;
    int64 Points = StartingPoints;
    for (const auto& Step : Steps)
    {
        if (Step.RequiredExperience <= 0 || Step.GrantedPoints < 0) return false;
        Points += Step.GrantedPoints;
    }
    return Points <= MAX_int32;
}
int32 FPGLevelRules::InvestmentBonus(const FPGLevelState& State, EPGStatType Stat) const
{
    const auto* Rule = InvestmentRules.Find(Stat);
    return Rule ? int32(FMath::Clamp<int64>(int64(State.Investments.FindRef(Stat)) * Rule->AmountPerPoint, 0, MAX_int32)) : 0;
}
bool FPGLevelRules::Invest(FPGLevelState& State, const TMap<EPGStatType, int32>& Draft) const
{
    if (!Validate(State)) return false;
    int64 Spent = 0;
    for (const auto& Pair : Draft)
    {
        const auto* Rule = InvestmentRules.Find(Pair.Key);
        if (!Rule || Pair.Value < 0 || int64(State.Investments.FindRef(Pair.Key)) + Pair.Value > Rule->MaxPoints) return false;
        Spent += Pair.Value;
    }
    if (Spent <= 0 || Spent > State.UnspentPoints) return false;
    FPGLevelState Next = State;
    for (const auto& Pair : Draft) Next.Investments.FindOrAdd(Pair.Key) += Pair.Value;
    Next.UnspentPoints -= int32(Spent);
    if (!Validate(Next)) return false;
    State = MoveTemp(Next);
    return true;
}
FPGLevelState FPGLevelRules::InitialState() const
{
    FPGLevelState State;
    State.Level = StartingLevel;
    State.EarnedPoints = State.UnspentPoints = StartingPoints;
    return State;
}
bool FPGLevelRules::Validate(const FPGLevelState& State) const
{
    if (!IsValid() || State.Level < StartingLevel || State.Level > Steps.Num() + 1 ||
        State.Experience < 0 || State.EarnedPoints < 0 || State.UnspentPoints < 0) return false;
    if (State.Level == Steps.Num() + 1 ? State.Experience != 0 : State.Experience >= Steps[State.Level - 1].RequiredExperience) return false;
    int64 Total = State.UnspentPoints;
    for (const auto& Pair : State.Investments)
    {
        // Preserve investments when tuning a cap down, but reject values outside the supported data range.
        if ((Pair.Key != EPGStatType::Attack && Pair.Key != EPGStatType::Defense && Pair.Key != EPGStatType::Health) || Pair.Value < 0 || Pair.Value > 10000) return false;
        Total += Pair.Value;
    }
    return Total == State.EarnedPoints;
}
bool FPGLevelRules::AddExperience(FPGLevelState& State, int64 Amount) const
{
    if (Amount < 0 || !Validate(State)) return false;
    FPGLevelState Next = State;
    while (Next.Level <= Steps.Num() && Amount > 0)
    {
        const auto& Step = Steps[Next.Level - 1];
        const int64 Needed = Step.RequiredExperience - Next.Experience;
        if (Amount < Needed) { Next.Experience += Amount; break; }
        Amount -= Needed;
        if (Next.EarnedPoints > MAX_int32 - Step.GrantedPoints) return false;
        ++Next.Level;
        Next.Experience = 0;
        Next.EarnedPoints += Step.GrantedPoints;
        Next.UnspentPoints += Step.GrantedPoints;
    }
    State = MoveTemp(Next);
    return true;
}
