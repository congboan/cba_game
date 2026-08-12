// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class SWidget;
class SComboButton;
class SEditableTextBox;

/** FSettingBindingPath 的属性选择器：基于所属 Collection 的 HostClass 枚举属性，点选回填路径。 */
class FSettingBindingPathCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	TSharedRef<SWidget> BuildPicker();
	FText OnGetPathText() const;
	void OnPathCommitted(const FText& Text, ETextCommit::Type CommitType);
	void SetPath(FString InPath);

	/** 沿 Outer 链找所属 USettingCollection（Entry 是 Instanced，Outer 为 Collection）。 */
	class USettingCollection* FindOwningCollection() const;

	TSharedPtr<IPropertyHandle> PathHandle;
	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr<SEditableTextBox> PathTextBox;
};
