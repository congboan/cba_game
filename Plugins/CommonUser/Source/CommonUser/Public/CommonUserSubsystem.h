// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserTypes.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/WeakObjectPtr.h"
#include "GameplayTagContainer.h"
#include "CommonUserSubsystem.generated.h"

#if COMMONUSER_OSSV1
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineError.h"
#else
#include "Online/OnlineAsyncOpHandle.h"
#endif

class FNativeGameplayTag;
class IOnlineSubsystem;

/** CommonUser 子系统使用的标签列表 */
struct FCommonUserTags
{
	// 通用严重级别和特定系统消息

	static COMMONUSER_API FNativeGameplayTag SystemMessage_Error;	// 系统消息.错误
	static COMMONUSER_API FNativeGameplayTag SystemMessage_Warning; // 系统消息.警告
	static COMMONUSER_API FNativeGameplayTag SystemMessage_Display; // 系统消息.显示

	/** 所有初始化玩家的尝试都失败了，用户需要做些什么然后才能重试 */
	static COMMONUSER_API FNativeGameplayTag SystemMessage_Error_InitializeLocalPlayerFailed; // 系统消息.错误.初始化本地玩家失败


	// 平台特性标签，预期游戏实例或其他系统会为相应平台调用 SetTraitTags 设置这些标签

	/** 此标签表示这是一个主机平台，将控制器 ID 直接映射到不同的系统用户。如果为 false，同一用户可以有多个控制器 */
	static COMMONUSER_API FNativeGameplayTag Platform_Trait_RequiresStrictControllerMapping; // 平台.特性.需要严格控制器映射

	/** 此标签表示平台只有一个在线用户，所有玩家都使用索引 0 */
	static COMMONUSER_API FNativeGameplayTag Platform_Trait_SingleOnlineUser; // 平台.特性.单一在线用户
};

/** 单个用户的逻辑表示，每个已初始化的本地玩家都会存在一个此类实例 */
UCLASS(MinimalAPI, BlueprintType)
class UCommonUserInfo : public UObject
{
	GENERATED_BODY()

public:
	/** 此用户的主控制器输入设备，还可能有额外的辅助设备 */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	FInputDeviceId PrimaryInputDevice;

	/** 指定本地平台上的逻辑用户，来宾用户将指向主用户 */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	FPlatformUserId PlatformUser;
	
	/** 如果此用户被分配了 LocalPlayer，一旦完全创建完成，此值将匹配 GameInstance localplayers 数组中的索引 */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	int32 LocalPlayerIndex = -1;

	/** 如果为 true，则允许此用户为来宾 */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	bool bCanBeGuest = false;

	/** 如果为 true，则这是附加到主用户 0 的来宾用户 */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	bool bIsGuest = false;

	/** 用户初始化过程的整体状态 */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	ECommonUserInitializationState InitializationState = ECommonUserInitializationState::Invalid;

	/** 如果此用户已成功登录，则返回 true */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API bool IsLoggedIn() const;

	/** 如果此用户正在登录过程中，则返回 true */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API bool IsDoingLogin() const;

