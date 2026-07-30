// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#if COMMONUSER_OSSV1

// Online Subsystem (OSS v1) includes and forward declares
#include "OnlineSubsystemTypes.h"
class IOnlineSubsystem;
struct FOnlineError;
using FOnlineErrorType = FOnlineError;
using ELoginStatusType = ELoginStatus::Type;

#else

// Online Services (OSS v2) includes and forward declares
#include "Online/Connectivity.h"
#include "Online/OnlineError.h"
namespace UE::Online
{
	enum class ELoginStatus : uint8;
	enum class EPrivilegeResults : uint32;
	enum class EUserPrivileges : uint8;
	using IAuthPtr = TSharedPtr<class IAuth>;
	using IOnlineServicesPtr = TSharedPtr<class IOnlineServices>;
	template <typename OpType>
	class TOnlineResult;
	struct FAuthLogin;
	struct FConnectionStatusChanged;
	struct FExternalUIShowLoginUI;
	struct FAuthLoginStatusChanged;
	struct FQueryUserPrivilege;
	struct FAccountInfo;
}
using FOnlineErrorType = UE::Online::FOnlineError;
using ELoginStatusType = UE::Online::ELoginStatus;

#endif

#include "CommonUserTypes.generated.h"


/** 指定在线查询的运行位置和方式的枚举 */
UENUM(BlueprintType)
enum class ECommonUserOnlineContext : uint8
{
	/** 从游戏代码调用，使用默认系统，但有特殊处理可以合并多个上下文的结果 */
	Game,

	/** 默认的引擎在线系统，始终存在，等同于 Service 或 Platform */
	Default,
	
	/** 显式请求外部服务，可能不存在 */
	Service,

	/** 先查找外部服务，失败后回退到默认 */
	ServiceOrDefault,
	
	/** 显式请求平台系统，可能不存在 */
	Platform,

	/** 先查找平台系统，失败后回退到默认 */
	PlatformOrDefault,

	/** 无效系统 */
	Invalid
};

/** 描述特定用户初始化状态的枚举 */
UENUM(BlueprintType)
enum class ECommonUserInitializationState : uint8
{
	/** 用户尚未开始登录流程 */
	Unknown,

	/** 玩家正在通过本地登录获取用户 ID */
	DoingInitialLogin,

	/** 玩家正在执行网络登录，已完成本地登录 */
	DoingNetworkLogin,

	/** 玩家完全登录失败 */
	FailedtoLogin,

	
	/** 玩家已登录并可以访问在线功能 */
	LoggedInOnline,

	/** 玩家已本地登录（访客或真实用户），但无法执行在线操作 */
	LoggedInLocalOnly,


	/** 无效状态或用户 */
	Invalid,
};

/** 指定用户可用的不同权限和功能的枚举 */
UENUM(BlueprintType)
enum class ECommonUserPrivilege : uint8
{
	/** 用户是否可以进行游戏（在线或离线） */
	CanPlay,

	/** 用户是否可以在线模式游戏 */
	CanPlayOnline,

	/** 用户是否可以使用文字聊天 */
	CanCommunicateViaTextOnline,

	/** 用户是否可以使用语音聊天 */
	CanCommunicateViaVoiceOnline,

	/** 用户是否可以访问其他用户生成的内容 */
	CanUseUserGeneratedContent,

	/** 用户是否可以参与跨平台游戏 */
	CanUseCrossPlay,

	/** 无效权限（也是有效权限的计数） */
	Invalid_Count					UMETA(Hidden)
};

/** 指定功能或权限的总体可用性的枚举，组合了多个来源的信息 */
UENUM(BlueprintType)
enum class ECommonUserAvailability : uint8
{
	/** 状态完全未知，需要查询 */
	Unknown,

	/** 此功能现在完全可用 */
	NowAvailable,

	/** 此功能可能在完成正常登录流程后可用 */
	PossiblyAvailable,

	/** 由于网络连接等原因，此功能当前不可用，但未来可能可用 */
	CurrentlyUnavailable,

	/** 由于硬性账户或平台限制，此功能在此会话的剩余时间内永远不可用 */
	AlwaysUnavailable,

	/** 无效功能 */
	Invalid,
};

/** 指定用户可能或不能使用特定权限的具体原因的枚举 */
UENUM(BlueprintType)
enum class ECommonUserPrivilegeResult : uint8
{
	/** 状态未知，需要查询 */
	Unknown,

	/** 此权限完全可用 */
	Available,

	/** 用户尚未完全登录 */
	UserNotLoggedIn,

	/** 用户未拥有此游戏或内容 */
	LicenseInvalid,

	/** 游戏需要更新或补丁后才能使用 */
	VersionOutdated,

	/** 无网络连接，重新连接后可能解决 */
	NetworkConnectionUnavailable,

	/** 家长控制限制 */
	AgeRestricted,

	/** 账户没有所需的订阅或账户类型 */
	AccountTypeRestricted,

	/** 其他账户/用户限制，如被服务封禁 */
	AccountUseRestricted,

	/** 其他平台特定故障 */
	PlatformFailure,
};

/** 用于跟踪不同异步操作进度的枚举 */
enum class ECommonUserAsyncTaskState : uint8
{
	/** 任务尚未启动 */
	NotStarted,
	/** 任务当前正在处理中 */
	InProgress,
	/** 任务已成功完成 */
	Done,
	/** 任务未能完成 */
	Failed
};

/** 关于在线错误的详细信息，实际上是 FOnlineError 的封装。 */
USTRUCT(BlueprintType)
struct FOnlineResultInformation
{
	GENERATED_BODY()

	/** 操作是否成功。如果成功，此结构体的错误字段将不包含额外信息。 */
	UPROPERTY(BlueprintReadOnly)
	bool bWasSuccessful = true;

	/** 唯一错误 ID。可用于与特定已知错误进行比较。 */
	UPROPERTY(BlueprintReadOnly)
	FString ErrorId;

	/** 向用户显示的错误文本。 */
	UPROPERTY(BlueprintReadOnly)
	FText ErrorText;

	/**
	 * 从 FOnlineErrorType 初始化此结构体
	 * @param InOnlineError 要初始化的在线错误
	 */
	void COMMONUSER_API FromOnlineError(const FOnlineErrorType& InOnlineError);
};
