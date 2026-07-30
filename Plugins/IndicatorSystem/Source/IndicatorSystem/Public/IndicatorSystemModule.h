#pragma once

#include "Modules/ModuleManager.h"

class FIndicatorSystemModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
