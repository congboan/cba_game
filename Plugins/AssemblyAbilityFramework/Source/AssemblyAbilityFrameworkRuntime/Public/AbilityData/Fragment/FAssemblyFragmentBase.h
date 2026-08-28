#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "FAssemblyFragmentBase.generated.h"

class UAbilitySystemComponent;
class UAssemblyAbilitySource;
class UGA_AssemblyDataDriven;
struct FGameplayAbilitySpec;
struct FGameplayAbilityActorInfo;
struct FGameplayAbilityActivationInfo;

/**
 * 具名键值条目 — 替代“平行双容器”表达（Tag→float 单概念单容器）。
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyNamedValue
{
	GENERATED_BODY()

	/** 键标签（如覆写条件持有的标签）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragment")
	FGameplayTag Key;

	/** 数值（含义由所在片段定义，如冷却秒数）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragment")
	float Value = 0.0f;
};

/**
 * 片段基类 — Row.Fragments 单数组的窄接口。仅承载授予期工具，零行为虚函数。
 *
 * 行为全部下沉到窄基类（片按需继承，ForEachHook 按类型查表分派）：
 *   - 结算族：FAssemblyFragmentCostBase（Check 门卫 + Apply 结算，成本语义）
 *   - 生命周期族：FAssemblyFragmentOn*Base（8 个，各流程点一个）
 *   - 授予期贡献：ContributeToSpec（Trigger 片等）
 *
 * ┌─ 铁律：基类禁止长出任何行为虚函数。新行为 = 新窄基类 + GA 一行转发 ─┐
 * │ （历史教训：旧 Fragment 层死于“镜像全部 GA 虚函数”的多监听神接口）。 │
 * └──────────────────────────────────────────────────────────────┘
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyFragmentBase
{
	GENERATED_BODY()

	virtual ~FAssemblyFragmentBase() = default;

	/**
	 * 授予期预载钩子：GiveAbilityFromRow 对拷贝进 Source 的片段实例调用一次，
	 * 子类加载自身高频软资产写入 Transient 缓存，运行时只读缓存。默认无操作。
	 */
	virtual void Precache(UAssemblyAbilitySource* Owner) {}

	/**
	 * 授予期贡献钩子：GiveAbilityFromRow 在构造 Spec 后、GiveAbility 前对每个片段调用一次。
	 * 子类把自身配置贡献进 Spec（如 Trigger 片写入 Spec.DynamicAbilityTriggers，由引擎
	 * 授予时原生注册）。默认无操作 —— 管线不引用任何具体片类型。
	 */
	virtual void ContributeToSpec(FGameplayAbilitySpec& Spec) {}
};

/**
 * 成本窄基类 — 成本/资源结算（Check 门卫 + Apply 扣费）。
 *
 * 体现成本语义：Check/Apply 即引擎 CheckCost/ApplyCost 管线（激活门 + Commit 扣费）。
 * 准入判据：两段式、可多实例叠加、与 GA 生命周期零耦合。
 * 冷却以降级为成本管线的一员（引擎原生 Cost/Cooldown 通道在 DataDriven 恒返回空，
 * 片段即唯一结算层 —— 见 FAssemblyFragment_Cooldown）。
 * 成本片继承本类即进 CostBase 桶（ForEachHook 查表分派）。
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyFragmentCostBase : public FAssemblyFragmentBase
{
	GENERATED_BODY()

	/** 门卫：是否可支付/可执行。失败时写入可读原因 tag 到 OptionalRelevantTags。默认通过。 */
	virtual bool Check(
		const UGA_AssemblyDataDriven& GA,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags) const
	{
		return true;
	}

	/** 结算：Commit 阶段调用一次。默认无操作。 */
	virtual void Apply(
		const UGA_AssemblyDataDriven& GA,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo& ActivationInfo) const
	{
	}
};

// ── GA 生命周期钩子窄基类 ─────────────────────────────
// 每个流程点一个独立窄基类（ISP）：片按需继承其一（可多继承多个钩子基类），
// GA 在对应流程点用 ForEachHook<HookT> 按脚本结构 IsChildOf 分派（只遍历命中组）。
// 新流程点 = 新建窄基类 + GA 加一行转发；基类 FAssemblyFragmentBase 永不改动。

/** 授予完成（OnGiveAbility 之后）。 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyFragmentOnGiveAbilityBase : public FAssemblyFragmentBase
{
	GENERATED_BODY()

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilitySpec& Spec) const {}
};

/** 移除前（OnRemoveAbility 之前）。 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyFragmentOnRemoveAbilityBase : public FAssemblyFragmentBase
{
	GENERATED_BODY()

	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilitySpec& Spec) const {}
};

/** Avatar 就位（OnAvatarSet 之后）。 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyFragmentOnAvatarSetBase : public FAssemblyFragmentBase
{
	GENERATED_BODY()

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilitySpec& Spec) const {}
};

/** 激活门卫（CanActivateAbility 内）：任一返回 false 即拒绝。 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyFragmentOnCanActivateBase : public FAssemblyFragmentBase
{
	GENERATED_BODY()

	/**
	 * 激活门卫：返回 false 拒绝激活。失败时写入可读原因 tag 到 OptionalRelevantTags。
	 * SourceTags/TargetTags 为引擎 CanActivateAbility 透传参数（Trigger 事件激活时来自
	 * TriggerEventData.InstigatorTags/TargetTags，非事件激活为 nullptr —— 空指针 = 该侧不判定）。
	 */
	virtual bool OnCanActivate(
		const UGA_AssemblyDataDriven& GA,
		const FGameplayAbilityActorInfo& ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const { return true; }
};

/** 激活前（PreActivate 之后，执行前）。 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyFragmentOnPreActivateBase : public FAssemblyFragmentBase
{
	GENERATED_BODY()

	virtual void OnPreActivate(const UGA_AssemblyDataDriven& GA, const FGameplayAbilityActorInfo& ActorInfo) const {}
};

/** 激活成立（Commit 成功）。 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyFragmentOnActivatedBase : public FAssemblyFragmentBase
{
	GENERATED_BODY()

	virtual void OnActivated(const UGA_AssemblyDataDriven& GA, const FGameplayAbilityActorInfo& ActorInfo) const {}
};

/** 结束（EndAbility 之后，含取消）。 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyFragmentOnEndAbilityBase : public FAssemblyFragmentBase
{
	GENERATED_BODY()

	virtual void OnEndAbility(const UGA_AssemblyDataDriven& GA, bool bWasCancelled) const {}
};

/** 激活失败（CanActivateAbility 拒绝 / Commit 失败）。 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyFragmentOnFailedBase : public FAssemblyFragmentBase
{
	GENERATED_BODY()

	virtual void OnFailed(const UGA_AssemblyDataDriven& GA, const FGameplayTagContainer& FailTags) const {}
};
