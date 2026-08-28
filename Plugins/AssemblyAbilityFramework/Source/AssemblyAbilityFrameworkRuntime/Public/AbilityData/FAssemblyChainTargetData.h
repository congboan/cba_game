#pragma once

#include "CoreMinimal.h"
#include "AbilityData/FAssemblyChainContext.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "FAssemblyChainTargetData.generated.h"

// ── 链上下文载体版本适配 ───────────────────────────
// FGameplayEventData::InstancedEventData 为 UE 5.9 新增字段；<5.9 时链上下文改经
// TargetData 通道（本文件的自定义 FGameplayAbilityTargetData 子类）传递。
// 使用者只调 API（UAssemblyAbilityStatics::SetChainContext / GetChainContextFrom），
// 内部按本宏决定填充位置 —— 适配层对使用者透明。
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 9)
#define AAF_HAS_INSTANCED_EVENT_DATA 1
#else
#define AAF_HAS_INSTANCED_EVENT_DATA 0
#endif

/**
 * 链上下文的目标数据载体 — 让 FAssemblyChainContext 可经 FGameplayAbilityTargetDataHandle
 * 多态数组传递（<5.9 无 InstancedEventData 时的通道；5.9+ 编译但不被使用，UHT 禁止
 * 反射类型位于预处理器块内，故类始终存在）。
 *
 * TargetData 官方即多态扩展容器（自带 LocationInfo/ActorArray/SingleTargetHit 三类），
 * 自定义子类为常规用法。本类仅作链上下文透传载体：
 *   - GetActors 返回空（不是目标数据，执行策略按类型跳过本类）；
 *   - NetSerialize 序列化 FAssemblyChainContext 四字段。
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyChainTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	FAssemblyChainTargetData() = default;
	explicit FAssemblyChainTargetData(const FAssemblyChainContext& InContext)
		: Context(InContext)
	{
	}

	/** 承载的链上下文（读取侧 FindFirst<本类> 取回）。 */
	UPROPERTY()
	FAssemblyChainContext Context;

	// ── FGameplayAbilityTargetData 覆写 ─────────────

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FAssemblyChainTargetData::StaticStruct();
	}

	virtual bool HasHitResult() const override
	{
		return false;
	}

	// 注：基类 NetSerialize 非虚（官方子类同），序列化经 TStructOpsTypeTraits WithNetSerializer 反射分派
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << Context.ChainDepth;

		// FGuid 直接序列化
		Ar << Context.RootEventId;

		// InstigatorActor — 走 PackageMap（TWeakObjectPtr<AActor>）
		UObject* RawObject = Context.InstigatorActor.Get();
		Map->SerializeObject(Ar, AActor::StaticClass(), RawObject);
		if (Ar.IsLoading())
		{
			Context.InstigatorActor = Cast<AActor>(RawObject);
		}

		// DamageModifiers — TMap<FGameplayTag, float> 手写循环
		int32 Num = Context.DamageModifiers.Num();
		Ar << Num;
		if (Ar.IsLoading())
		{
			Context.DamageModifiers.Reset();
			for (int32 i = 0; i < Num; ++i)
			{
				FGameplayTag Key;
				Key.NetSerialize(Ar, Map, bOutSuccess);
				float Value = 0.0f;
				Ar << Value;
				Context.DamageModifiers.Add(Key, Value);
			}
		}
		else
		{
			for (auto& [Key, Value] : Context.DamageModifiers)
			{
				Key.NetSerialize(Ar, Map, bOutSuccess);
				Ar << Value;
			}
		}

		bOutSuccess = true;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FAssemblyChainTargetData> : public TStructOpsTypeTraitsBase2<FAssemblyChainTargetData>
{
	enum
	{
		WithNetSerializer = true,
	};
};
