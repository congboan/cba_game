#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Experimental/AbilityData/Context/FAssemblyContextExtension.h"
#include "FAssemblyEffectContext.generated.h"

class UAssemblyAbilitySource;
struct FAssemblyAbilityTrigger;

/**
 * AssemblyAbilityFramework 自定义 EffectContext。
 *
 * 在 FGameplayEffectContext 基础上携带：
 *   - AssemblySource：技能 SourceObject 包装（持 Row 副本 + OriginSource）
 *   - OriginAbilityTag：出处技能 tag，便于下游识别（无 Source 时也可用）
 *   - Extension：按需挂载的扩展数据（TInstancedStruct 多态）
 *
 * 设计原则：
 *   - 单纯伤害 GE 不需要 Extension → Extension.IsValid() == false，零字段开销
 *   - 通过 UAbilitySystemGlobals::AllocGameplayEffectContext 在**项目侧**注册为默认 Context
 *     （AAF 插件不强制；项目可选注册）
 *   - NetSerialize / Duplicate 走 Super + Extension 序列化两段式
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

	FAssemblyEffectContext() = default;
	virtual ~FAssemblyEffectContext() = default;

	// ── 自有数据 ──────────────────────────────────────

	/** 技能 SourceObject 包装。可为 nullptr（非数据驱动 GA / 蓝图直配 GA）。 */
	UPROPERTY()
	TObjectPtr<UAssemblyAbilitySource> AssemblySource = nullptr;

	/** 出处技能 tag。即使无 AssemblySource 也可用于下游识别。 */
	UPROPERTY()
	FGameplayTag OriginAbilityTag;

	/** 按需挂载的扩展数据（多态）。不挂时零开销。 */
	UPROPERTY()
	TInstancedStruct<FAssemblyContextExtension> Extension;

	// ── 工厂便捷函数 ──────────────────────────────────

	/**
	 * 构造一个仅含 AssemblySource + OriginAbilityTag 的 Context，不挂 Extension。
	 * 适用于单纯伤害 / 治疗等无附带数据的 GE。
	 */
	static FGameplayEffectContextHandle MakeForAssembly(
		UAssemblyAbilitySource* InAssemblySource,
		FGameplayTag InOriginAbilityTag,
		AActor* Instigator,
		AActor* EffectCauser);

	/**
	 * 构造一个含 Trigger 扩展的 Context，供 UGETriggerComponent_Assembly 读取注册。
	 * Triggers 为空时退化为 MakeForAssembly。
	 */
	static FGameplayEffectContextHandle MakeWithTriggers(
		UAssemblyAbilitySource* InAssemblySource,
		FGameplayTag InOriginAbilityTag,
		const TArray<FAssemblyAbilityTrigger>& Triggers,
		AActor* Instigator,
		AActor* EffectCauser);

	// ── 读取助手 ──────────────────────────────────────

	/**
	 * 安全取出 Trigger 列表。Extension 不是 Triggers 型时返回 nullptr。
	 * 注意：返回指针指向 Extension 内部数据，调用方不得跨 Context 生命周期持有。
	 */
	const TArray<FAssemblyAbilityTrigger>* GetTriggers() const;

	// ── FGameplayEffectContext 覆写 ───────────────────

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FAssemblyEffectContext::StaticStruct();
	}

	virtual FAssemblyEffectContext* Duplicate() const override
	{
		FAssemblyEffectContext* NewCtx = new FAssemblyEffectContext();
		*NewCtx = *this;
		// 注意：FGameplayEffectContext 的字段（含 HitResult TSharedPtr）通过赋值已完成复制，
		// 不再重复调用 AddHitResult。Extension（TInstancedStruct）的赋值会深拷贝内部值。
		return NewCtx;
	}

	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;
};

template<>
struct TStructOpsTypeTraits<FAssemblyEffectContext> : public TStructOpsTypeTraitsBase2<FAssemblyEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true,
	};
};
