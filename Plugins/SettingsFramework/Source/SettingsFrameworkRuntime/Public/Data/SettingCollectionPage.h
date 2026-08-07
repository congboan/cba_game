#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SettingCollection.h"
#include "SettingCollectionPage.generated.h"

/**
 * 子分类页数据资产：可单独在内容浏览器创建，代表一个可导航进入的子设置分类。
 * 对应原版 GameSettings 的 UGameSettingCollectionPage。
 * 通过父级 USettingCollection.ChildPages 挂接，支持任意层级嵌套。
 */
UCLASS(BlueprintType)
class SETTINGSFRAMEWORKRUNTIME_API USettingCollectionPage : public USettingCollection
{
	GENERATED_BODY()

public:
	/** 页面 DevName，用于导航定位（可选）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName PageDevName;
};
