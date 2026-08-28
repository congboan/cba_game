#pragma once

#include "CoreMinimal.h"
#include "AbilityData/FAssemblyAbilityRow.h"
#include "UAssemblyAbilitySource.generated.h"

class UAbilitySystemComponent;

/**
 * 授予期编译产物 — Row 配置 + 运行时分派索引的伴生结构。
 *
 * 配置形态（FAssemblyAbilityRow：DataTable / JSON 创作接口）在授予时快照进本结构，
 * 连同按钩子窄基类类型组织的分派索引（HookIndices）一起构成运行时唯一真相源。
 * 授予后只读（索引惰性构建一次，Row 为快照故永不失效）。
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyAbilityCompiledRow
{
	GENERATED_BODY()

	/** 技能定义行快照（运行时只读）。GA 从此读取所有数据。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AssemblyAbility")
	FAssemblyAbilityRow Row;

	/**
	 * Dispatch index (lazy "runtime compile"): key = dispatch type UScriptStruct*
	 * (hook narrow base OR data-fragment type, both valid ForEachHook keys),
	 * value = indices of Row.Fragments whose type IsChildOf(key).
	 * Hook base key -> shared bucket of implementing fragments; data-fragment key -> own bucket
	 * (aggregate entry). First call builds once; afterwards O(hits), no per-call full scan.
	 * Row is a grant-time snapshot (read-only), so indices never go stale.
	 * Non-reflected (TMap cannot be UPROPERTY); lifecycle tied to this struct.
	 */
	mutable TMap<const UScriptStruct*, TArray<int32>> HookIndices;

	/** Indices for one dispatch type (lazy build; single-threaded after grant). */
	template<typename HookT>
	const TArray<int32>& GetHookIndices() const
	{
		const UScriptStruct* Key = HookT::StaticStruct();
		if (const TArray<int32>* Cached = HookIndices.Find(Key))
		{
			return *Cached;
		}

		TArray<int32>& Indices = HookIndices.Add(Key);
		for (int32 Idx = 0; Idx < Row.Fragments.Num(); ++Idx)
		{
			const UScriptStruct* FragStruct = Row.Fragments[Idx].GetScriptStruct();
			if (FragStruct && FragStruct->IsChildOf(Key))
			{
				Indices.Add(Idx);
			}
		}
		return Indices;
	}
};

/**
 * SourceObject 包装 — 持有授予期编译产物 + 来源对象。
 * 回溯来源：Cast<UAssemblyAbilitySource>(Spec->SourceObject)->OriginSource。
 * 触发声明经 Spec.DynamicAbilityTriggers 由引擎托管；热路径缓存归片段/策略实例自身。
 */
UCLASS()
class ASSEMBLYABILITYFRAMEWORKRUNTIME_API UAssemblyAbilitySource : public UObject
{
	GENERATED_BODY()

public:
	/** 授予期编译产物（Row 快照 + 钩子分派索引）。GA 的唯一数据入口。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AssemblyAbility")
	FAssemblyAbilityCompiledRow CompiledRow;

	/** 来源对象（武器 EquipmentInstance 等），可为 nullptr。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AssemblyAbility")
	TObjectPtr<UObject> OriginSource = nullptr;
};

// 分派助手（查表版）：首次调用编译索引，之后 O(命中数)。高频钩子零重构即享。
// 类型安全：GetPtr<HookT>() 内部做 IsChildOf 检查（不匹配返回 nullptr），无需 static_cast；
// 片为授予期快照（运行时只读），Fn 一律收 const HookT&，片方法必须 const。
template<typename HookT, typename FnT>
void ForEachHook(const UAssemblyAbilitySource* Source, FnT&& Fn)
{
	if (!Source)
	{
		return;
	}

	for (const int32 Idx : Source->CompiledRow.GetHookIndices<HookT>())
	{
		const TInstancedStruct<FAssemblyFragmentBase>& Fragment = Source->CompiledRow.Row.Fragments[Idx];
		if (const HookT* Ptr = Fragment.GetPtr<HookT>())
		{
			Fn(*Ptr);
		}
	}
}
