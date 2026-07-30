#pragma once

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/ExperienceAbilitySet.h"
#include "GameFeatures/ExperienceGameFeatureAction_WorldActionBase.h"
#include "ExperienceGameFeatureAction_AddAbilities.generated.h"

class UAttributeSet;
class UActorComponent;
class UDataTable;
class AActor;
struct FComponentRequestHandle;
struct FWorldContext;

USTRUCT(BlueprintType)
struct FExperienceAbilityGrant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability", meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UGameplayAbility> AbilityType;
};

USTRUCT(BlueprintType)
struct FExperienceAttributeSetGrant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UAttributeSet> AttributeSetType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UDataTable> InitializationData;
};

USTRUCT()
struct FExperienceGameFeatureAbilitiesEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Abilities")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<FExperienceAbilityGrant> GrantedAbilities;

	UPROPERTY(EditAnywhere, Category = "Attributes")
	TArray<FExperienceAttributeSetGrant> GrantedAttributes;

	UPROPERTY(EditAnywhere, Category = "Ability Sets", meta = (AssetBundles = "Client,Server"))
	TArray<TSoftObjectPtr<const UExperienceAbilitySet>> GrantedAbilitySets;
};

UCLASS(meta = (DisplayName = "Add Abilities"))
class EXPERIENCEFRAMEWORKRUNTIME_API UExperienceGameFeatureAction_AddAbilities final : public UExperienceGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditAnywhere, Category = "Abilities", meta = (TitleProperty = "ActorClass", ShowOnlyInnerProperties))
	TArray<FExperienceGameFeatureAbilitiesEntry> AbilitiesList;

	struct FActorExtensions
	{
		TArray<FGameplayAbilitySpecHandle> Abilities;
		TArray<UAttributeSet*> Attributes;
		TArray<FExperienceAbilitySet_GrantedHandles> AbilitySetHandles;
	};

	struct FPerContextData
	{
		TMap<AActor*, FActorExtensions> ActiveExtensions;
		TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequests;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;

	void Reset(FPerContextData& ActiveData);
	void HandleActorExtension(AActor* Actor, FName EventName, int32 EntryIndex, FGameFeatureStateChangeContext ChangeContext);
	void AddActorAbilities(AActor* Actor, const FExperienceGameFeatureAbilitiesEntry& AbilitiesEntry, FPerContextData& ActiveData);
	void RemoveActorAbilities(AActor* Actor, FPerContextData& ActiveData);

	template<class ComponentType>
	ComponentType* FindOrAddComponentForActor(AActor* Actor, const FExperienceGameFeatureAbilitiesEntry& AbilitiesEntry, FPerContextData& ActiveData)
	{
		return Cast<ComponentType>(FindOrAddComponentForActor(ComponentType::StaticClass(), Actor, AbilitiesEntry, ActiveData));
	}

	UActorComponent* FindOrAddComponentForActor(UClass* ComponentType, AActor* Actor, const FExperienceGameFeatureAbilitiesEntry& AbilitiesEntry, FPerContextData& ActiveData);
};
