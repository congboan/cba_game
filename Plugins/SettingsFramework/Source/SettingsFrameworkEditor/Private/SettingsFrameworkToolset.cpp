#include "SettingsFrameworkToolset.h"
#include "Data/SettingCollection.h"
#include "Data/SettingEntry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "PackageTools.h"
#include "Misc/PackageName.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace SettingsFrameworkToolsetPrivate
{
	ESettingValueType ParseValueType(const FString& Str)
	{
		if (Str == TEXT("Scalar")) return ESettingValueType::Scalar;
		if (Str == TEXT("Bool")) return ESettingValueType::Bool;
		if (Str == TEXT("Enum")) return ESettingValueType::Enum;
		if (Str == TEXT("Action")) return ESettingValueType::Action;
		if (Str == TEXT("Page")) return ESettingValueType::Page;
		if (Str == TEXT("Group")) return ESettingValueType::Group;
		return ESettingValueType::Scalar;
	}

	const TCHAR* ValueTypeToString(ESettingValueType Type)
	{
		switch (Type)
		{
		case ESettingValueType::Scalar: return TEXT("Scalar");
		case ESettingValueType::Bool: return TEXT("Bool");
		case ESettingValueType::Enum: return TEXT("Enum");
		case ESettingValueType::Action: return TEXT("Action");
		case ESettingValueType::Page: return TEXT("Page");
		case ESettingValueType::Group: return TEXT("Group");
		default: return TEXT("Scalar");
		}
	}

	USettingEntry* ParseEntry(const TSharedPtr<FJsonObject>& Obj, UObject* Outer)
	{
		USettingEntry* Entry = NewObject<USettingEntry>(Outer);
		Entry->DisplayName = FText::FromString(Obj->GetStringField(TEXT("DisplayName")));
		Entry->Description = FText::FromString(Obj->GetStringField(TEXT("Description")));
		Entry->DevName = FName(*Obj->GetStringField(TEXT("DevName")));
		Entry->ValueType = ParseValueType(Obj->GetStringField(TEXT("ValueType")));
		Entry->BindingPath.Path = Obj->GetStringField(TEXT("BindingPath"));
		Entry->DefaultValue = Obj->GetStringField(TEXT("DefaultValue"));

		const TSharedPtr<FJsonObject>* RangePtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("ScalarRange"), RangePtr))
		{
			Entry->ScalarRange.Min = (*RangePtr)->GetNumberField(TEXT("Min"));
			Entry->ScalarRange.Max = (*RangePtr)->GetNumberField(TEXT("Max"));
			Entry->ScalarRange.Step = (*RangePtr)->GetNumberField(TEXT("Step"));
		}

		const TArray<TSharedPtr<FJsonValue>>* OptionsPtr = nullptr;
		if (Obj->TryGetArrayField(TEXT("Options"), OptionsPtr))
		{
			for (const TSharedPtr<FJsonValue>& Val : *OptionsPtr)
			{
				const TSharedPtr<FJsonObject>* OptPtr = nullptr;
				if (Val->TryGetObject(OptPtr))
				{
					FSettingOption Option;
					Option.Label = FText::FromString((*OptPtr)->GetStringField(TEXT("Label")));
					Option.Value = (*OptPtr)->GetStringField(TEXT("Value"));
					Entry->Options.Add(Option);
				}
			}
		}
		// PlatformTraits（条件标签，缺失时 Hide）
		const TArray<TSharedPtr<FJsonValue>>* TraitsPtr = nullptr;
		if (Obj->TryGetArrayField(TEXT("PlatformTraits"), TraitsPtr))
		{
			for (const TSharedPtr<FJsonValue>& Val : *TraitsPtr)
			{
				const FString TagStr = Val->AsString();
				if (!TagStr.IsEmpty())
				{
					Entry->PlatformTraits.AddTag(
						FGameplayTag::RequestGameplayTag(FName(*TagStr)));
				}
			}
		}

		// EditConditionTag（条件标签，缺失时 Disable）
		FString EditConditionTagStr;
		if (Obj->TryGetStringField(TEXT("EditConditionTag"), EditConditionTagStr)
			&& !EditConditionTagStr.IsEmpty())
		{
			Entry->EditConditionTag =
				FGameplayTag::RequestGameplayTag(FName(*EditConditionTagStr));
		}

		// EditDependencyDevNames（依赖设置 DevName，值变化时刷新）
		const TArray<TSharedPtr<FJsonValue>>* DepPtr = nullptr;
		if (Obj->TryGetArrayField(TEXT("EditDependencyDevNames"), DepPtr))
		{
			for (const TSharedPtr<FJsonValue>& Val : *DepPtr)
			{
				const FString DepName = Val->AsString();
				if (!DepName.IsEmpty()) Entry->EditDependencyDevNames.Add(FName(*DepName));
			}
		}

		// ValueConditions（值依赖条件）
		const TArray<TSharedPtr<FJsonValue>>* ValueCondPtr = nullptr;
		if (Obj->TryGetArrayField(TEXT("ValueConditions"), ValueCondPtr))
		{
			for (const TSharedPtr<FJsonValue>& Val : *ValueCondPtr)
			{
				const TSharedPtr<FJsonObject>* CondPtr = nullptr;
				if (Val->TryGetObject(CondPtr))
				{
					FSettingValueCondition Cond;
					Cond.DependencyDevName = FName(*(*CondPtr)->GetStringField(TEXT("DependencyDevName")));
					Cond.MatchValue = (*CondPtr)->GetStringField(TEXT("MatchValue"));
					const FString ActionStr = (*CondPtr)->GetStringField(TEXT("Action"));
					Cond.Action = (ActionStr == TEXT("Hide"))
						? ESettingValueConditionAction::Hide
						: ESettingValueConditionAction::Disable;
					Entry->ValueConditions.Add(Cond);
				}
			}
		}
		// Children 递归（Page/Group 容器节点）
		const TArray<TSharedPtr<FJsonValue>>* ChildrenPtr = nullptr;
		if (Obj->TryGetArrayField(TEXT("Children"), ChildrenPtr))
		{
			for (const TSharedPtr<FJsonValue>& Val : *ChildrenPtr)
			{
				const TSharedPtr<FJsonObject>* ChildPtr = nullptr;
				if (Val->TryGetObject(ChildPtr))
				{
					Entry->Children.Add(ParseEntry(*ChildPtr, Entry));
				}
			}
		}
		return Entry;
	}

	TSharedPtr<FJsonObject> EntryToJson(const USettingEntry* Entry)
	{
		TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject());
		Obj->SetStringField(TEXT("DisplayName"), Entry->DisplayName.ToString());
		Obj->SetStringField(TEXT("Description"), Entry->Description.ToString());
		Obj->SetStringField(TEXT("DevName"), Entry->DevName.ToString());
		Obj->SetStringField(TEXT("ValueType"), ValueTypeToString(Entry->ValueType));
		Obj->SetStringField(TEXT("BindingPath"), Entry->BindingPath.Path);
		Obj->SetStringField(TEXT("DefaultValue"), Entry->DefaultValue);

		// ScalarRange
		if (Entry->ValueType == ESettingValueType::Scalar)
		{
			TSharedPtr<FJsonObject> RangeObj = MakeShareable(new FJsonObject());
			RangeObj->SetNumberField(TEXT("Min"), Entry->ScalarRange.Min);
			RangeObj->SetNumberField(TEXT("Max"), Entry->ScalarRange.Max);
			RangeObj->SetNumberField(TEXT("Step"), Entry->ScalarRange.Step);
			Obj->SetObjectField(TEXT("ScalarRange"), RangeObj);
		}

		// Options
		if (Entry->Options.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> OptionsJson;
			for (const FSettingOption& Option : Entry->Options)
			{
				TSharedPtr<FJsonObject> OptObj = MakeShareable(new FJsonObject());
				OptObj->SetStringField(TEXT("Label"), Option.Label.ToString());
				OptObj->SetStringField(TEXT("Value"), Option.Value);
				OptionsJson.Add(MakeShareable(new FJsonValueObject(OptObj)));
			}
			Obj->SetArrayField(TEXT("Options"), OptionsJson);
		}

		// PlatformTraits
		if (Entry->PlatformTraits.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> TraitsJson;
			for (const FGameplayTag& Tag : Entry->PlatformTraits)
			{
				TraitsJson.Add(MakeShareable(new FJsonValueString(Tag.ToString())));
			}
			Obj->SetArrayField(TEXT("PlatformTraits"), TraitsJson);
		}

		// EditConditionTag / EditDependencyDevNames
		if (Entry->EditConditionTag.IsValid())
		{
			Obj->SetStringField(TEXT("EditConditionTag"), Entry->EditConditionTag.ToString());
		}
		if (Entry->EditDependencyDevNames.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> DepJson;
			for (const FName& DepName : Entry->EditDependencyDevNames)
			{
				DepJson.Add(MakeShareable(new FJsonValueString(DepName.ToString())));
			}
			Obj->SetArrayField(TEXT("EditDependencyDevNames"), DepJson);
		}

		// ValueConditions
		if (Entry->ValueConditions.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> CondJson;
			for (const FSettingValueCondition& Cond : Entry->ValueConditions)
			{
				TSharedPtr<FJsonObject> CondObj = MakeShareable(new FJsonObject());
				CondObj->SetStringField(TEXT("DependencyDevName"), Cond.DependencyDevName.ToString());
				CondObj->SetStringField(TEXT("MatchValue"), Cond.MatchValue);
				CondObj->SetStringField(TEXT("Action"),
					Cond.Action == ESettingValueConditionAction::Hide ? TEXT("Hide") : TEXT("Disable"));
				CondJson.Add(MakeShareable(new FJsonValueObject(CondObj)));
			}
			Obj->SetArrayField(TEXT("ValueConditions"), CondJson);
		}
		if (Entry->Children.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ChildrenJson;
			for (const USettingEntry* Child : Entry->Children)
			{
				if (Child)
				{
					ChildrenJson.Add(MakeShareable(new FJsonValueObject(EntryToJson(Child))));
				}
			}
			Obj->SetArrayField(TEXT("Children"), ChildrenJson);
		}
		return Obj;
	}
}

