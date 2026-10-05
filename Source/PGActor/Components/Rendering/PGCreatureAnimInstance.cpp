#include "PGCreatureAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "GameFramework/Actor.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"

struct FPGCreatureAnimProxy : FAnimInstanceProxy
{
    explicit FPGCreatureAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    FAnimNode_BlendSpacePlayer_Standalone Movement;
    FAnimNode_Slot Slot;
    virtual FAnimNode_Base* GetCustomRootNode() override { return &Slot; }
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override { Nodes.Add(&Movement); Nodes.Add(&Slot); }
    virtual void Initialize(UAnimInstance* Instance) override
    {
        const auto* Enemy = Cast<APGCharacterEnemy>(Instance->GetOwningActor());
        Movement.SetBlendSpace(Enemy ? Enemy->CreatureLocomotion.Get() : nullptr);
        Movement.SetLoop(true);
        Slot.SlotName = TEXT("DefaultSlot");
        Slot.Source.SetLinkNode(&Movement);
        FAnimInstanceProxy::Initialize(Instance);
    }
    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        const auto* Owner = Instance->GetOwningActor();
        Movement.SetPosition(FVector(Owner ? Owner->GetVelocity().Size2D() : 0.f, 0.f, 0.f));
    }
};

FAnimInstanceProxy* UPGCreatureAnimInstance::CreateAnimInstanceProxy() { return new FPGCreatureAnimProxy(this); }
void UPGCreatureAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
