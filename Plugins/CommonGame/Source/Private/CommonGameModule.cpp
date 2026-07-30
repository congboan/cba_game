// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

/**
 * 实现 FCommonGameModule 模块。
 */
class FCommonGameModule : public IModuleInterface
{
public:
	FCommonGameModule();
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

};


FCommonGameModule::FCommonGameModule()
{
}

void FCommonGameModule::StartupModule()
{
}

void FCommonGameModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FCommonGameModule, CommonGame);
