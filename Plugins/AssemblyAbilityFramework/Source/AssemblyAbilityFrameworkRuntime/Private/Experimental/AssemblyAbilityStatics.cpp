#include "Experimental/AssemblyAbilityStatics.h"
#include "Experimental/AbilityData/FAssemblyAbilityRow.h"
#include "Experimental/UAssemblyAbilitySource.h"
#include "Experimental/AbilitySystem/UGA_AssemblyDataDriven.h"
#include "AbilitySystem/UGA_AssemblyEventRouter.h"
#include "AbilitySystemComponent.h"
#include "AssemblyAbilityFrameworkLog.h"
#include "DataRegistrySubsystem.h"
#include "GameplayEffect.h"

// ── GiveAbilityFromRow — 核心授权入口 ──────────────

FGameplayAbilitySpecHandle UAssemblyAbilityStatics::GiveAbilityFromRow(
	const FAssemblyAbilityRow& Row,
	UAbilitySystemComponent* ASC,
	int32 Level,
	UObject* OriginSource)
{
	if (!ASC)
	{
		return FGameplayAbilitySpecHandle();
	}

	const FGameplayTag AbilityTag = Row.AbilityTag;
	if (!AbilityTag.IsValid())
	{
		return FGameplayAbilitySpecHandle();
	}

	TSubclassOf<UGameplayAbility> AbilityClass = UGA_AssemblyDataDriven::StaticClass();
	if (Row.AbilityClass.Get())
	{
		AbilityClass = Row.AbilityClass;
	}

	// 步骤 1：创建 Spec
	FGameplayAbilitySpec Spec(AbilityClass, Level);
	Spec.GetDynamicSpecSourceTags().AddTag(AbilityTag);

	// 步骤 2：设置 InputID（如有 InputTag）
	if (Row.InputTag.IsValid())
	{
		Spec.InputID = GetTypeHash(Row.InputTag);
		Spec.GetDynamicSpecSourceTags().AddTag(Row.InputTag);
	}

	// 步骤 3：创建 Source 包装对象，持有 Row 副本
	UObject* SourceOuter = ASC->GetOwner();
	UAssemblyAbilitySource* AbilitySource = NewObject<UAssemblyAbilitySource>(SourceOuter);
	AbilitySource->Row = Row;  // 值拷贝
	AbilitySource->OriginSource = OriginSource;

	// 步骤 4：挂到 Spec.SourceObject
	Spec.SourceObject = AbilitySource;

	// 步骤 5：授予技能
	FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
	if (Handle.IsValid())
	{
		// 步骤 6：确保此 ASC 上 EventRouter 正在运行
		GetOrCreateEventRouter(ASC);

		UE_LOG(LogAssemblyAbility, Log,
			TEXT("[AssemblyAbility] GiveAbilityFromRow: 已授予 '%s' (Handle: %s, Level: %d, OriginSource: %s)"),
			*AbilityTag.ToString(), *Handle.ToString(), Level,
			OriginSource ? *OriginSource->GetName() : TEXT("none"));
	}

	return Handle;
}

// ── GiveAbilityFromDataRegistry — DataRegistry 查找 ──

FGameplayAbilitySpecHandle UAssemblyAbilityStatics::GiveAbilityFromDataRegistry(
	FDataRegistryId AbilityId,
	UAbilitySystemComponent* ASC,
	int32 Level,
	UObject* OriginSource)
{
	if (!ASC || !AbilityId.IsValid())
	{
		return FGameplayAbilitySpecHandle();
	}

	const UDataRegistrySubsystem* DRSubsystem = UDataRegistrySubsystem::Get();
	if (!DRSubsystem)
	{
		UE_LOG(LogAssemblyAbility, Warning,
			TEXT("[AssemblyAbility] GiveAbilityFromDataRegistry: DataRegistrySubsystem 不可用"));
		return FGameplayAbilitySpecHandle();
	}

	const FAssemblyAbilityRow* Row = DRSubsystem->GetCachedItem<FAssemblyAbilityRow>(AbilityId);
	if (!Row)
	{
		UE_LOG(LogAssemblyAbility, Warning,
			TEXT("[AssemblyAbility] GiveAbilityFromDataRegistry: 未找到 DataRegistry id '%s'"),
			*AbilityId.ToString());
		return FGameplayAbilitySpecHandle();
	}

	return GiveAbilityFromRow(*Row, ASC, Level, OriginSource);
}

// ── GiveAbilityFromTag — 便捷封装 ──────────────────

FGameplayAbilitySpecHandle UAssemblyAbilityStatics::GiveAbilityFromTag(
	FGameplayTag AbilityTag,
	UAbilitySystemComponent* ASC,
	int32 Level,
	UObject* OriginSource)
{
	if (!AbilityTag.IsValid() || !ASC)
	{
		return FGameplayAbilitySpecHandle();
	}

	const FDataRegistryId RegistryId(TEXT("AssemblyAbility"), AbilityTag.GetTagName());
	return GiveAbilityFromDataRegistry(RegistryId, ASC, Level, OriginSource);
}

// ── FindAbilityByAbilityTag ───────────────────────

FGameplayAbilitySpecHandle UAssemblyAbilityStatics::FindAbilityByAbilityTag(
	UAbilitySystemComponent* ASC,
	FGameplayTag AbilityTag)
{
	if (!ASC || !AbilityTag.IsValid())
	{
		return FGameplayAbilitySpecHandle();
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTag(AbilityTag))
		{
			return Spec.Handle;
		}
	}

	return FGameplayAbilitySpecHandle();
}

// ── 内部：EventRouter 获取/创建 ───────────────────

UGA_AssemblyEventRouter* UAssemblyAbilityStatics::GetOrCreateEventRouter(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->IsA<UGA_AssemblyEventRouter>())
		{
			return Cast<UGA_AssemblyEventRouter>(Spec.GetPrimaryInstance());
		}
	}

	FGameplayAbilitySpec RouterSpec(UGA_AssemblyEventRouter::StaticClass(), 1);
	FGameplayAbilitySpecHandle RouterHandle = ASC->GiveAbility(RouterSpec);

	bool bActivated = ASC->TryActivateAbility(RouterHandle);

	UGA_AssemblyEventRouter* Router = nullptr;
	if (bActivated)
	{
		FGameplayAbilitySpec* GrantedSpec = ASC->FindAbilitySpecFromHandle(RouterHandle);
		Router = GrantedSpec
			? Cast<UGA_AssemblyEventRouter>(GrantedSpec->GetPrimaryInstance())
			: nullptr;
	}

	if (!Router)
	{
		UE_LOG(LogAssemblyAbility, Error,
			TEXT("[AssemblyAbility] EventRouter 激活失败 (ASC: %s, Handle: %s)。"
			     "技能已授予但事件路由不可用，触发器将无法工作。"),
			*GetNameSafe(ASC->GetOwner()), *RouterHandle.ToString());
		return nullptr;
	}

	UE_LOG(LogAssemblyAbility, Log,
		TEXT("[AssemblyAbility] 已为 ASC %s 创建并激活 EventRouter (Handle: %s)"),
		*GetNameSafe(ASC->GetOwner()), *RouterHandle.ToString());

	return Router;
}
