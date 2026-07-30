#pragma once

#include "CommonActivatableWidget.h"
#include "GameFeatures/ExperienceGameFeatureAction_WorldActionBase.h"
#include "UIExtensionSystem.h"
#include "UObject/ObjectKey.h"
#include "ExperienceGameFeatureAction_AddWidgets.generated.h"

struct FComponentRequestHandle;
class UUserWidget;

USTRUCT()
struct FExperienceHUDLayoutRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "UI", meta = (AssetBundles = "Client"))
	TSoftClassPtr<UCommonActivatableWidget> LayoutClass;

	UPROPERTY(EditAnywhere, Category = "UI", meta = (Categories = "UI.Layer"))
	FGameplayTag LayerID;
};

USTRUCT()
struct FExperienceHUDElementEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "UI", meta = (AssetBundles = "Client"))
	TSoftClassPtr<UUserWidget> WidgetClass;

	UPROPERTY(EditAnywhere, Category = "UI")
	FGameplayTag SlotID;
};

UCLASS(meta = (DisplayName = "Add Widgets"))
class EXPERIENCEFRAMEWORKRUNTIME_API UExperienceGameFeatureAction_AddWidgets final : public UExperienceGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditAnywhere, Category = "UI", meta = (TitleProperty = "{LayerID} -> {LayoutClass}"))
	TArray<FExperienceHUDLayoutRequest> Layout;

	UPROPERTY(EditAnywhere, Category = "UI", meta = (TitleProperty = "{SlotID} -> {WidgetClass}"))
	TArray<FExperienceHUDElementEntry> Widgets;

private:
	struct FPerActorData
	{
		TArray<TWeakObjectPtr<UCommonActivatableWidget>> LayoutsAdded;
		TArray<FUIExtensionHandle> ExtensionHandles;
	};

	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequests;
		TMap<FObjectKey, FPerActorData> ActorData;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;

	void Reset(FPerContextData& ActiveData);
	void HandleActorExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext);
	void AddWidgets(AActor* Actor, FPerContextData& ActiveData);
	void RemoveWidgets(AActor* Actor, FPerContextData& ActiveData);
};
