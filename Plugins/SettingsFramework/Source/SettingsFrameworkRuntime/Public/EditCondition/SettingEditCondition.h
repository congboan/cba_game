#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct FSettingEditableState
{
	bool bVisible = true;
	bool bEnabled = true;
	bool bResetable = true;

	void Hide() { bVisible = false; }
	void Disable() { bEnabled = false; }
	void UnableToReset() { bResetable = false; }
};

/** 可组合编辑条件（对应 Lyra FGameSettingEditCondition）。
 *  项目侧可派生实现平台/标签/依赖等动态条件。 */
class FSettingEditCondition
{
public:
	virtual ~FSettingEditCondition() = default;

	virtual void GatherEditState(FSettingEditableState& InOutState) const
	{
	}
};

/** 平台标签条件：Traits 中缺少要求标签时隐藏。 */
class FWhenPlatformHasTrait : public FSettingEditCondition
{
public:
	explicit FWhenPlatformHasTrait(const FGameplayTagContainer& InRequiredTraits)
		: RequiredTraits(InRequiredTraits)
	{
	}

	virtual void GatherEditState(FSettingEditableState& InOutState) const override
	{
		if (!RequiredTraits.IsEmpty() && !CurrentTraits.HasAny(RequiredTraits))
		{
			InOutState.Hide();
		}
	}

	FGameplayTagContainer CurrentTraits;

private:
	FGameplayTagContainer RequiredTraits;
};
