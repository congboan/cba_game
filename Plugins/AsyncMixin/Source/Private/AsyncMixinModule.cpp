// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FAsyncMixinModule : public IModuleInterface
{
public:
	/** IModuleInterface 实现 */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

void FAsyncMixinModule::StartupModule()
{
	// 此代码将在你的模块加载到内存后执行；具体时间在 .uplugin 文件中按模块指定
}

void FAsyncMixinModule::ShutdownModule()
{
	// 此函数可能在关闭时被调用来清理模块。对于支持动态重新加载的模块，
	// 我们在卸载模块之前调用此函数。
}
	
IMPLEMENT_MODULE(FAsyncMixinModule, AsyncMixin)
