#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/UGA_AssemblyBase.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "UGA_AssemblyDataDriven.generated.h"

class UGameplayEffect;
class UAssemblyAbilitySource;
struct FAssemblyExecutionBase;
struct FAssemblyAbilityRow;
struct FAssemblyEffectContext;
struct FGameplayEffectSpec;
struct FGameplayEffectContextHandle;

/**
 * 通用数据驱动 GameplayAbility — 从 Spec.SourceObject (UAssemblyAbilitySource) 读取 FAssemblyAbilityRow。
 *
 * 继承 UGA_AssemblyBase 复用：
 *   - ActivationPolicy / OnAvatarSet 钩子
 *   - FailureTagToUserFacingMessages 失败提示
 *   - bLogCancelation 取消日志
 *
 * 数据驱动专属覆写 + 下沉自基类的方法：
 *   - MakeEffectContext (含 FAssemblyEffectContext / AssemblySource / OriginAbilityTag 注入)
 *   - GetAssemblySource / GetAssemblyContextFrom* 静态助手
 *   - GetOriginAbilityTag（从 Row.AbilityTag 获取）
 *   - CanActivateAbility()         → Super + Tags 片 7 项 tag 判定（引擎 DoesAbilitySatisfyTagRequirements 同构）
 *   - CheckCost / ApplyCost        → Fragments 统一结算法管线（引擎原生 Cost/Cooldown 通道退役）
 *   - 生命周期转发                  → OnGive/OnRemove/OnAvatarSet/PreActivate/OnActivated/OnEndAbility/OnFailed
 *
 * ActivateAbility() 执行流程：
 *   1. CommitAbility（消耗 + 冷却）
 *   2. CancelAbilitiesWithTags（取消冲突技能）
 *   3. ResolveExecutionData（含 Overrides）→ Execute()
 *   4. EndAbility
 */
UCLASS()
class ASSEMBLYABILITYFRAMEWORKRUNTIME_API UGA_AssemblyDataDriven : public UGA_AssemblyBase
{
	GENERATED_BODY()

public:
	UGA_AssemblyDataDriven();

	// ── 自 UGA_AssemblyBase 下沉（装配层专属）──────────

	/**
	 * 返回自定义 FAssemblyEffectContext，注入 AssemblySource + OriginAbilityTag。
	 */
	virtual FGameplayEffectContextHandle MakeEffectContext(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const override;

	/**
	 * 从 Spec.SourceObject 读取 AssemblySource 引用。
	 * nullptr 表示非数据驱动路径（不应在装配层路径出现）。
	 */
	UAssemblyAbilitySource* GetAssemblySource() const;

	/** 从 GE Spec 取 FAssemblyEffectContext（非该类型返回 nullptr）。 */
	static const FAssemblyEffectContext* GetAssemblyContextFromSpec(const struct FGameplayEffectSpec& Spec);

	/** 从 Handle 取 FAssemblyEffectContext（非该类型返回 nullptr）。 */
	static const FAssemblyEffectContext* GetAssemblyContextFromHandle(const FGameplayEffectContextHandle& Handle);

	/**
	 * 子类技能出处 tag：写入 MakeEffectContext 的 OriginAbilityTag。
	 * 默认从 AssemblySource->CompiledRow.Row.AbilityTag 获取（没有时返回空 tag）。
	 */
	virtual FGameplayTag GetOriginAbilityTag() const;

	// ── UGameplayAbility 覆写 ─────────────────────────
	// （引擎原生 Cost/Cooldown 通道在此退役：GetCooldown/CostGameplayEffect
	//   走基类默认（返回空），结算全部由 Row.Fragments 承担，见 CheckCost/ApplyCost。）

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// ── 生命周期钩子转发（片按窄基类 IsChildOf 分派，见 ForEachHook）──

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData = nullptr) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// ── 数据访问 ──────────────────────────────────────

	/** 从 Spec.SourceObject 读取 Row。热路径使用。 */
	const FAssemblyAbilityRow* GetRow() const;

	/** 行级身份标签聚合（Tags 片 CollectAssetTags 逐片聚合；无片返回空）。 */
	FGameplayTagContainer GetIdentityTags() const;

	/** 行级激活期自挂标签（聚合所有 Tags 片 ActivationOwnedTags；无片返回空容器）。 */
	FGameplayTagContainer GetActivationOwnedTags() const;

	/** 获取来源对象（武器 EquipmentInstance 等）。 */
	UObject* GetOriginSource() const;

	/** 解析有效的执行策略（考虑 Overrides）。 */
	FAssemblyExecutionBase* ResolveExecutionData();
};
