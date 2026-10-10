#include "PGCreatureAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "GameFramework/Actor.h"
#include "PGActor/Characters/NonPlayer/Enemy/PGCharacterEnemy.h"

struct FPGCreatureAnimProxy : FAnimInstanceProxy
{
    explicit FPGCreatureAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    FAnimNode_BlendSpacePlayer_Standalone Movement;
    FAnimNode_Slot Slot;
    bool bDirectional = false;
    float LastDirection = 0.f;
    virtual FAnimNode_Base* GetCustomRootNode() override { return &Slot; }
    virtual void GetCustomNodes(TArray<FAnimNode_Base*>& Nodes) override { Nodes.Add(&Movement); Nodes.Add(&Slot); }
    virtual void Initialize(UAnimInstance* Instance) override
    {
        const auto* Enemy = Cast<APGCharacterEnemy>(Instance->GetOwningActor());
        Movement.SetBlendSpace(Enemy ? Enemy->CreatureLocomotion.Get() : nullptr);
        bDirectional = Enemy && Enemy->CreatureLocomotion && !Enemy->CreatureLocomotion->IsA<UBlendSpace1D>();
        Movement.SetLoop(true);
        Slot.SlotName = TEXT("DefaultSlot");
        Slot.Source.SetLinkNode(&Movement);
        FAnimInstanceProxy::Initialize(Instance);
    }
    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        const auto* Owner = Instance->GetOwningActor();
        const FVector Velocity = Owner ? Owner->GetVelocity() : FVector::ZeroVector;
        const float Speed = Velocity.Size2D();
        if (bDirectional && Owner && Speed > 1.f)
        {
            const FVector Local = Owner->GetActorRotation().UnrotateVector(Velocity);
            LastDirection = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
        }
        Movement.SetPosition(bDirectional ? FVector(LastDirection, Speed, 0.f) : FVector(Speed, 0.f, 0.f));
    }
};

FAnimInstanceProxy* UPGCreatureAnimInstance::CreateAnimInstanceProxy() { return new FPGCreatureAnimProxy(this); }
void UPGCreatureAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
