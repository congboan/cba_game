#pragma once

#include "ViewModels/SettingViewModelBase.h"
#include "SettingValueDiscrete.generated.h"

/** 离散值设置 VM 基类（对应 Lyra UGameSettingValueDiscrete）。
 *  具体离散设置（分辨率/语言/质量档等）继承此类并实现选项枚举逻辑。 */
UCLASS(BlueprintType, Blueprintable, Abstract)
class SETTINGSFRAMEWORKRUNTIME_API USettingValueDiscrete : public USettingViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	virtual void SetDiscreteOptionByIndex(int32 Index);

	UFUNCTION(BlueprintPure)
	virtual int32 GetDiscreteOptionIndex() const;

	UFUNCTION(BlueprintPure)
	virtual TArray<FText> GetDiscreteOptions() const;

	UFUNCTION(BlueprintPure)
	virtual FString GetDiscreteOptionValueAt(int32 Index) const;

protected:
	virtual void OnSelectedOptionChanged(int32 NewIndex);
};
