#include "GameFeatures/ExperienceGameFeatureAction_AddAbilities.h"

#include "AbilitySystemComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFeaturesSubsystemSettings.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ExperienceGameFeatures"

void UExperienceGameFeatureAction_AddAbilities::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	FPerContextData& ActiveData = ContextData.FindOrAdd(Context);

	if (!ensureAlways(ActiveData.ActiveExtensions.IsEmpty()) ||
		!ensureAlways(ActiveData.ComponentRequests.IsEmpty()))
	{
		Reset(ActiveData);
	}

	Super::OnGameFeatureActivating(Context);
}

void UExperienceGameFeatureAction_AddAbilities::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	if (FPerContextData* ActiveData = ContextData.Find(Context))
	{
		Reset(*ActiveData);
	}
}

#if WITH_EDITORONLY_DATA
void UExperienceGameFeatureAction_AddAbilities::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData)
{
	Super::AddAdditionalAssetBundleData(AssetBundleData);

	for (const FExperienceGameFeatureAbilitiesEntry& Entry : AbilitiesList)
	{
		for (const FExperienceAbilityGrant& Ability : Entry.GrantedAbilities)
		{
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient, Ability.AbilityType.ToSoftObjectPath().GetAssetPath());
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateServer, Ability.AbilityType.ToSoftObjectPath().GetAssetPath());
		}

		for (const FExperienceAttributeSetGrant& Attributes : Entry.GrantedAttributes)
		{
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient, Attributes.AttributeSetType.ToSoftObjectPath().GetAssetPath());
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateServer, Attributes.AttributeSetType.ToSoftObjectPath().GetAssetPath());
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient, Attributes.InitializationData.ToSoftObjectPath().GetAssetPath());
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateServer, Attributes.InitializationData.ToSoftObjectPath().GetAssetPath());
		}

		for (const TSoftObjectPtr<const UExperienceAbilitySet>& AbilitySet : Entry.GrantedAbilitySets)
		{
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient, AbilitySet.ToSoftObjectPath().GetAssetPath());
			AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateServer, AbilitySet.ToSoftObjectPath().GetAssetPath());
		}
	}
}
#endif

#if WITH_EDITOR
EDataValidationResult UExperienceGameFeatureAction_AddAbilities::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	int32 EntryIndex = 0;
	for (const FExperienceGameFeatureAbilitiesEntry& Entry : AbilitiesList)
	{
		if (Entry.ActorClass.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("EntryHasNullActor", "Null ActorClass at index {0} in AbilitiesList"), FText::AsNumber(EntryIndex)));
		}

		if (Entry.GrantedAbilities.IsEmpty() && Entry.GrantedAttributes.IsEmpty() && Entry.GrantedAbilitySets.IsEmpty())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(LOCTEXT("EntryHasNoAddOns", "Index {0} in AbilitiesList will do nothing"), FText::AsNumber(EntryIndex)));
		}

		int32 AbilityIndex = 0;
		for (const FExperienceAbilityGrant& Ability : Entry.GrantedAbilities)
		{
			if (Ability.AbilityType.IsNull())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("EntryHasNullAbility", "Null AbilityType at index {0} in AbilitiesList[{1}].GrantedAbilities"), FText::AsNumber(AbilityIndex), FText::AsNumber(EntryIndex)));
			}
			++AbilityIndex;
		}

		int32 AttributeIndex = 0;
		for (const FExperienceAttributeSetGrant& Attributes : Entry.GrantedAttributes)
		{
			if (Attributes.AttributeSetType.IsNull())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("EntryHasNullAttributeSet", "Null AttributeSetType at index {0} in AbilitiesList[{1}].GrantedAttributes"), FText::AsNumber(AttributeIndex), FText::AsNumber(EntryIndex)));
			}
			++AttributeIndex;
		}

		int32 AbilitySetIndex = 0;
		for (const TSoftObjectPtr<const UExperienceAbilitySet>& AbilitySet : Entry.GrantedAbilitySets)
		{
			if (AbilitySet.IsNull())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(FText::Format(LOCTEXT("EntryHasNullAbilitySet", "Null AbilitySet at index {0} in AbilitiesList[{1}].GrantedAbilitySets"), FText::AsNumber(AbilitySetIndex), FText::AsNumber(EntryIndex)));
			}
			++AbilitySetIndex;
		}

		++EntryIndex;
	}

	return Result;
}
#endif

void UExperienceGameFeatureAction_AddAbilities::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	UGameInstance* GameInstance = WorldContext.OwningGameInstance;
	FPerContextData& ActiveData = ContextData.FindOrAdd(ChangeContext);

	if (GameInstance && World && World->IsGameWorld())
	{
		if (UGameFrameworkComponentManager* ComponentManager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
		{
			int32 EntryIndex = 0;
			for (const FExperienceGameFeatureAbilitiesEntry& Entry : AbilitiesList)
			{
				if (!Entry.ActorClass.IsNull())
				{
					TSharedPtr<FComponentRequestHandle> ExtensionRequestHandle = ComponentManager->AddExtensionHandler(
						Entry.ActorClass,
						UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(this, &ThisClass::HandleActorExtension, EntryIndex, ChangeContext));
					ActiveData.ComponentRequests.Add(ExtensionRequestHandle);
				}

				++EntryIndex;
			}
		}
	}
}

void UExperienceGameFeatureAction_AddAbilities::Reset(FPerContextData& ActiveData)
{
	while (!ActiveData.ActiveExtensions.IsEmpty())
	{
		auto ExtensionIt = ActiveData.ActiveExtensions.CreateIterator();
		RemoveActorAbilities(ExtensionIt->Key, ActiveData);
	}

	ActiveData.ComponentRequests.Empty();
}

