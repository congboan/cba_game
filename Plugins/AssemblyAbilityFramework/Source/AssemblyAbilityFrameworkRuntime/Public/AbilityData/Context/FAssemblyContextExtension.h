#pragma once

#include "CoreMinimal.h"
#include "FAssemblyContextExtension.generated.h"

/**
 * Assembly EffectContext 扩展基类（USTRUCT 抽象基）。
 *
 * 通过 TInstancedStruct<FAssemblyContextExtension> 多态承载在 FAssemblyEffectContext 中。
 * 单纯伤害 GE 不挂 Extension → Context 0 字段开销。
 *
 * 子类示例：
 *   - FAssemblyContextExt_Triggers：携带要注册的 GE Trigger 列表
 *   - 未来可加：DotConfig / ProjectileConfig / ChainContextSeed 等
 *
 * 设计契约：
 *   - 子类只放纯数据（不持有 UObject 强引用，避免 Context 复制时跨域）
 *   - 子类必须可 NetSerialize，由 FInstancedStruct 自身的 NetSerialize 链路承担
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyContextExtension
{
	GENERATED_BODY()

	virtual ~FAssemblyContextExtension() = default;
};