	/** 返回最近查询的特定权限结果，如果从未查询则返回 Unknown */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API ECommonUserPrivilegeResult GetCachedPrivilegeResult(ECommonUserPrivilege Privilege, ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 查询某个功能的总体可用性，结合了缓存结果和状态 */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API ECommonUserAvailability GetPrivilegeAvailability(ECommonUserPrivilege Privilege) const;

	/** 获取给定上下文的 Net ID */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API FUniqueNetIdRepl GetNetId(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 获取用户的可读昵称，返回在 UpdateCachedNetId 或 SetNickname 期间缓存的值 */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API FString GetNickname(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 修改用户的可读昵称，可用于设置多个来宾用户，但真实用户的平台昵称会覆盖此值 */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API void SetNickname(const FString& NewNickname, ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game);

	/** 获取此玩家的内部调试字符串 */
	UFUNCTION(BlueprintCallable, Category = UserInfo)
	COMMONUSER_API FString GetDebugString() const;

	/** 平台用户 ID 的访问器 */
	COMMONUSER_API FPlatformUserId GetPlatformUserId() const;

	/** 获取平台用户索引，用于需要整数的旧函数 */
	COMMONUSER_API int32 GetPlatformUserIndex() const;

	// 内部数据，仅供在线子系统访问

	/** 每个在线系统的缓存数据 */
	struct FCachedData
	{
		/** 每个系统的缓存 Net ID */
		FUniqueNetIdRepl CachedNetId;

		/** 缓存的昵称，在 Net ID 可能变化时更新 */
		FString CachedNickname;

		/** 各种用户权限的缓存值 */
		TMap<ECommonUserPrivilege, ECommonUserPrivilegeResult> CachedPrivileges;
	};

	/** 每个上下文的缓存，Game 始终存在但其他可能不存在 */
	TMap<ECommonUserOnlineContext, FCachedData> CachedDataMap;
	
	/** 使用解析规则查找缓存数据 */
	COMMONUSER_API FCachedData* GetCachedData(ECommonUserOnlineContext Context);
	COMMONUSER_API const FCachedData* GetCachedData(ECommonUserOnlineContext Context) const;

	/** 更新缓存的权限结果，如果需要则传播到游戏 */
	COMMONUSER_API void UpdateCachedPrivilegeResult(ECommonUserPrivilege Privilege, ECommonUserPrivilegeResult Result, ECommonUserOnlineContext Context);

	/** 更新缓存的 Net ID，如果需要则传播到游戏 */
	COMMONUSER_API void UpdateCachedNetId(const FUniqueNetIdRepl& NewId, ECommonUserOnlineContext Context);

	/** 返回此对象所属的子系统 */
	COMMONUSER_API class UCommonUserSubsystem* GetSubsystem() const;
};


/** 初始化过程成功或失败时的委托 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FCommonUserOnInitializeCompleteMulticast, const UCommonUserInfo*, UserInfo, bool, bSuccess, FText, Error, ECommonUserPrivilege, RequestedPrivilege, ECommonUserOnlineContext, OnlineContext);
DECLARE_DYNAMIC_DELEGATE_FiveParams(FCommonUserOnInitializeComplete, const UCommonUserInfo*, UserInfo, bool, bSuccess, FText, Error, ECommonUserPrivilege, RequestedPrivilege, ECommonUserOnlineContext, OnlineContext);

/** 当发送系统错误消息时的委托，游戏可以选择使用类型标签向用户显示 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCommonUserHandleSystemMessageDelegate, FGameplayTag, MessageType, FText, TitleText, FText, BodyText);

/** 当权限发生变化时的委托，可绑定以观察游戏过程中在线状态等变化 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FCommonUserAvailabilityChangedDelegate, const UCommonUserInfo*, UserInfo, ECommonUserPrivilege, Privilege, ECommonUserAvailability, OldAvailability, ECommonUserAvailability, NewAvailability);


/** 初始化函数的参数结构体，通常由异步节点等封装函数填充 */
USTRUCT(BlueprintType)
struct FCommonUserInitializeParams
{
	GENERATED_BODY()
	
	/** 要使用的本地玩家索引，如果允许创建玩家则可以使用当前值 +1 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	int32 LocalPlayerIndex = 0;

	/** 选择平台用户和输入设备的已弃用方法 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	int32 ControllerId = -1;

	/** 此用户的主控制器输入设备，还可能有额外的辅助设备 */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	FInputDeviceId PrimaryInputDevice;

	/** 指定本地平台上的逻辑用户 */
	UPROPERTY(BlueprintReadOnly, Category = UserInfo)
	FPlatformUserId PlatformUser;
	
	/** 通常为 CanPlay 或 CanPlayOnline，指定所需的权限级别 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	ECommonUserPrivilege RequestedPrivilege = ECommonUserPrivilege::CanPlay;

	/** 要登录的具体在线上下文，Game 表示登录到所有相关上下文 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	ECommonUserOnlineContext OnlineContext = ECommonUserOnlineContext::Game;

	/** 如果允许为初始登录创建新的本地玩家则为 true */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	bool bCanCreateNewLocalPlayer = false;

	/** 如果此玩家可以是没有实际在线身份的来宾用户则为 true */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	bool bCanUseGuestLogin = false;

	/** 如果不显示登录错误则为 true，游戏将负责显示错误 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	bool bSuppressLoginErrors = false;

	/** 如果绑定，在登录完成时调用此动态委托 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Default)
	FCommonUserOnInitializeComplete OnUserInitializeComplete;
};

/**
 * 游戏子系统，处理用户身份和登录状态的查询与变更。
 * 每个游戏实例创建一个子系统，可从蓝图或 C++ 代码访问。
 * 如果存在游戏特定的子类，则不会创建此基础子系统。
 */
UCLASS(MinimalAPI, BlueprintType, Config=Engine)
class UCommonUserSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UCommonUserSubsystem() { }

	COMMONUSER_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	COMMONUSER_API virtual void Deinitialize() override;
	COMMONUSER_API virtual bool ShouldCreateSubsystem(UObject* Outer) const override;


	/** 当任何请求的初始化操作完成时，BP 委托被调用 */
	UPROPERTY(BlueprintAssignable, Category = CommonUser)
	FCommonUserOnInitializeCompleteMulticast OnUserInitializeComplete;

	/** 当系统发送错误/警告消息时，BP 委托被调用 */
	UPROPERTY(BlueprintAssignable, Category = CommonUser)
	FCommonUserHandleSystemMessageDelegate OnHandleSystemMessage;

	/** 当用户的权限可用性发生变化时，BP 委托被调用 */
	UPROPERTY(BlueprintAssignable, Category = CommonUser)
	FCommonUserAvailabilityChangedDelegate OnUserPrivilegeChanged;

	/** 通过 OnHandleSystemMessage 发送系统消息 */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual void SendSystemMessage(FGameplayTag MessageType, FText TitleText, FText BodyText);

	/** 设置最大本地玩家数，不会销毁现有的 */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual void SetMaxLocalPlayers(int32 InMaxLocalPLayers);

	/** 获取最大本地玩家数 */
	UFUNCTION(BlueprintPure, Category = CommonUser)
	COMMONUSER_API int32 GetMaxLocalPlayers() const;

	/** 获取当前本地玩家数，始终至少为 1 */
	UFUNCTION(BlueprintPure, Category = CommonUser)
	COMMONUSER_API int32 GetNumLocalPlayers() const;

	/** 返回指定本地玩家的初始化状态 */
	UFUNCTION(BlueprintPure, Category = CommonUser)
	COMMONUSER_API ECommonUserInitializationState GetLocalPlayerInitializationState(int32 LocalPlayerIndex) const;

	/** 返回游戏实例中给定本地玩家索引的用户信息，0 在运行游戏中始终有效 */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = CommonUser)
	COMMONUSER_API const UCommonUserInfo* GetUserInfoForLocalPlayerIndex(int32 LocalPlayerIndex) const;

	/** 已弃用，请在有 PlatformUserId 时使用 */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = CommonUser)
	COMMONUSER_API const UCommonUserInfo* GetUserInfoForPlatformUserIndex(int32 PlatformUserIndex) const;

	/** 返回给定平台用户索引的主用户信息。可能返回 null */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = CommonUser)
	COMMONUSER_API const UCommonUserInfo* GetUserInfoForPlatformUser(FPlatformUserId PlatformUser) const;

