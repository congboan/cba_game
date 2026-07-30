// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "CommonUserBasicPresence.generated.h"

#define UE_API COMMONUSER_API

class UCommonSessionSubsystem;
enum class ECommonSessionInformationState : uint8;

//////////////////////////////////////////////////////////////////////
// UCommonUserBasicPresence

/**
 * 此子系统接入会话子系统，并将其信息推送到 Presence 接口。
 * 它并非旨在成为功能齐全的富 Presence 实现，但可作为将信息从会话子系统
 * 推送到 Presence 系统的概念验证。
 */
UCLASS(MinimalAPI, BlueprintType, Config = Engine)
class UCommonUserBasicPresence : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UE_API UCommonUserBasicPresence();


	/** 实现此方法以初始化系统实例 */
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 实现此方法以反初始化系统实例 */
	UE_API virtual void Deinitialize() override;

	/** False 是一个通用的终止开关，用于阻止此类推送 Presence */
	UPROPERTY(Config)
	bool bEnableSessionsBasedPresence = false;

	/** 将 "In-game" Presence 状态映射到后端键 */
	UPROPERTY(Config)
	FString PresenceStatusInGame;

	/** 将 "Main Menu" Presence 状态映射到后端键 */
	UPROPERTY(Config)
	FString PresenceStatusMainMenu;

	/** 将 "Matchmaking" Presence 状态映射到后端键 */
	UPROPERTY(Config)
	FString PresenceStatusMatchmaking;

	/** 将 "Game Mode" 富 Presence 条目映射到后端键 */
	UPROPERTY(Config)
	FString PresenceKeyGameMode;

	/** 将 "Map Name" 富 Presence 条目映射到后端键 */
	UPROPERTY(Config)
	FString PresenceKeyMapName;

	UE_API void OnNotifySessionInformationChanged(ECommonSessionInformationState SessionStatus, const FString& GameMode, const FString& MapName);
	UE_API FString SessionStateToBackendKey(ECommonSessionInformationState SessionStatus);
};

#undef UE_API
