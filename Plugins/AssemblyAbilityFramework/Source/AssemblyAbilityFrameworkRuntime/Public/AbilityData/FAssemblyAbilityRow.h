#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "StructUtils/InstancedStruct.h"
#include "AbilityData/Execution/FAssemblyExecutionBase.h"
#include "AbilityData/Fragment/FAssemblyFragmentBase.h"
#include "AbilityData/Fragment/FAssemblyFragment_Cooldown.h"
#if WITH_EDITOR
	#include "Misc/DataValidation.h"
#endif
#include "FAssemblyAbilityRow.generated.h"

class UGameplayAbility;
class UGameplayEffect;

/**
 * 具名执行覆写条目。
 * 数组声明顺序即优先级：激活时按顺序匹配持有者标签，首个命中者获胜
 * （first-match-wins，Mover Transition 同款语义）。禁止使用无序容器承载多候选。
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FNamedExecutionOverride
{
	GENERATED_BODY()

	/** 触发覆写的持有者标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Execute")
	FGameplayTag HolderTag;

	/** 命中时替代默认 ExecutionStrategy 的策略实例。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Execute")
	TInstancedStruct<FAssemblyExecutionBase> Strategy;
};

/**
 * 单条技能定义行 — 平铺所有生命周期数据 + 多态执行策略。
 *
 * 一行 = 一个技能。编辑器中直接编辑平铺字段，
 * 执行策略通过 TInstancedStruct<FAssemblyExecutionBase> 选择子类。
 *
 * 无 Fragment 层、无 Compile 步骤：Row 即配置即运行时数据。
 * UAssemblyAbilitySource 持有 Row 副本挂到 Spec.SourceObject。
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyAbilityRow : public FTableRowBase
{
	GENERATED_BODY()

	// ── 身份 ──────────────────────────────────────────

	/** 主键 — 同时作为 DataRegistry 查找标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	FGameplayTag AbilityTag;

	/** 覆写默认 GA 类（默认 = UGA_AssemblyDataDriven）。为空时使用默认 GA。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> AbilityClass;

	// ── 结算法片段（单数组聚合：成本/冷却/未来资源类）──

	/**
	 * 统一结算法片段。每片 Check（激活门）+ Apply（Commit 结算）；
	 * 冷却以 FAssemblyFragment_Cooldown 参与同一管线（引擎原生通道已被
	 * DataDriven 的空返回收敛，无双结算）。JSON 目标下的唯一多态面之一。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Fragments")
	TArray<TInstancedStruct<FAssemblyFragmentBase>> Fragments;

	// ── 数值 ──────────────────────────────────────────

	/** SetByCaller Tag → float 数值映射，执行策略施加 GE 时应用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Values")
	TMap<FGameplayTag, float> Values;

	// ── 执行策略（多态，编辑器选子类）──────────────

	/** 默认执行策略。编辑器中选择 FAssemblyExecution_Xxx 子类。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Execute")
	TInstancedStruct<FAssemblyExecutionBase> ExecutionStrategy;

	/** 执行覆写列表。按声明顺序做 first-match-wins 匹配（见 FNamedExecutionOverride）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Execute")
	TArray<FNamedExecutionOverride> ExecutionOverrides;

#if WITH_EDITOR
	/** DataTable 编辑器自动校验入口（UDataTable::IsDataValid 逐行调用）。逻辑委托给 UAssemblyAbilityRowValidator。 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
