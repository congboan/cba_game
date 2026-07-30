#pragma once

#include "Engine/DataAsset.h"
#include "ExperiencePawnData.generated.h"

class APawn;
class UExperienceAbilitySet;
class UExperienceAbilityTagRelationshipMapping;
class UExperienceInputConfig;

UCLASS(BlueprintType, Const)
class EXPERIENCEFRAMEWORKRUNTIME_API UExperiencePawnData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UExperiencePawnData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pawn")
	TSubclassOf<APawn> PawnClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<TObjectPtr<const UExperienceAbilitySet>> AbilitySets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UExperienceAbilityTagRelationshipMapping> TagRelationshipMapping;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UExperienceInputConfig> InputConfig;
};
