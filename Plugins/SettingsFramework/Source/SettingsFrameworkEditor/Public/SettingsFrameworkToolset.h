#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "SettingsFrameworkToolset.generated.h"

/**
 * SettingsFramework MCP 工具集：AI 通过 MCP 创建/配置/保存
 * USettingCollection 资产（Entry 单树，Page/Group 容器 + Children 递归）。
 */
UCLASS(BlueprintType, MinimalAPI)
class USettingsFrameworkToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * 创建 USettingCollection 资产。
	 * @param AssetPath 资产路径，如 /Game/Settings/GameSettings
	 * @param CollectionName 集合显示名
	 * @param HostClassName UGameUserSettings 子类路径，如 /Script/CBA.MyGameUserSettings
	 * @return 成功返回资产路径，失败返回空
	 */
	UFUNCTION(meta = (AICallable), Category = "SettingsFramework")
	static FString CreateSettingCollectionAsset(
		const FString& AssetPath,
		const FString& CollectionName,
		const FString& HostClassName);

	/**
	 * 用 JSON 覆盖填充 Collection 资产（Entries 递归树）。
	 * JSON: {"CollectionName","DevName","Entries":[{"DisplayName",
	 * "DevName","ValueType","BindingPath","DefaultValue","Children":[...]}]}
	 * @return true 成功
	 */
	UFUNCTION(meta = (AICallable), Category = "SettingsFramework")
	static bool SetSettingCollectionFromJson(
		const FString& AssetPath,
		const FString& CollectionJson);

	/**
	 * 保存资产。
	 * @return true 成功
	 */
	UFUNCTION(meta = (AICallable), Category = "SettingsFramework")
	static bool SaveSettingCollectionAsset(const FString& AssetPath);

	/**
	 * 读取资产为 JSON（验证用）。
	 */
	UFUNCTION(meta = (AICallable), Category = "SettingsFramework")
	static FString GetSettingCollectionAsJson(const FString& AssetPath);
};
