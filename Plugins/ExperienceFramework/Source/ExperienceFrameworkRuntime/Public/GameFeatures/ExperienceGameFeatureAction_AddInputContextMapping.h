#pragma once

#include "GameFeatures/ExperienceGameFeatureAction_WorldActionBase.h"
#include "UObject/SoftObjectPtr.h"
#include "ExperienceGameFeatureAction_AddInputContextMapping.generated.h"

class AActor;
class APlayerController;
class UGameInstance;
class UInputMappingContext;
class ULocalPlayer;
struct FComponentRequestHandle;

USTRUCT()
struct FExperienceInputMappingContextAndPriority
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UInputMappingContext> InputMapping;

	UPROPERTY(EditAnywhere, Category = "Input")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, Category = "Input")
	bool bRegisterWithSettings = true;
};

UCLASS(meta = (DisplayName = "Add Input Mapping"))
class EXPERIENCEFRAMEWORKRUNTIME_API UExperienceGameFeatureAction_AddInputContextMapping final : public UExperienceGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureRegistering() override;
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	virtual void OnGameFeatureUnregistering() override;

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, Category = "Input")
	TArray<FExperienceInputMappingContextAndPriority> InputMappings;

private:
	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>> ExtensionRequestHandles;
		TArray<TWeakObjectPtr<APlayerController>> ControllersAddedTo;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;
	FDelegateHandle RegisterInputContextMappingsForGameInstanceHandle;

	void RegisterInputMappingContexts();
	void RegisterInputContextMappingsForGameInstance(UGameInstance* GameInstance);
	void RegisterInputMappingContextsForLocalPlayer(ULocalPlayer* LocalPlayer);
	void UnregisterInputMappingContexts();
	void UnregisterInputContextMappingsForGameInstance(UGameInstance* GameInstance);
	void UnregisterInputMappingContextsForLocalPlayer(ULocalPlayer* LocalPlayer);

	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;

	void Reset(FPerContextData& ActiveData);
	void HandleControllerExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext);
	void AddInputMappingForController(APlayerController* PlayerController, FPerContextData& ActiveData);
	void RemoveInputMapping(APlayerController* PlayerController, FPerContextData& ActiveData);
};
