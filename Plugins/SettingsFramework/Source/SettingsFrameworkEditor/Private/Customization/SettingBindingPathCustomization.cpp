// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customization/SettingBindingPathCustomization.h"
#include "Data/SettingCollection.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "SettingBindingPathCustomization"

TSharedRef<IPropertyTypeCustomization> FSettingBindingPathCustomization::MakeInstance()
{
	return MakeShareable(new FSettingBindingPathCustomization());
}

void FSettingBindingPathCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PathHandle = PropertyHandle->GetChildHandle(TEXT("Path"));

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(300.f)
		[
			SAssignNew(ComboButton, SComboButton)
			.OnGetMenuContent(this, &FSettingBindingPathCustomization::BuildPicker)
			.ButtonContent()
			[
				SAssignNew(PathTextBox, SEditableTextBox)
				.Text(this, &FSettingBindingPathCustomization::OnGetPathText)
				.OnTextCommitted(this, &FSettingBindingPathCustomization::OnPathCommitted)
			]
		];
}

void FSettingBindingPathCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Path 已在 header 内联显示，隐藏子属性行
}

FText FSettingBindingPathCustomization::OnGetPathText() const
{
	FString Value;
	if (PathHandle && PathHandle->GetValue(Value) == FPropertyAccess::Success)
	{
		return FText::FromString(Value);
	}
	return FText();
}

void FSettingBindingPathCustomization::OnPathCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (PathHandle)
	{
		PathHandle->SetValue(Text.ToString());
	}
}

void FSettingBindingPathCustomization::SetPath(FString InPath)
{
	if (PathHandle)
	{
		PathHandle->SetValue(InPath);
		PathHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
	if (PathTextBox.IsValid())
	{
		PathTextBox->SetText(FText::FromString(InPath));
	}
}

USettingCollection* FSettingBindingPathCustomization::FindOwningCollection() const
{
	if (!PathHandle) return nullptr;

	TArray<UObject*> OuterObjects;
	PathHandle->GetOuterObjects(OuterObjects);
	for (UObject* Obj : OuterObjects)
	{
		for (UObject* Cur = Obj; Cur; Cur = Cur->GetOuter())
		{
			if (USettingCollection* Collection = Cast<USettingCollection>(Cur))
			{
				return Collection;
			}
		}
	}
	return nullptr;
}

TSharedRef<SWidget> FSettingBindingPathCustomization::BuildPicker()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	USettingCollection* Collection = FindOwningCollection();
	if (!Collection || !Collection->HostClass)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NoHostClass", "(需在所属 Collection 配置 HostClass)"),
			FText(), FSlateIcon(), FUIAction());
		return MenuBuilder.MakeWidget();
	}

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("HostClassSection", "HostClass 属性"));
	for (TFieldIterator<FProperty> It(Collection->HostClass); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop || Prop->HasAnyPropertyFlags(CPF_EditorOnly) || Prop->IsA<FObjectProperty>())
		{
			continue;
		}
		const FString PropName = Prop->GetName();
		const FString DisplayText = Prop->GetDisplayNameText().IsEmpty()
			? PropName
			: Prop->GetDisplayNameText().ToString();
		MenuBuilder.AddMenuEntry(
			FText::FromString(DisplayText),
			FText::FromString(PropName),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FSettingBindingPathCustomization::SetPath, PropName)));
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
