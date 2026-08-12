// Copyright Epic Games, Inc. All Rights Reserved.

#include "SettingsFrameworkEditorModule.h"
#include "PropertyEditorModule.h"
#include "Customization/SettingBindingPathCustomization.h"
#include "SettingsFrameworkToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#define LOCTEXT_NAMESPACE "FSettingsFrameworkEditorModule"

void FSettingsFrameworkEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(
		"SettingBindingPath",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FSettingBindingPathCustomization::MakeInstance));

	UToolsetRegistry::RegisterToolsetClass(USettingsFrameworkToolset::StaticClass());
}

void FSettingsFrameworkEditorModule::ShutdownModule()
{
	UToolsetRegistry::UnregisterToolsetClass(USettingsFrameworkToolset::StaticClass());

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout("SettingBindingPath");
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSettingsFrameworkEditorModule, SettingsFrameworkEditor)
