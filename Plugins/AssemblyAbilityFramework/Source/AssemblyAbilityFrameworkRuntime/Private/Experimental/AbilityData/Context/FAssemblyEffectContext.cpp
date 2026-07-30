#include "Experimental/AbilityData/Context/FAssemblyEffectContext.h"
#include "Experimental/AbilityData/Context/FAssemblyContextExt_Triggers.h"
#include "AbilityData/FAssemblyAbilityTrigger.h"
#include "Experimental/UAssemblyAbilitySource.h"
#include "Engine/PackageMapClient.h"
#include "StructUtils/InstancedStruct.h"

// ── 工厂便捷函数 ─────────────────────────────────

FGameplayEffectContextHandle FAssemblyEffectContext::MakeForAssembly(
	UAssemblyAbilitySource* InAssemblySource,
	FGameplayTag InOriginAbilityTag,
	AActor* Instigator,
	AActor* EffectCauser)
{
	FAssemblyEffectContext* NewCtx = new FAssemblyEffectContext();
	NewCtx->AssemblySource    = InAssemblySource;
	NewCtx->OriginAbilityTag  = InOriginAbilityTag;
	NewCtx->AddInstigator(Instigator, EffectCauser);

	return FGameplayEffectContextHandle(NewCtx);
}

FGameplayEffectContextHandle FAssemblyEffectContext::MakeWithTriggers(
	UAssemblyAbilitySource* InAssemblySource,
	FGameplayTag InOriginAbilityTag,
	const TArray<FAssemblyAbilityTrigger>& Triggers,
	AActor* Instigator,
	AActor* EffectCauser)
{
	FGameplayEffectContextHandle Handle = MakeForAssembly(InAssemblySource, InOriginAbilityTag, Instigator, EffectCauser);

	if (Triggers.IsEmpty())
	{
		return Handle;
	}

	FAssemblyEffectContext* NewCtx = static_cast<FAssemblyEffectContext*>(Handle.Get());
	FAssemblyContextExt_Triggers TriggersExt;
	TriggersExt.Triggers = Triggers;
	NewCtx->Extension = TInstancedStruct<FAssemblyContextExtension>::Make(TriggersExt);

	return Handle;
}

// ── 读取助手 ─────────────────────────────────────

const TArray<FAssemblyAbilityTrigger>* FAssemblyEffectContext::GetTriggers() const
{
	if (!Extension.IsValid())
	{
		return nullptr;
	}

	if (const FAssemblyContextExt_Triggers* TriggersExt = Extension.GetPtr<FAssemblyContextExt_Triggers>())
	{
		return &TriggersExt->Triggers;
	}

	return nullptr;
}

// ── NetSerialize ─────────────────────────────────

bool FAssemblyEffectContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	// 先走父类序列化所有标准 GAS 字段（Instigator/EffectCauser/HitResult/SourceObject 等）
	if (!FGameplayEffectContext::NetSerialize(Ar, Map, bOutSuccess))
	{
		return false;
	}

	// AssemblySource — 走 PackageMap 序列化 UObject 引用（与 SourceObject 同机制）
	UObject* SrcPtr = AssemblySource;
	Map->SerializeObject(Ar, UAssemblyAbilitySource::StaticClass(), SrcPtr);
	if (Ar.IsLoading())
	{
		AssemblySource = Cast<UAssemblyAbilitySource>(SrcPtr);
	}

	// OriginAbilityTag
	OriginAbilityTag.NetSerialize(Ar, Map, bOutSuccess);

	// Extension（TInstancedStruct 自带 NetSerialize）
	Extension.NetSerialize(Ar, Map, bOutSuccess);

	bOutSuccess = true;
	return true;
}
