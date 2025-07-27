


#include "PGPlayerAnimLayer.h"
#include "AnimInstances/LocalPlayer//PGPlayerAnimLayer.h"

#include "PGPlayerAnimInstance.h"

UPGPlayerAnimInstance* UPGPlayerAnimLayer::GetPlayerAnimInstance() const
{
	return Cast<UPGPlayerAnimInstance>(GetOwningComponent()->GetAnimInstance());
}
