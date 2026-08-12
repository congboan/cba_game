#pragma once

#include "CoreMinimal.h"
#include "Data/SettingCollection.h"
#include "SettingRegistry.generated.h"

class USettingViewModelBase;
class USettingPageViewModel;

UCLASS(BlueprintType)
class SETTINGSFRAMEWORKRUNTIME_API USettingRegistry : public UObject
{
	GENERATED_BODY()

public:
	/** 加载 Collection：从 Collection->HostClass 静态 Get() 解析宿主实例（InHost 为可选回退）。 */
	UFUNCTION(BlueprintCallable)
	void LoadCollection(USettingCollection* Collection, UObject* InHost = nullptr);

	/** 按类解析宿主：校验静态 Get()/GetGameUserSettings() 契约后调用，失败回退 InHost。 */
	UFUNCTION(BlueprintCallable)
	UObject* ResolveHost(TSubclassOf<UGameUserSettings> InHostClass, UObject* InHost) const;

	UFUNCTION(BlueprintCallable)
	void RegisterViewModelClass(ESettingValueType ValueType, TSubclassOf<USettingViewModelBase> ViewModelClass);

	UFUNCTION(BlueprintCallable)
	virtual USettingViewModelBase* CreateViewModelForEntry(USettingEntry* Entry, UObject* InHost);

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
	const TArray<USettingViewModelBase*>& GetAllViewModels() const { return AllViewModels; }

	UFUNCTION(BlueprintPure)
	TArray<USettingViewModelBase*> SearchSettings(const FString& Query) const;

	UFUNCTION(BlueprintPure)
	USettingViewModelBase* GetCurrentPage() const { return CurrentPage; }

	UFUNCTION(BlueprintCallable)
	void RefreshAllTraits(FGameplayTagContainer Traits);

protected:
	TSubclassOf<USettingViewModelBase> GetViewModelClassForType(ESettingValueType ValueType) const;

	/** 递归加载 Collection；ParentPage 非空时 VM 挂到该页的 ChildViewModels。 */
	void LoadCollectionInto(USettingCollection* Collection, UObject* InHost,
		USettingPageViewModel* ParentPage);

	/** 递归加载单个 Entry：生成 VM 挂到 ParentVM；Page/Group 类型继续加载其 Children。 */
	void LoadEntryInto(USettingPageViewModel* ParentVM, USettingEntry* Entry, UObject* InHost);

	/** 全部 VM 建立后，按 DevName 解析依赖关系并建立订阅。 */
	void ResolveDependencies();

	struct FSettingDependencyRequest
	{
		USettingViewModelBase* VM = nullptr;
		TArray<FName> DevNames;
	};

	UPROPERTY(Transient)
	TMap<ESettingValueType, TSubclassOf<USettingViewModelBase>> RegisteredViewModelClasses;

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

	/** 待解析的依赖请求（加载完成后再建订阅）。 */
	TArray<FSettingDependencyRequest> PendingDependencies;
};
