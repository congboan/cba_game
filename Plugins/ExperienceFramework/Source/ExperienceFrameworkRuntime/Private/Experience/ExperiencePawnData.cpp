#include "Experience/ExperiencePawnData.h"

UExperiencePawnData::UExperiencePawnData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PawnClass = nullptr;
	TagRelationshipMapping = nullptr;
	InputConfig = nullptr;
}
