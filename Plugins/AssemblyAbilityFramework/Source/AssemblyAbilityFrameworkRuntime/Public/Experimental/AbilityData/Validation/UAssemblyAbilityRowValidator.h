#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UAssemblyAbilityRowValidator.generated.h"

/**
 * 编辑器专用 FAssemblyAbilityRow 数据验证器。
 *
 * 检查每行是否满足：
 *   - 拥有有效的 AbilityTag
 *   - 执行策略为 FAssemblyExecutionBase 子类（如配置了）
 *   - 冷却覆写配置一致性（CooldownOverrideTags 需要配合 CooldownGE）
 *   - ExecutionOverrides 标签和执行策略有效
 *
 * 此验证器可从以下位置运行：
 *   - DataTable 编辑器 → 右键 → Validate Data
 *   - Content Browser → 右键 → Asset Actions → Validate Assets
 */
UCLASS()
class ASSEMBLYABILITYFRAMEWORKRUNTIME_API UAssemblyAbilityRowValidator : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 验证单个 FAssemblyAbilityRow。
	 * @param Row     要验证的行。
	 * @param RowName 行名称（用于错误上下文）。
	 * @param OutErrors  错误消息输出数组（为空表示验证通过）。
	 * @return 行通过验证则返回 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "AssemblyAbility|Validation")
	static bool ValidateRow(const struct FAssemblyAbilityRow& Row, FName RowName, TArray<FString>& OutErrors);
};