void UExperienceGameFeatureAction_AddAbilities::HandleActorExtension(AActor* Actor, FName EventName, int32 EntryIndex, FGameFeatureStateChangeContext ChangeContext)
{
	FPerContextData* ActiveData = ContextData.Find(ChangeContext);
	if (!AbilitiesList.IsValidIndex(EntryIndex) || !ActiveData)
	{
		return;
	}

	const FExperienceGameFeatureAbilitiesEntry& Entry = AbilitiesList[EntryIndex];
	if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved) || (EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved))
	{
		RemoveActorAbilities(Actor, *ActiveData);
	}
	else if ((EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded) || (EventName == UGameFrameworkComponentManager::NAME_GameActorReady))
	{
		AddActorAbilities(Actor, Entry, *ActiveData);
	}
}

void UExperienceGameFeatureAction_AddAbilities::AddActorAbilities(AActor* Actor, const FExperienceGameFeatureAbilitiesEntry& AbilitiesEntry, FPerContextData& ActiveData)
{
	check(Actor);

	if (!Actor->HasAuthority() || ActiveData.ActiveExtensions.Find(Actor))
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = FindOrAddComponentForActor<UAbilitySystemComponent>(Actor, AbilitiesEntry, ActiveData))
	{
		FActorExtensions AddedExtensions;
		AddedExtensions.Abilities.Reserve(AbilitiesEntry.GrantedAbilities.Num());
		AddedExtensions.Attributes.Reserve(AbilitiesEntry.GrantedAttributes.Num());
		AddedExtensions.AbilitySetHandles.Reserve(AbilitiesEntry.GrantedAbilitySets.Num());

		for (const FExperienceAbilityGrant& Ability : AbilitiesEntry.GrantedAbilities)
		{
			if (TSubclassOf<UGameplayAbility> AbilityClass = Ability.AbilityType.LoadSynchronous())
			{
				FGameplayAbilitySpec NewAbilitySpec(AbilityClass);
				AddedExtensions.Abilities.Add(AbilitySystemComponent->GiveAbility(NewAbilitySpec));
			}
		}

		for (const FExperienceAttributeSetGrant& Attributes : AbilitiesEntry.GrantedAttributes)
		{
			if (TSubclassOf<UAttributeSet> SetType = Attributes.AttributeSetType.LoadSynchronous())
			{
				UAttributeSet* NewSet = NewObject<UAttributeSet>(AbilitySystemComponent->GetOwner(), SetType);
				if (UDataTable* InitData = Attributes.InitializationData.LoadSynchronous())
				{
					NewSet->InitFromMetaDataTable(InitData);
				}

				AddedExtensions.Attributes.Add(NewSet);
				AbilitySystemComponent->AddAttributeSetSubobject(NewSet);
			}
		}

		for (const TSoftObjectPtr<const UExperienceAbilitySet>& SetPtr : AbilitiesEntry.GrantedAbilitySets)
		{
			if (const UExperienceAbilitySet* Set = SetPtr.LoadSynchronous())
			{
				Set->GiveToAbilitySystem(AbilitySystemComponent, &AddedExtensions.AbilitySetHandles.AddDefaulted_GetRef());
			}
		}

		ActiveData.ActiveExtensions.Add(Actor, AddedExtensions);
	}
	else
	{
		UE_LOG(LogGameFeatures, Error, TEXT("Failed to find or add an ability component to '%s'. Abilities will not be granted."), *Actor->GetPathName());
	}
}

void UExperienceGameFeatureAction_AddAbilities::RemoveActorAbilities(AActor* Actor, FPerContextData& ActiveData)
{
	if (FActorExtensions* ActorExtensions = ActiveData.ActiveExtensions.Find(Actor))
	{
		if (UAbilitySystemComponent* AbilitySystemComponent = Actor->FindComponentByClass<UAbilitySystemComponent>())
		{
			for (UAttributeSet* AttributeSetInstance : ActorExtensions->Attributes)
			{
				AbilitySystemComponent->RemoveSpawnedAttribute(AttributeSetInstance);
			}

			for (FGameplayAbilitySpecHandle AbilityHandle : ActorExtensions->Abilities)
			{
				AbilitySystemComponent->SetRemoveAbilityOnEnd(AbilityHandle);
			}

			for (FExperienceAbilitySet_GrantedHandles& SetHandle : ActorExtensions->AbilitySetHandles)
			{
				SetHandle.TakeFromAbilitySystem(AbilitySystemComponent);
			}
		}

		ActiveData.ActiveExtensions.Remove(Actor);
	}
}

UActorComponent* UExperienceGameFeatureAction_AddAbilities::FindOrAddComponentForActor(UClass* ComponentType, AActor* Actor, const FExperienceGameFeatureAbilitiesEntry& AbilitiesEntry, FPerContextData& ActiveData)
{
	UActorComponent* Component = Actor->FindComponentByClass(ComponentType);

	bool bMakeComponentRequest = Component == nullptr;
	if (Component && Component->CreationMethod == EComponentCreationMethod::Native)
	{
		UObject* ComponentArchetype = Component->GetArchetype();
		bMakeComponentRequest = ComponentArchetype->HasAnyFlags(RF_ClassDefaultObject);
	}

	if (bMakeComponentRequest)
	{
		if (UWorld* World = Actor->GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				if (UGameFrameworkComponentManager* ComponentManager = UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
				{
					ActiveData.ComponentRequests.Add(ComponentManager->AddComponentRequest(AbilitiesEntry.ActorClass, ComponentType));
				}
			}
		}

		if (!Component)
		{
			Component = Actor->FindComponentByClass(ComponentType);
			ensureAlways(Component);
		}
	}

	return Component;
}

#undef LOCTEXT_NAMESPACE
