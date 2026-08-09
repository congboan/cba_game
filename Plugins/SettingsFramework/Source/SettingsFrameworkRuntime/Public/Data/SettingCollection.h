#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SettingEntry.h"
#include "SettingCollection.generated.h"

UCLASS(BlueprintType)
class SETTINGSFRAMEWORKRUNTIME_API USettingCollection : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText CollectionName;

	/** 设置宿主类：须提供静态 Get() 返回宿主实例（Lyra 式，如 UCbaSettingsLocal::Get()）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UObject> HostClass;

	/** 内联设置条目：可在 Collection 资产面板内直接添加/编辑，无需单独创建资产。 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TArray<USettingEntry*> Entries;

	/** 子分类页（可嵌套）：每个 Page 资产代表一个可导航进入的子设置分类。 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TArray<USettingCollection*> ChildPages;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag AssetBundleTag;
};