FString USettingsFrameworkToolset::CreateSettingCollectionAsset(
	const FString& AssetPath, const FString& CollectionName, const FString& HostClassName)
{
	FString PackageName = FPackageName::IsValidObjectPath(AssetPath)
		? FPackageName::ObjectPathToPackageName(AssetPath) : AssetPath;
	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		UE_LOG(LogTemp, Warning, TEXT("SettingsFrameworkToolset: 非法资产路径 %s"), *AssetPath);
		return FString();
	}

	UClass* HostClass = nullptr;
	if (!HostClassName.IsEmpty())
	{
		HostClass = FSoftClassPath(HostClassName).TryLoadClass<UGameUserSettings>();
		if (!HostClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("SettingsFrameworkToolset: 宿主类解析失败 %s"), *HostClassName);
			return FString();
		}
	}

	const FString AssetName = FPackageName::GetShortName(PackageName);
	UPackage* Package = CreatePackage(*PackageName);
	USettingCollection* Collection = NewObject<USettingCollection>(
		Package, *AssetName, RF_Public | RF_Standalone);
	if (!Collection) return FString();

	Collection->CollectionName = FText::FromString(CollectionName);
	Collection->HostClass = HostClass;

	IAssetRegistry::Get()->AssetCreated(Collection);
	Package->MarkPackageDirty();
	UPackageTools::SavePackagesForObjects(TArray<UObject*>{ Collection });

	return Collection->GetPathName();
}

