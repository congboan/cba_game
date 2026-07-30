#pragma once

#include "CoreMinimal.h"
#include "Experimental/AbilityData/FAssemblyAbilityRow.h"
#include "UAssemblyAbilitySource.generated.h"

/**
 * SourceObject 包装 — 持有 Row 副本 + 来源对象。
 *
 * 解决 Spec.SourceObject 语义冲突：
 * - GAS 约定 SourceObject 表示"GA 的来源对象"（如武器）
 * - AAF 需要 SourceObject 存放技能运行时数据
 * - 通过包装对象两者兼得
 *
 * 需要回溯来源时：Cast<UAssemblyAbilitySource>(Spec->SourceObject)->OriginSource。
 */
UCLASS()
class ASSEMBLYABILITYFRAMEWORKRUNTIME_API UAssemblyAbilitySource : public UObject
{
	GENERATED_BODY()

public:
	/** 技能定义行副本（运行时只读）。GA 从此读取所有数据。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AssemblyAbility")
	FAssemblyAbilityRow Row;

	/** 来源对象（武器 EquipmentInstance 等），可为 nullptr。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AssemblyAbility")
	TObjectPtr<UObject> OriginSource = nullptr;
};
