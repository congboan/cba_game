#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SettingEntry.h"
#include "GameFramework/GameUserSettings.h"
#include "SettingCollection.generated.h"

UCLASS(BlueprintType)
class SETTINGSFRAMEWORKRUNTIME_API USettingCollection : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText CollectionName;

	/** 设置宿主类：限定 UGameUserSettings 子类（对齐 Lyra ULyraSettingsLocal），须提供静态 Get()/GetGameUserSettings()。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGameUserSettings> HostClass;

	/** 内联设置条目：可在 Collection 资产面板内直接添加/编辑，无需单独创建资产。 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TArray<USettingEntry*> Entries;

	/** 页面标识（可选），用于导航定位。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName DevName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag AssetBundleTag;
};
