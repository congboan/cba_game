#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AbilityData/Fragment/FAssemblyFragmentBase.h"
#include "FAssemblyFragment_Cooldown.generated.h"

class UGameplayEffect;
class UAssemblyAbilitySource;
struct FGameplayEffectSpec;

/**
 * 冷却片段 — 把引擎原生冷却降级为成本管线的一员。
 *
 * 继承 FAssemblyFragmentCostBase：Check（在 CD 判定）/ Apply（启动 CD）经
 * 引擎 CheckCost/ApplyCost 管线结算（DataDriven 的 Cost/Cooldown 引擎通道恒返回空，
 * 片段即唯一结算层）。DurationOverrides 命中时由 Apply 的 SetByCaller 注入改写时长。
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyFragment_Cooldown : public FAssemblyFragmentCostBase
{
	GENERATED_BODY()

public:
	/** 冷却 GE 资产。Duration 建议为 SetByCaller（配合 DurationOverrides）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooldown")
	TSoftObjectPtr<UGameplayEffect> CooldownGE;

	/** 冷却占位标签集合（激活期间由 GE 写入 ASC；同时作为本片“是否在CD”的判定依据）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooldown")
	FGameplayTagContainer CooldownTags;

	/** 条件时长覆写：ASC 持有 Key 时以 Value 秒注入 SetByCaller(Key)。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cooldown")
	TArray<FAssemblyNamedValue> DurationOverrides;

	// ── 授予期预载缓存 ─────────────────────────────

	/** 授予期已加载的冷却 GE。运行时只读缓存。 */
	UPROPERTY(Transient)
	TObjectPtr<UGameplayEffect> CachedGE = nullptr;

	virtual void Precache(UAssemblyAbilitySource* Owner) override;

	/**
	 * 门卫：ASC 当前持有任一 CooldownTags 即视为在 CD，阻止激活并输出原因标签。
	 * （引擎原生 CheckCooldown 因 getter 空返回已退役，判定职责移入本片。）
	 */
	virtual bool Check(
		const UGA_AssemblyDataDriven& GA,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags) const override;

	/** 结算：构建 CachedGE 的 Spec（含条件 SetByCaller 时长注入）后应用到 Owner ASC。 */
	virtual void Apply(
		const UGA_AssemblyDataDriven& GA,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo& ActivationInfo) const override;
};
