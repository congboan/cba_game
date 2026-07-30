// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "UObject/SoftObjectPtr.h"

#define UE_API ASYNCMIXIN_API

class FAsyncCondition;
class FName;
class UPrimaryDataAsset;
struct FPrimaryAssetId;
struct FStreamableHandle;
template <class TClass> class TSubclassOf;

DECLARE_DELEGATE_OneParam(FStreamableHandleDelegate, TSharedPtr<FStreamableHandle>)

//TODO 我认为需要引入保留策略，预加载的资源会自动保留在内存中直到取消
//     但是如果你只想使用 AsyncLoad 函数预加载单个资源呢？我不想为每个调用引入单独的策略，
//     也不想引入一整套预加载 vs 异步加载的概念，所以更倾向于使用保留策略。
//     它应该是成员变量，在继承 AsyncMixin 时真正分配内存，还是应该作为模板参数？
//enum class EAsyncMixinRetentionPolicy : uint8
//{
//	Default,
//	KeepResidentUntilComplete,  // 完成后保持驻留
//	KeepResidentUntilCancel     // 取消前保持驻留
//};

/**
 * FAsyncMixin 简化了异步加载请求的管理，确保线性请求处理，使编写代码更加容易。使用模式如下：
 *
 * 首先 - 继承自 FAsyncMixin，即使你是一个 UObject，也可以同时继承 FAsyncMixin。
 *
 * 然后 - 你可以按如下方式进行异步加载：
 * 
 * CancelAsyncLoading();			// 某些对象（如在列表中）会被重用，因此取消所有待处理项至关重要，防止未完成的任务继续执行。
 * AsyncLoad(ItemOne, CallbackOne);
 * AsyncLoad(ItemTwo, CallbackTwo);
 * StartAsyncLoading();
 * 
 * 你也可以安全地包含 'this' 作用域，这是 mix-in 的一个好处，所有回调都不会超出宿主 AsyncMixin 派生对象的作用域。
 * 例如：
 * AsyncLoad(SomeSoftObjectPtr, [this, ...]() {
 *    
 * });
 * 
 *
 * 实际执行流程：首先取消任何现有的加载请求，比如一个 Widget 刚被通知要表示新内容时。
 * 然后会加载 ItemOne 和 ItemTwo，*之后*按你请求异步加载的顺序调用回调——
 * 即使 ItemOne 或 ItemTwo 在你请求时已经加载完成。
 *
 * 当所有异步加载请求完成时，OnFinishedLoading 将被调用。
 * 
 * 如果你忘记调用 StartAsyncLoading()，我们会在下一帧自动调用它。但你最好在设置完成后记得调用，
 * 因为可能所有资源都已经加载完毕，这样可以避免一帧恼人的加载指示器闪烁。
 * 
 * 注意：FAsyncMixin 还使 [this] 作为 lambda 捕获输入变得安全，因为它会在宿主类被销毁或你取消所有操作时
 * 处理所有解绑。
 *
 * 注意：FAsyncMixin 不会为你的类增加任何额外内存。一些当前处理异步加载的类内部会分配
 * TSharedPtr<FStreamableHandle> 成员，并倾向于持有 SoftObjectPaths 临时状态。
 * FAsyncMixin 使用一个静态 TMap 在内部完成所有这些工作，因此所有异步请求内存都是临时且稀疏存储的。
 * 
 * 注意：为了调试和理解运行状态，你应该在命令行中添加 -LogCmds="LogAsyncMixin Verbose"。
 */