	/** 返回给定唯一 Net ID 的用户信息。可能返回 null */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = CommonUser)
	COMMONUSER_API const UCommonUserInfo* GetUserInfoForUniqueNetId(const FUniqueNetIdRepl& NetId) const;

	/** 已弃用，请在有 InputDeviceId 时使用 */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = CommonUser)
	COMMONUSER_API const UCommonUserInfo* GetUserInfoForControllerId(int32 ControllerId) const;

	/** 返回给定输入设备的用户信息。可能返回 null */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = CommonUser)
	COMMONUSER_API const UCommonUserInfo* GetUserInfoForInputDevice(FInputDeviceId InputDevice) const;

	/**
	 * 尝试启动创建或更新本地玩家的过程，包括登录和创建 PlayerController。
	 * 当过程成功或失败时，将广播 OnUserInitializeComplete 委托。
	 *
	 * @param LocalPlayerIndex	Game Instance 中 LocalPlayer 的期望索引，0 为主玩家，1+ 为本地多人游戏
	 * @param PrimaryInputDevice 应该映射到此用户的物理控制器，如果无效将使用默认设备
	 * @param bCanUseGuestLogin	如果为 true，此玩家可以是没有真实 Unique Net Id 的来宾
	 *
	 * @returns 如果过程已启动则返回 true，如果在正确启动前失败则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual bool TryToInitializeForLocalPlay(int32 LocalPlayerIndex, FInputDeviceId PrimaryInputDevice, bool bCanUseGuestLogin);

	/**
	 * 启动将本地已登录用户进行完整在线登录（包括账户权限检查）的过程。
	 * 当过程成功或失败时，将广播 OnUserInitializeComplete 委托。
	 *
	 * @param LocalPlayerIndex	Game Instance 中现有 LocalPlayer 的索引
	 *
	 * @returns 如果过程已启动则返回 true，如果在正确启动前失败则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual bool TryToLoginForOnlinePlay(int32 LocalPlayerIndex);

	/**
	 * 启动通用用户登录和初始化过程，使用参数结构体确定要登录的内容。
	 * 当过程成功或失败时，将广播 OnUserInitializeComplete 委托。
	 * AsyncAction_CommonUserInitialize 提供了多个封装函数，用于在事件图表中使用。
	 *
	 * @returns 如果过程已启动则返回 true，如果在正确启动前失败则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual bool TryToInitializeUser(FCommonUserInitializeParams Params);

	/** 
	 * 启动监听新控制器和现有控制器的用户输入及登录过程。
	 * 这将在活动的 GameViewportClient 上插入一个按键输入处理器，通过再次调用并传入空按键数组来关闭。
	 *
	 * @param AnyUserKeys		为任何用户（甚至默认用户）监听这些按键。设置此参数用于初始"按开始"屏幕，或设为空以禁用
	 * @param NewUserKeys		为没有 PlayerController 的新用户监听这些按键。设置此参数用于分屏/本地多人游戏，或设为空以禁用
	 * @param Params			检测到按键输入后传递给 TryToInitializeUser 的参数
	 */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual void ListenForLoginKeyInput(TArray<FKey> AnyUserKeys, TArray<FKey> NewUserKeys, FCommonUserInitializeParams Params);

	/** 尝试取消正在进行的初始化尝试，可能并非在所有平台上有效，但会禁用回调 */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual bool CancelUserInitialization(int32 LocalPlayerIndex);

	/** 将玩家从所有在线系统中登出，如果不是第一个玩家，可选择完全销毁该玩家 */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual bool TryToLogOutUser(int32 LocalPlayerIndex, bool bDestroyPlayer = false);

	/** 在错误后返回主菜单时重置登录和初始化状态 */
	UFUNCTION(BlueprintCallable, Category = CommonUser)
	COMMONUSER_API virtual void ResetUserState();

	/** 如果可能是一个具有有效身份的真实平台用户（即使当前未登录），则返回 true */
	COMMONUSER_API virtual bool IsRealPlatformUserIndex(int32 PlatformUserIndex) const;

	/** 如果可能是一个具有有效身份的真实平台用户（即使当前未登录），则返回 true */
	COMMONUSER_API virtual bool IsRealPlatformUser(FPlatformUserId PlatformUser) const;

	/** 将索引转换为 ID */
	COMMONUSER_API virtual FPlatformUserId GetPlatformUserIdForIndex(int32 PlatformUserIndex) const;

	/** 将 ID 转换为索引 */
	COMMONUSER_API virtual int32 GetPlatformUserIndexForId(FPlatformUserId PlatformUser) const;

	/** 获取输入设备的用户 */
	COMMONUSER_API virtual FPlatformUserId GetPlatformUserIdForInputDevice(FInputDeviceId InputDevice) const;

	/** 获取用户的主输入设备 ID */
	COMMONUSER_API virtual FInputDeviceId GetPrimaryInputDeviceForPlatformUser(FPlatformUserId PlatformUser) const;

	/** 当平台状态或选项改变时，从游戏代码调用以设置缓存的特性标签 */
	COMMONUSER_API virtual void SetTraitTags(const FGameplayTagContainer& InTags);

	/** 获取影响功能可用性的当前标签 */
	const FGameplayTagContainer& GetTraitTags() const { return CachedTraitTags; }

	/** 检查特定平台/特性标签是否启用 */
	UFUNCTION(BlueprintPure, Category=CommonUser)
	bool HasTraitTag(const FGameplayTag TraitTag) const { return CachedTraitTags.HasTag(TraitTag); }

	/** 检查是否应在启动时显示"按开始"/输入确认画面。游戏可直接调用此函数或检查特性标签 */
	UFUNCTION(BlueprintPure, BlueprintPure, Category=CommonUser)
	COMMONUSER_API virtual bool ShouldWaitForStartInput() const;


	// 访问底层在线系统信息的函数

