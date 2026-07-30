#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Internationalization/Text.h"
#include "AbilityData/FAssemblyChainContext.h"
#include "UGA_AssemblyBase.generated.h"

/**
 * 激活策略 — 决定 GA 何时被激活。
 *
 * 参考 Lyra ELyraAbilityActivationPolicy，剔除项目特定项后保留通用 4 档：
 *   - OnInputTriggered：输入按下时触发（最常见的主动技能）
 *   - WhileInputActive：输入持续按下时反复尝试激活（连发武器、持续吟唱）
 *   - OnSpawn：技能 Give 到 ASC 后立即激活（被动 buff 注入）
 *   - OnAvatarSet：Pawn Avatar 就绪后激活（被动需访问 Avatar 组件时）
 */
UENUM(BlueprintType)
enum class EAssemblyAbilityActivationPolicy : uint8
{
	OnInputTriggered,
	WhileInputActive,
	OnSpawn,
	OnAvatarSet,
};

/**
 * AssemblyAbilityFramework 主框架 GA 基类 — 编辑器优先（editor-first）。
 *
 * 面向"在编辑器中手工制作技能"的刚需：蓝图/C++ 直接继承本类，在其中编写
 * 激活逻辑，无需任何数据行/来源包装对象/自定义 EffectContext。
 *
 * 提供 Lyra 风格的通用关注点：
 *   - 激活策略（ActivationPolicy）：OnSpawn / OnAvatarSet 由本类在 Avatar 就绪时自动激活
 *   - 失败原因 → 玩家提示文本映射（FailureTagToUserFacingMessages）
 *   - bLogCancelation 调试开关
 *
 * 技能身份识别：直接使用引擎原生 GA AssetTags（UGameplayAbility::GetAssetTags）；
 * 引擎默认的 ApplyAbilityTagsToGameplayEffectSpec 已将 AssetTags 注入出向 GE Spec，
 * 下游据此识别 GE 来自哪个技能。本框架不再维护独立的身份标签字段。
 *
 * 链式触发上下文（FAssemblyChainContext）的收发接口在 Phase 3 追加，供手写 GA 使用。
 *
 * 边界：本类**不依赖**任何 Experimental/ 下的数据驱动类型
 * （FAssemblyEffectContext / UAssemblyAbilitySource / FAssemblyAbilityRow）。
 * 数据驱动动态注入相关的 MakeEffectContext / AssemblySource 读取等，
 * 全部下沉到 Experimental/ 的 UGA_AssemblyDataDriven。
 */
UCLASS(Abstract)
class ASSEMBLYABILITYFRAMEWORKRUNTIME_API UGA_AssemblyBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AssemblyBase();

	// ── 配置字段（编辑器/蓝图）────────────────────────

	/** 激活策略。决定 GA 何时被激活。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AssemblyAbility|Activation")
	EAssemblyAbilityActivationPolicy ActivationPolicy = EAssemblyAbilityActivationPolicy::OnInputTriggered;

	/** 失败原因 tag → 给玩家的提示文本。供 UI 层订阅。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AssemblyAbility|Failure")
	TMap<FGameplayTag, FText> FailureTagToUserFacingMessages;

	/** 取消时输出诊断日志，方便排查"为什么这个技能突然被打断"。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AssemblyAbility|Debug")
	bool bLogCancelation = false;

	/** 链事件最大深度。防止触发链无限递归。 */
	static constexpr int32 MaxChainDepth = 32;

	// ── 链式触发上下文收发（editor-first 手写 GA 使用）────

	/**
	 * 发送链式 GameplayEvent — 从 TriggerEventData 继承并推进链上下文（FAssemblyChainContext）。
	 *
	 * 读取父事件的链上下文（若有），深度 +1，回填 RootEventId / Instigator，
	 * 再通过 ASC->HandleGameplayEvent 广播 EventTag。超过 MaxChainDepth 时中止并告警。
	 *
	 * C++ 全保真版：TriggerEventData 可为 nullptr（新链根）。执行策略等内部调用走此重载。
	 */
	void SendChainEvent(
		const FGameplayTag& EventTag,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData);

	/**
	 * 蓝图版发送链事件 — 供编辑器手写 GA 蓝图调用。
	 * 使用当前 ActorInfo；ParentContext 传入上一跳的链上下文（无则留默认）。
	 */
	UFUNCTION(BlueprintCallable, Category = "AssemblyAbility|Chain")
	void K2_SendChainEvent(FGameplayTag EventTag, const FAssemblyChainContext& ParentContext);

	/**
	 * 蓝图版读取链上下文 — 从 TriggerEventData 的 InstancedEventData 取出 FAssemblyChainContext。
	 * 无有效上下文时返回默认值（ChainDepth=0）。bFound 指示是否命中。
	 */
	UFUNCTION(BlueprintCallable, Category = "AssemblyAbility|Chain")
	FAssemblyChainContext GetChainContext(const FGameplayEventData& TriggerEventData, bool& bFound) const;

protected:
	// ── UGameplayAbility 覆写 ─────────────────────────

	/**
	 * Pawn Avatar 就绪钩子 — OnAvatarSet 激活策略 + 被动技能初始化点。
	 * 默认按 ActivationPolicy 在合适时机激活，子类可覆写扩展。
	 */
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	/**
	 * 失败回调 — 走 FailureTagToUserFacingMessages 输出可读日志。
	 *
	 * 调用约定：调用方在 `CanActivateAbility` / `CommitCheck` 等失败路径上把
	 * 失败原因 tag 装进 OptionalRelevantTags / FailedReason 后主动调用本函数。
	 * 引擎不会自动调用——保留为 helper 让子类灵活组合。
	 *
	 * 子类可覆写以推送到 UI 层（如订阅 OnAbilityFailedDelegate）。
	 */
	virtual void NativeOnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const;

	/** CancelAbility 走 bLogCancelation 输出诊断。 */
	virtual void CancelAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateCancelAbility) override;
};