class FAsyncMixin : public FNoncopyable
{
protected:
	UE_API FAsyncMixin();

public:
	UE_API virtual ~FAsyncMixin();

protected:
	/** 加载开始时调用。 */
	virtual void OnStartedLoading() { }
	/** 所有加载完成时调用。 */
	virtual void OnFinishedLoading() { }

protected:
	/** 异步加载一个 TSoftClassPtr<T>，完成后调用 Callback。 */
	template<typename T = UObject>
	void AsyncLoad(TSoftClassPtr<T> SoftClass, TFunction<void()>&& Callback)
	{
		AsyncLoad(SoftClass.ToSoftObjectPath(), FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/** 异步加载一个 TSoftClassPtr<T>，完成后调用 Callback。 */
	template<typename T = UObject>
	void AsyncLoad(TSoftClassPtr<T> SoftClass, TFunction<void(TSubclassOf<T>)>&& Callback)
	{
		AsyncLoad(SoftClass.ToSoftObjectPath(),
			FSimpleDelegate::CreateLambda([SoftClass, UserCallback = MoveTemp(Callback)]() mutable {
				UserCallback(SoftClass.Get());
			})
		);
	}

	/** 异步加载一个 TSoftClassPtr<T>，完成后调用 Callback。 */
	template<typename T = UObject>
	void AsyncLoad(TSoftClassPtr<T> SoftClass, const FSimpleDelegate& Callback = FSimpleDelegate())
	{
		AsyncLoad(SoftClass.ToSoftObjectPath(), Callback);
	}

	/** 异步加载一个 TSoftObjectPtr<T>，完成后调用 Callback。 */
	template<typename T = UObject>
	void AsyncLoad(TSoftObjectPtr<T> SoftObject, TFunction<void()>&& Callback)
	{
		AsyncLoad(SoftObject.ToSoftObjectPath(), FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/** 异步加载一个 TSoftObjectPtr<T>，完成后调用 Callback。 */
	template<typename T = UObject>
	void AsyncLoad(TSoftObjectPtr<T> SoftObject, TFunction<void(T*)>&& Callback)
	{
		AsyncLoad(SoftObject.ToSoftObjectPath(),
			FSimpleDelegate::CreateLambda([SoftObject, UserCallback = MoveTemp(Callback)]() mutable {
				UserCallback(SoftObject.Get());
			})
		);
	}

	/** 异步加载一个 TSoftObjectPtr<T>，完成后调用 Callback。 */
	template<typename T = UObject>
	void AsyncLoad(TSoftObjectPtr<T> SoftObject, const FSimpleDelegate& Callback = FSimpleDelegate())
	{
		AsyncLoad(SoftObject.ToSoftObjectPath(), Callback);
	}

	/** 异步加载一个 FSoftObjectPath，完成后调用 Callback。 */
	UE_API void AsyncLoad(FSoftObjectPath SoftObjectPath, const FSimpleDelegate& Callback = FSimpleDelegate());

	/** 异步加载一个 FSoftObjectPath 数组，完成后调用 Callback。 */
	void AsyncLoad(const TArray<FSoftObjectPath>& SoftObjectPaths, TFunction<void()>&& Callback)
	{
		AsyncLoad(SoftObjectPaths, FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/** 异步加载一个 FSoftObjectPath 数组，完成后调用 Callback。 */
	UE_API void AsyncLoad(const TArray<FSoftObjectPath>& SoftObjectPaths, const FSimpleDelegate& Callback = FSimpleDelegate());

	/** 给定一个主资源数组，加载这些资源属性中引用的、LoadBundles 数组中指定的所有 Bundle。 */
	template<typename T = UPrimaryDataAsset>
	void AsyncPreloadPrimaryAssetsAndBundles(const TArray<T*>& Assets, const TArray<FName>& LoadBundles, const FSimpleDelegate& Callback = FSimpleDelegate())
	{
		TArray<FPrimaryAssetId> PrimaryAssetIds;
		for (const T* Item : Assets)
		{
			PrimaryAssetIds.Add(Item);
		}

		AsyncPreloadPrimaryAssetsAndBundles(PrimaryAssetIds, LoadBundles, Callback);
	}

	/** 给定一个主资源 ID 数组，加载这些资源属性中引用的、LoadBundles 数组中指定的所有 Bundle。 */
	void AsyncPreloadPrimaryAssetsAndBundles(const TArray<FPrimaryAssetId>& AssetIds, const TArray<FName>& LoadBundles, TFunction<void()>&& Callback)
	{
		AsyncPreloadPrimaryAssetsAndBundles(AssetIds, LoadBundles, FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/** 给定一个主资源 ID 数组，加载这些资源属性中引用的、LoadBundles 数组中指定的所有 Bundle。 */
	UE_API void AsyncPreloadPrimaryAssetsAndBundles(const TArray<FPrimaryAssetId>& AssetIds, const TArray<FName>& LoadBundles, const FSimpleDelegate& Callback = FSimpleDelegate());

	/** 添加一个必须为 true 才能继续执行的未来条件。 */
	UE_API void AsyncCondition(TSharedRef<FAsyncCondition> Condition, const FSimpleDelegate& Callback = FSimpleDelegate());

	/**
	 * 此回调不会加载任何内容，而是插入到回调序列中，以便在异步加载完成时，
	 * 该事件将在序列中的同一位置被调用。如果你不希望某个步骤绑定到特定资源
	 * （某些资源可能是可选的），这非常有用。
	 */
	void AsyncEvent(TFunction<void()>&& Callback)
	{
		AsyncEvent(FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/**
	 * 此回调不会加载任何内容，而是插入到回调序列中，以便在异步加载完成时，
	 * 该事件将在序列中的同一位置被调用。如果你不希望某个步骤绑定到特定资源
	 * （某些资源可能是可选的），这非常有用。
	 */
	UE_API void AsyncEvent(const FSimpleDelegate& Callback);

	/** 启动所有异步加载请求。 */
	UE_API void StartAsyncLoading();

	/** 取消所有待处理的异步加载。 */
	UE_API void CancelAsyncLoading();

	/** 当前是否有异步加载正在进行？ */
	UE_API bool IsAsyncLoadingInProgress() const;

private:
	/**
	 * FLoadingState 实际在 FAsyncMixin 所属的一个大 Map 中分配，使 FAsyncMixin 本身不占用内存，
	 * 仅在需要时动态创建 FLoadingState，不需要时销毁。
	 */
	class FLoadingState : public TSharedFromThis<FLoadingState>
	{
	public:
		FLoadingState(FAsyncMixin& InOwner);
		virtual ~FLoadingState();

		/** 启动异步序列。 */
		void Start();

		/** 取消异步序列。 */
		void CancelAndDestroy();

		void AsyncLoad(FSoftObjectPath SoftObject, const FSimpleDelegate& DelegateToCall);
		void AsyncLoad(const TArray<FSoftObjectPath>& SoftObjectPaths, const FSimpleDelegate& DelegateToCall);
		void AsyncPreloadPrimaryAssetsAndBundles(const TArray<FPrimaryAssetId>& PrimaryAssetIds, const TArray<FName>& LoadBundles, const FSimpleDelegate& DelegateToCall);
		void AsyncCondition(TSharedRef<FAsyncCondition> Condition, const FSimpleDelegate& Callback);
		void AsyncEvent(const FSimpleDelegate& Callback);

		bool IsLoadingComplete() const { return !IsLoadingInProgress(); }
		bool IsLoadingInProgress() const;
		bool IsLoadingInProgressOrPending() const;
		bool IsPendingDestroy() const;

	private:
		void CancelOnly(bool bDestroying);
		void CancelStartTimer();
		void TryScheduleStart();
		void TryCompleteAsyncLoading();
		void CompleteAsyncLoading();

	private:
		void RequestDestroyThisMemory();
		void CancelDestroyThisMemory(bool bDestroying);

		/** 谁拥有此加载状态？我们需要它来回调所属的 mix-in 对象。 */
		FAsyncMixin& OwnerRef;

		/**
		 * 我们是否需要预加载 Bundle？如果没有预加载 Bundle（它们需要保留 StreamingHandle 否则会被销毁），
		 * 那么当所有加载完成后可以安全地销毁 FLoadingState。
		 */
		bool bPreloadedBundles = false;

		class FAsyncStep
		{
		public:
			FAsyncStep(const FSimpleDelegate& InUserCallback);
			FAsyncStep(const FSimpleDelegate& InUserCallback, const TSharedPtr<FStreamableHandle>& InStreamingHandle);
			FAsyncStep(const FSimpleDelegate& InUserCallback, const TSharedPtr<FAsyncCondition>& InCondition);

			~FAsyncStep();

			void ExecuteUserCallback();

			bool IsLoadingInProgress() const
			{
				return !IsComplete();
			}

			bool IsComplete() const;
			void Cancel();

			bool BindCompleteDelegate(const FSimpleDelegate& NewDelegate);
			bool IsCompleteDelegateBound() const;

		private:
			FSimpleDelegate UserCallback;
			bool bIsCompletionDelegateBound = false;

			// 可能的异步对象
			TSharedPtr<FStreamableHandle> StreamingHandle;
			TSharedPtr<FAsyncCondition> Condition;
		};

		bool bHasStarted = false;

		int32 CurrentAsyncStep = 0;
		TArray<TUniquePtr<FAsyncStep>> AsyncSteps;
		TArray<TUniquePtr<FAsyncStep>> AsyncStepsPendingDestruction;

		FTSTicker::FDelegateHandle StartTimerDelegate;
		FTSTicker::FDelegateHandle DestroyMemoryDelegate;
	};

	UE_API const FLoadingState& GetLoadingStateConst() const;
	
	UE_API FLoadingState& GetLoadingState();

	UE_API bool HasLoadingState() const;

	UE_API bool IsLoadingInProgressOrPending() const;

private:
	static UE_API TMap<FAsyncMixin*, TSharedRef<FLoadingState>> Loading;
};

/**
 * 有时 mix-in 模式不适用。也许对象需要管理许多不同的任务，
 * 每个任务都有自己的异步依赖链/作用域。在这些情况下，你可以使用 FAsyncScope。
 * 
 * 此类是一个独立的异步依赖处理器，你可以发起多个加载任务并始终按正确顺序处理它们，
 * 就像将 FAsyncMixin 与你的类组合使用一样。
 */
class FAsyncScope : public FAsyncMixin
{
public:
	using FAsyncMixin::AsyncLoad;

	using FAsyncMixin::AsyncPreloadPrimaryAssetsAndBundles;

	using FAsyncMixin::AsyncCondition;

	using FAsyncMixin::AsyncEvent;

	using FAsyncMixin::CancelAsyncLoading;

	using FAsyncMixin::StartAsyncLoading;

	using FAsyncMixin::IsAsyncLoadingInProgress;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

enum class EAsyncConditionResult : uint8
{
	TryAgain,
	Complete
};

DECLARE_DELEGATE_RetVal(EAsyncConditionResult, FAsyncConditionDelegate);

/**
 * 异步条件允许你自定义暂停异步加载的原因，直到满足某个条件为止。
 */
class FAsyncCondition : public TSharedFromThis<FAsyncCondition>
{
public:
	FAsyncCondition(const FAsyncConditionDelegate& Condition);
	FAsyncCondition(TFunction<EAsyncConditionResult()>&& Condition);
	virtual ~FAsyncCondition();

protected:
	bool IsComplete() const;
	bool BindCompleteDelegate(const FSimpleDelegate& NewDelegate);

private:
	bool TryToContinue(float DeltaTime);

	FTSTicker::FDelegateHandle RepeatHandle;
	FAsyncConditionDelegate UserCondition;
	FSimpleDelegate CompletionDelegate;

	friend FAsyncMixin;
};

#undef UE_API
