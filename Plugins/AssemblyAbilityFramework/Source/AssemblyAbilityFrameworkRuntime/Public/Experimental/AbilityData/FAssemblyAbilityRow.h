#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Experimental/AbilityData/FAssemblyExecutionBase.h"
#include "FAssemblyAbilityRow.generated.h"

class UGameplayAbility;
class UGameplayEffect;

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

	// ── 生命周期数据（GA 直接读取）──────────────────

	/** 技能冷却 GE。为空则无冷却。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Cooldown")
	TSoftObjectPtr<UGameplayEffect> CooldownGE;

	/** 技能消耗 GE。为空则无消耗。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Cost")
	TSoftObjectPtr<UGameplayEffect> CostGE;

	/** ASC 拥有任一标签时阻止激活。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Activation")
	FGameplayTagContainer ActivationBlockedTags;

	/** 激活时取消拥有这些标签的技能。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Activation")
	FGameplayTagContainer CancelAbilitiesWithTags;

	// ── 输入 ──────────────────────────────────────────

	/** 输入标签。非空时 GiveAbility 设置 Spec.InputID = Hash(InputTag)。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Input")
	FGameplayTag InputTag;

	// ── 数值 ──────────────────────────────────────────

	/** SetByCaller Tag → float 数值映射，执行策略施加 GE 时应用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Values")
	TMap<FGameplayTag, float> Values;

	// ── 冷却覆写 ──────────────────────────────────────

	/** 触发冷却覆写的标签集合。ASC 拥有其中任一标签时应用对应覆写。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|CooldownOverride")
	FGameplayTagContainer CooldownOverrideTags;

	/** 每个覆写标签的冷却时间（秒）。通过 SetByCaller 注入 CooldownGE Duration。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|CooldownOverride")
	TMap<FGameplayTag, float> PerTagCooldown;

	// ── 执行策略（多态，编辑器选子类）──────────────

	/** 默认执行策略。编辑器中选择 FAssemblyExecution_Xxx 子类。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Execute")
	TInstancedStruct<FAssemblyExecutionBase> ExecutionStrategy;

	/** 持有者标签 → 替代执行策略。激活时按 ASC 标签匹配覆写。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Execute")
	TMap<FGameplayTag, TInstancedStruct<FAssemblyExecutionBase>> ExecutionOverrides;
};
