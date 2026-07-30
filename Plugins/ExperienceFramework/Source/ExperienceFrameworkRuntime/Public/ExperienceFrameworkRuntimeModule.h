#pragma once

#include "Modules/ModuleManager.h"

class FExperienceFrameworkRuntimeModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
