#pragma once

#include "CoreMinimal.h"
#include "Experimental/AbilityData/FAssemblyAbilityRow.h"
#include "DataRegistryId.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AssemblyAbilityStatics.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * Assembly Ability Framework 的静态工具入口。
 *
 * 提供从 FAssemblyAbilityRow / DataRegistry 授予技能、查找已授予技能等工具函数。
 *
 * 授予管线（GiveAbilityFromRow）：
 *   1. 设置 Spec.InputID + DynamicSpecSourceTags（从 Row.InputTag/AbilityTag）
 *   2. 创建 UAssemblyAbilitySource（持有 Row 副本 + OriginSource）
 *   3. Spec.SourceObject = UAssemblyAbilitySource
 *   4. ASC->GiveAbility(Spec)
 *   5. 确保此 ASC 上有 EventRouter GA
 *
 * Trigger 注册路径（数据驱动）：
 *   编辑器 GE 资产挂 UGETriggerComponent_Assembly + StaticTriggers，
 *   或调用方在 Apply 时把 Triggers 写入 EffectContext.Extension(FAssemblyContextExt_Triggers)。
 *   EventRouter 监听 ASC 上每个新 ActiveGE 自动从 Context 拉 Triggers — 无需调用注册 API。
 */
UCLASS()
class ASSEMBLYABILITYFRAMEWORKRUNTIME_API UAssemblyAbilityStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 从 FAssemblyAbilityRow 创建 Spec、编译并授予给指定 ASC。所有数据源的最终授权入口。 */
	UFUNCTION(BlueprintCallable, Category = "AssemblyAbility")
	static FGameplayAbilitySpecHandle GiveAbilityFromRow(
		const FAssemblyAbilityRow& Row,
		UAbilitySystemComponent* ASC,
		int32 Level = 1,
		UObject* OriginSource = nullptr);

	/** 从 DataRegistry 查询技能数据行，委托 GiveAbilityFromRow 授予。 */
	UFUNCTION(BlueprintCallable, Category = "AssemblyAbility")
	static FGameplayAbilitySpecHandle GiveAbilityFromDataRegistry(
		FDataRegistryId AbilityId,
		UAbilitySystemComponent* ASC,
		int32 Level = 1,
		UObject* OriginSource = nullptr);

	/** 从 GameplayTag 查询技能数据行并授予（便捷封装，Registry 类型固定为 "AssemblyAbility"）。 */
	UFUNCTION(BlueprintCallable, Category = "AssemblyAbility")
	static FGameplayAbilitySpecHandle GiveAbilityFromTag(
		FGameplayTag AbilityTag,
		UAbilitySystemComponent* ASC,
		int32 Level = 1,
		UObject* OriginSource = nullptr);

	/** 在 ASC 中按 AbilityTag（DynamicSpecSourceTags）搜索已授予的技能 SpecHandle。 */
	UFUNCTION(BlueprintCallable, Category = "AssemblyAbility")
	static FGameplayAbilitySpecHandle FindAbilityByAbilityTag(
		UAbilitySystemComponent* ASC,
		FGameplayTag AbilityTag);

private:
	/** 确保目标 ASC 上有 EventRouter GA。内部调用。 */
	static class UGA_AssemblyEventRouter* GetOrCreateEventRouter(UAbilitySystemComponent* ASC);
};