#if COMMONUSER_OSSV1
	/** 返回特定类型的 OSS 接口，如果不存在该类型则返回 null */
	COMMONUSER_API IOnlineSubsystem* GetOnlineSubsystem(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回特定类型的身份接口，如果不存在该类型则返回 null */
	COMMONUSER_API IOnlineIdentity* GetOnlineIdentity(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回 OSS 系统的可读名称 */
	COMMONUSER_API FName GetOnlineSubsystemName(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回当前在线连接状态 */
	COMMONUSER_API EOnlineServerConnectionStatus::Type GetConnectionStatus(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;
#else
	/** 获取服务提供商类型，如果不存在则返回 None。 */
	COMMONUSER_API UE::Online::EOnlineServices GetOnlineServicesProvider(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;
	
	/** 返回特定类型的身份验证接口，如果不存在该类型则返回 null */
	COMMONUSER_API UE::Online::IAuthPtr GetOnlineAuth(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回当前在线连接状态 */
	COMMONUSER_API UE::Online::EOnlineServicesConnectionStatus GetConnectionStatus(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;
#endif

	/** 如果当前连接到后端服务器则返回 true */
	COMMONUSER_API bool HasOnlineConnection(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回玩家在指定在线系统上的当前登录状态，仅对真实平台用户有效 */
	COMMONUSER_API ELoginStatusType GetLocalUserLoginStatus(FPlatformUserId PlatformUser, ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回本地平台用户的唯一 Net ID */
	COMMONUSER_API FUniqueNetIdRepl GetLocalUserNetId(FPlatformUserId PlatformUser, ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 返回本地平台用户的昵称，已缓存在 CommonUser Info 中 */
	COMMONUSER_API FString GetLocalUserNickname(FPlatformUserId PlatformUser, ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;

	/** 将用户 ID 转换为调试字符串 */
	COMMONUSER_API FString PlatformUserIdToString(FPlatformUserId UserId);

	/** 将上下文转换为调试字符串 */
	COMMONUSER_API FString ECommonUserOnlineContextToString(ECommonUserOnlineContext Context);

	/** 返回权限检查的可读描述 */
	COMMONUSER_API virtual FText GetPrivilegeDescription(ECommonUserPrivilege Privilege) const;
	COMMONUSER_API virtual FText GetPrivilegeResultDescription(ECommonUserPrivilegeResult Result) const;

	/** 
	 * 为现有本地用户启动登录过程，如果回调未被调度则返回 false。
	 * 此操作激活低级状态机，不会修改用户信息中的初始化状态。
	 */
	DECLARE_DELEGATE_FiveParams(FOnLocalUserLoginCompleteDelegate, const UCommonUserInfo* /*UserInfo*/, ELoginStatusType /*NewStatus*/, FUniqueNetIdRepl /*NetId*/, const TOptional<FOnlineErrorType>& /*Error*/, ECommonUserOnlineContext /*Type*/);
	COMMONUSER_API virtual bool LoginLocalUser(const UCommonUserInfo* UserInfo, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext Context, FOnLocalUserLoginCompleteDelegate OnComplete);

	/** 将本地玩家分配给特定的本地用户并根据需要调用回调 */
	COMMONUSER_API virtual void SetLocalPlayerUserInfo(ULocalPlayer* LocalPlayer, const UCommonUserInfo* UserInfo);

	/** 将具有默认行为的上下文解析为具体上下文 */
	COMMONUSER_API ECommonUserOnlineContext ResolveOnlineContext(ECommonUserOnlineContext Context) const;

	/** 如果存在独立的平台和服务接口则为 true */
	COMMONUSER_API bool HasSeparatePlatformContext() const;

protected:
	/** 内部结构体，缓存每个在线上下文的状态和指针 */
	struct FOnlineContextCache
	{
#if COMMONUSER_OSSV1
		/** 指向基础子系统的指针，只要游戏实例存在就有效 */
		IOnlineSubsystem* OnlineSubsystem = nullptr;

		/** 缓存的身份系统，始终有效 */
		IOnlineIdentityPtr IdentityInterface;

		/** 上次传入 HandleNetworkConnectionStatusChanged 处理函数的连接状态 */
		EOnlineServerConnectionStatus::Type	CurrentConnectionStatus = EOnlineServerConnectionStatus::Normal;
#else
		/** 在线服务，访问特定服务的入口 */
		UE::Online::IOnlineServicesPtr OnlineServices;
		/** 缓存的身份验证服务 */
		UE::Online::IAuthPtr AuthService;
		/** 登录状态变更事件句柄 */
		UE::Online::FOnlineEventDelegateHandle LoginStatusChangedHandle;
		/** 连接状态变更事件句柄 */
		UE::Online::FOnlineEventDelegateHandle ConnectionStatusChangedHandle;
		/** 上次传入 HandleNetworkConnectionStatusChanged 处理函数的连接状态 */
		UE::Online::EOnlineServicesConnectionStatus CurrentConnectionStatus = UE::Online::EOnlineServicesConnectionStatus::NotConnected;
#endif

		/** 重置状态，清除所有共享指针非常重要 */
		void Reset()
		{
#if COMMONUSER_OSSV1
			OnlineSubsystem = nullptr;
			IdentityInterface.Reset();
			CurrentConnectionStatus = EOnlineServerConnectionStatus::Normal;
#else
			OnlineServices.Reset();
			AuthService.Reset();
			CurrentConnectionStatus = UE::Online::EOnlineServicesConnectionStatus::NotConnected;
#endif
		}
	};

	/** 表示正在进行的登录请求的内部结构体 */
	struct FUserLoginRequest : public TSharedFromThis<FUserLoginRequest>
	{
		FUserLoginRequest(UCommonUserInfo* InUserInfo, ECommonUserPrivilege InPrivilege, ECommonUserOnlineContext InContext, FOnLocalUserLoginCompleteDelegate&& InDelegate)
			: UserInfo(TWeakObjectPtr<UCommonUserInfo>(InUserInfo))
			, DesiredPrivilege(InPrivilege)
			, DesiredContext(InContext)
			, Delegate(MoveTemp(InDelegate))
			{}

		/** 哪个本地用户正在尝试登录 */
		TWeakObjectPtr<UCommonUserInfo> UserInfo;

		/** 登录请求的总体状态，可能来自多个来源 */
		ECommonUserAsyncTaskState OverallLoginState = ECommonUserAsyncTaskState::NotStarted;

		/** 尝试使用平台身份验证的状态。启动时，对于 OSSv1 会立即转为 Failed，因为我们不支持平台身份验证。 */
		ECommonUserAsyncTaskState TransferPlatformAuthState = ECommonUserAsyncTaskState::NotStarted;

		/** 尝试使用 AutoLogin 的状态 */
		ECommonUserAsyncTaskState AutoLoginState = ECommonUserAsyncTaskState::NotStarted;

		/** 尝试使用外部登录 UI 的状态 */
		ECommonUserAsyncTaskState LoginUIState = ECommonUserAsyncTaskState::NotStarted;

		/** 所请求的最终权限 */
		ECommonUserPrivilege DesiredPrivilege = ECommonUserPrivilege::Invalid_Count;

		/** 尝试请求相关权限的状态 */
		ECommonUserAsyncTaskState PrivilegeCheckState = ECommonUserAsyncTaskState::NotStarted;

		/** 要登录的最终上下文 */
		ECommonUserOnlineContext DesiredContext = ECommonUserOnlineContext::Invalid;

		/** 当前正在登录的在线系统 */
		ECommonUserOnlineContext CurrentContext = ECommonUserOnlineContext::Invalid;

		/** 完成时的用户回调 */
		FOnLocalUserLoginCompleteDelegate Delegate;

		/** 最近/最相关的错误，用于向用户显示 */
		TOptional<FOnlineErrorType> Error;
	};


	/** 创建新的用户信息对象 */
	COMMONUSER_API virtual UCommonUserInfo* CreateLocalUserInfo(int32 LocalPlayerIndex);

	/** 用于 const getter 的去 const 封装 */
	FORCEINLINE UCommonUserInfo* ModifyInfo(const UCommonUserInfo* Info) { return const_cast<UCommonUserInfo*>(Info); }

	/** 从 OSS 刷新用户信息 */
	COMMONUSER_API virtual void RefreshLocalUserInfo(UCommonUserInfo* UserInfo);

	/** 可能在权限可用性变化时发送通知，将当前值与缓存的旧值进行比较 */
	COMMONUSER_API virtual void HandleChangedAvailability(UCommonUserInfo* UserInfo, ECommonUserPrivilege Privilege, ECommonUserAvailability OldAvailability);

	/** 更新用户缓存的权限并通知委托 */
	COMMONUSER_API virtual void UpdateUserPrivilegeResult(UCommonUserInfo* UserInfo, ECommonUserPrivilege Privilege, ECommonUserPrivilegeResult Result, ECommonUserOnlineContext Context);

	/** 获取某种在线系统的内部数据，对 Service 可能返回 null */
	COMMONUSER_API const FOnlineContextCache* GetContextCache(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game) const;
	COMMONUSER_API FOnlineContextCache* GetContextCache(ECommonUserOnlineContext Context = ECommonUserOnlineContext::Game);

	/** 在绑定委托之前创建并设置系统对象 */
	COMMONUSER_API virtual void CreateOnlineContexts();
	COMMONUSER_API virtual void DestroyOnlineContexts();

	/** 绑定在线委托 */
	COMMONUSER_API virtual void BindOnlineDelegates();

	/** 强制单个用户登出并反初始化 */
	COMMONUSER_API virtual void LogOutLocalUser(FPlatformUserId PlatformUser);

	/** 执行登录请求的下一步，可能包括完成它。如果已完成则返回 true */
	COMMONUSER_API virtual void ProcessLoginRequest(TSharedRef<FUserLoginRequest> Request);

	/** 在 OSS 上调用登录，使用平台 OSS 的平台身份验证。如果 AutoLogin 已启动则返回 true */
	COMMONUSER_API virtual bool TransferPlatformAuth(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);

	/** 在 OSS 上调用 AutoLogin。如果 AutoLogin 已启动则返回 true。 */
	COMMONUSER_API virtual bool AutoLogin(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);

	/** 在 OSS 上调用 ShowLoginUI。如果 ShowLoginUI 已启动则返回 true。 */
	COMMONUSER_API virtual bool ShowLoginUI(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);

	/** 在 OSS 上调用 QueryUserPrivilege。如果 QueryUserPrivilege 已启动则返回 true。 */
	COMMONUSER_API virtual bool QueryUserPrivilege(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);

	/** OSS 特定函数 */
#if COMMONUSER_OSSV1
	COMMONUSER_API virtual ECommonUserPrivilege ConvertOSSPrivilege(EUserPrivileges::Type Privilege) const;
	COMMONUSER_API virtual EUserPrivileges::Type ConvertOSSPrivilege(ECommonUserPrivilege Privilege) const;
	COMMONUSER_API virtual ECommonUserPrivilegeResult ConvertOSSPrivilegeResult(EUserPrivileges::Type Privilege, uint32 Results) const;

	COMMONUSER_API void BindOnlineDelegatesOSSv1();
	COMMONUSER_API bool AutoLoginOSSv1(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
	COMMONUSER_API bool ShowLoginUIOSSv1(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
	COMMONUSER_API bool QueryUserPrivilegeOSSv1(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
#else
	COMMONUSER_API virtual ECommonUserPrivilege ConvertOnlineServicesPrivilege(UE::Online::EUserPrivileges Privilege) const;
	COMMONUSER_API virtual UE::Online::EUserPrivileges ConvertOnlineServicesPrivilege(ECommonUserPrivilege Privilege) const;
	COMMONUSER_API virtual ECommonUserPrivilegeResult ConvertOnlineServicesPrivilegeResult(UE::Online::EUserPrivileges Privilege, UE::Online::EPrivilegeResults Results) const;

	COMMONUSER_API void BindOnlineDelegatesOSSv2();
	COMMONUSER_API void CacheConnectionStatus(ECommonUserOnlineContext Context);
	COMMONUSER_API bool TransferPlatformAuthOSSv2(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
	COMMONUSER_API bool AutoLoginOSSv2(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
	COMMONUSER_API bool ShowLoginUIOSSv2(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
	COMMONUSER_API bool QueryUserPrivilegeOSSv2(FOnlineContextCache* System, TSharedRef<FUserLoginRequest> Request, FPlatformUserId PlatformUser);
	COMMONUSER_API TSharedPtr<UE::Online::FAccountInfo> GetOnlineServiceAccountInfo(UE::Online::IAuthPtr AuthService, FPlatformUserId InUserId) const;
#endif

	/** OSS 函数的回调 */
#if COMMONUSER_OSSV1
	COMMONUSER_API virtual void HandleIdentityLoginStatusChanged(int32 PlatformUserIndex, ELoginStatus::Type OldStatus, ELoginStatus::Type NewStatus, const FUniqueNetId& NewId, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleUserLoginCompleted(int32 PlatformUserIndex, bool bWasSuccessful, const FUniqueNetId& NetId, const FString& Error, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleControllerPairingChanged(int32 PlatformUserIndex, FControllerPairingChangedUserInfo PreviousUser, FControllerPairingChangedUserInfo NewUser);
	COMMONUSER_API virtual void HandleNetworkConnectionStatusChanged(const FString& ServiceName, EOnlineServerConnectionStatus::Type LastConnectionStatus, EOnlineServerConnectionStatus::Type ConnectionStatus, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleOnLoginUIClosed(TSharedPtr<const FUniqueNetId> LoggedInNetId, const int PlatformUserIndex, const FOnlineError& Error, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleCheckPrivilegesComplete(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, uint32 PrivilegeResults, ECommonUserPrivilege RequestedPrivilege, TWeakObjectPtr<UCommonUserInfo> CommonUserInfo, ECommonUserOnlineContext Context);
#else
	COMMONUSER_API virtual void HandleAuthLoginStatusChanged(const UE::Online::FAuthLoginStatusChanged& EventParameters, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleUserLoginCompletedV2(const UE::Online::TOnlineResult<UE::Online::FAuthLogin>& Result, FPlatformUserId PlatformUser, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleOnLoginUIClosedV2(const UE::Online::TOnlineResult<UE::Online::FExternalUIShowLoginUI>& Result, FPlatformUserId PlatformUser, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleNetworkConnectionStatusChanged(const UE::Online::FConnectionStatusChanged& EventParameters, ECommonUserOnlineContext Context);
	COMMONUSER_API virtual void HandleCheckPrivilegesComplete(const UE::Online::TOnlineResult<UE::Online::FQueryUserPrivilege>& Result, TWeakObjectPtr<UCommonUserInfo> CommonUserInfo, UE::Online::EUserPrivileges DesiredPrivilege, ECommonUserOnlineContext Context);
#endif

	/**
	 * 当输入设备（即手柄）连接或断开时的回调。
	 */
	COMMONUSER_API virtual void HandleInputDeviceConnectionChanged(EInputDeviceConnectionState NewConnectionState, FPlatformUserId PlatformUserId, FInputDeviceId InputDeviceId);

	COMMONUSER_API virtual void HandleLoginForUserInitialize(const UCommonUserInfo* UserInfo, ELoginStatusType NewStatus, FUniqueNetIdRepl NetId, const TOptional<FOnlineErrorType>& Error, ECommonUserOnlineContext Context, FCommonUserInitializeParams Params);
	COMMONUSER_API virtual void HandleUserInitializeFailed(FCommonUserInitializeParams Params, FText Error);
	COMMONUSER_API virtual void HandleUserInitializeSucceeded(FCommonUserInitializeParams Params);

	/** 处理"按开始"/登录逻辑的回调 */
	COMMONUSER_API virtual bool OverrideInputKeyForLogin(FInputKeyEventArgs& EventArgs);


	/** 之前的覆盖处理器，取消时将恢复 */
	FOverrideInputKeyHandler WrappedInputKeyHandler;

	/** 监听任何用户的操作按键列表 */
	TArray<FKey> LoginKeysForAnyUser;

	/** 监听新的未映射用户的操作按键列表 */
	TArray<FKey> LoginKeysForNewUser;

	/** 按键触发登录时使用的参数 */
	FCommonUserInitializeParams ParamsForLoginKey;

	/** 最大本地玩家数 */
	int32 MaxNumberOfLocalPlayers = 0;
	
	/** 如果是专用服务器（不需要 LocalPlayer）则为 true */
	bool bIsDedicatedServer = false;

	/** 当前正在进行的登录请求列表 */
	TArray<TSharedRef<FUserLoginRequest>> ActiveLoginRequests;

	/** 每个本地用户的信息，从本地玩家索引到用户的映射 */
	UPROPERTY()
	TMap<int32, TObjectPtr<UCommonUserInfo>> LocalUserInfos;
	
	/** 缓存的平台/模式特性标签 */
	FGameplayTagContainer CachedTraitTags;

	/** 不要在初始化之外访问此字段 */
	FOnlineContextCache* DefaultContextInternal = nullptr;
	FOnlineContextCache* ServiceContextInternal = nullptr;
	FOnlineContextCache* PlatformContextInternal = nullptr;

	friend UCommonUserInfo;
};
