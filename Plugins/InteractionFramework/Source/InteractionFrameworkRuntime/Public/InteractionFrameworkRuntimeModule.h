#pragma once

#include "Modules/ModuleManager.h"

class FInteractionFrameworkRuntimeModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
