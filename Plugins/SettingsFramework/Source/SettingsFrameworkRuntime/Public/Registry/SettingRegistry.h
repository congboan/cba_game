#pragma once

#include "CoreMinimal.h"
#include "Data/SettingCollection.h"
#include "SettingRegistry.generated.h"

class USettingViewModelBase;

UCLASS(BlueprintType)
class SETTINGSFRAMEWORKRUNTIME_API USettingRegistry : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void LoadCollection(USettingCollection* Collection, UObject* InHost);

	UFUNCTION(BlueprintCallable)
	void SaveChanges();

	UFUNCTION(BlueprintCallable)
	void NavigateToPage(USettingViewModelBase* PageVM);

	UFUNCTION(BlueprintCallable)
	bool NavigateBack();

	UFUNCTION(BlueprintPure)
	USettingViewModelBase* FindSettingByDevName(FName DevName) const;

	UFUNCTION(BlueprintPure)
	TArray<USettingViewModelBase*> GetRootViewModels() const { return RootViewModels; }

	UFUNCTION(BlueprintPure)
	TArray<USettingViewModelBase*> SearchSettings(const FString& Query) const;

	UFUNCTION(BlueprintPure)
	USettingViewModelBase* GetCurrentPage() const { return CurrentPage; }

	UFUNCTION(BlueprintCallable)
	void RefreshAllTraits(FGameplayTagContainer Traits);

protected:
	UPROPERTY()
	TArray<USettingCollection*> LoadedCollections;

	UPROPERTY()
	TArray<USettingViewModelBase*> RootViewModels;

	UPROPERTY()
	TArray<USettingViewModelBase*> AllViewModels;

	UPROPERTY()
	UObject* HostObject;

	UPROPERTY()
	USettingViewModelBase* CurrentPage;

	TArray<USettingViewModelBase*> NavStack;
};
