// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonUserModule.h"

#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FCommonUserModule"

void FCommonUserModule::StartupModule()
{
	// 此代码将在你的模块加载到内存后执行；具体时间在 .uplugin 文件中按模块指定
}

void FCommonUserModule::ShutdownModule()
{
	// 此函数可能在关闭时被调用来清理模块。对于支持动态重新加载的模块，
	// 我们在卸载模块之前调用此函数。
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCommonUserModule, CommonUser)