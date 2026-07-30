#pragma once

#include "Modules/ModuleManager.h"

class FPickupFrameworkRuntimeModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