bool USettingsFrameworkToolset::SetSettingCollectionFromJson(
	const FString& AssetPath, const FString& CollectionJson)
{
	USettingCollection* Collection = LoadObject<USettingCollection>(nullptr, *AssetPath);
	if (!Collection)
	{
		UE_LOG(LogTemp, Warning, TEXT("SettingsFrameworkToolset: 资产不存在 %s"), *AssetPath);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CollectionJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("SettingsFrameworkToolset: JSON 解析失败"));
		return false;
	}

	Collection->CollectionName = FText::FromString(Root->GetStringField(TEXT("CollectionName")));
	Collection->DevName = FName(*Root->GetStringField(TEXT("DevName")));

	// 解析 Entries（Merge 模式按 DevName 合并，缺省整体覆盖）
	TArray<USettingEntry*> ParsedEntries;
	const TArray<TSharedPtr<FJsonValue>>* EntriesPtr = nullptr;
	if (Root->TryGetArrayField(TEXT("Entries"), EntriesPtr))
	{
		for (const TSharedPtr<FJsonValue>& Val : *EntriesPtr)
		{
			const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
			if (Val->TryGetObject(EntryPtr))
			{
				ParsedEntries.Add(
					SettingsFrameworkToolsetPrivate::ParseEntry(*EntryPtr, Collection));
			}
		}
	}

	bool bMerge = false;
	Root->TryGetBoolField(TEXT("Merge"), bMerge);
	if (bMerge)
	{
		for (USettingEntry* NewEntry : ParsedEntries)
		{
			bool bReplaced = false;
			for (int32 i = 0; i < Collection->Entries.Num(); ++i)
			{
				if (Collection->Entries[i]
					&& Collection->Entries[i]->DevName == NewEntry->DevName)
				{
					Collection->Entries[i] = NewEntry;
					bReplaced = true;
					break;
				}
			}
			if (!bReplaced)
			{
				Collection->Entries.Add(NewEntry);
			}
		}
	}
	else
	{
		Collection->Entries.Reset();
		Collection->Entries = ParsedEntries;
	}

	Collection->MarkPackageDirty();
	return true;
}

bool USettingsFrameworkToolset::SaveSettingCollectionAsset(const FString& AssetPath)
{
	USettingCollection* Collection = LoadObject<USettingCollection>(nullptr, *AssetPath);
	if (!Collection) return false;
	return UPackageTools::SavePackagesForObjects(TArray<UObject*>{ Collection });
}

FString USettingsFrameworkToolset::GetSettingCollectionAsJson(const FString& AssetPath)
{
	USettingCollection* Collection = LoadObject<USettingCollection>(nullptr, *AssetPath);
	if (!Collection) return FString();

	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());
	Root->SetStringField(TEXT("AssetPath"), Collection->GetPathName());
	Root->SetStringField(TEXT("CollectionName"), Collection->CollectionName.ToString());
	Root->SetStringField(TEXT("DevName"), Collection->DevName.ToString());
	if (Collection->HostClass)
	{
		Root->SetStringField(TEXT("HostClass"), Collection->HostClass->GetPathName());
	}

	TArray<TSharedPtr<FJsonValue>> EntriesJson;
	for (const USettingEntry* Entry : Collection->Entries)
	{
		if (Entry)
		{
			EntriesJson.Add(MakeShareable(
				new FJsonValueObject(SettingsFrameworkToolsetPrivate::EntryToJson(Entry))));
		}
	}
	Root->SetArrayField(TEXT("Entries"), EntriesJson);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Output;
}

