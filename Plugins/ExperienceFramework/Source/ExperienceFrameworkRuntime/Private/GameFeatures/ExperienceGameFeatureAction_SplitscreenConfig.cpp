#include "GameFeatures/ExperienceGameFeatureAction_SplitscreenConfig.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"

TMap<FObjectKey, int32> UExperienceGameFeatureAction_SplitscreenConfig::GlobalDisableVotes;

void UExperienceGameFeatureAction_SplitscreenConfig::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	for (int32 Index = LocalDisableVotes.Num() - 1; Index >= 0; --Index)
	{
		FObjectKey ViewportKey = LocalDisableVotes[Index];
		UGameViewportClient* GameViewportClient = Cast<UGameViewportClient>(ViewportKey.ResolveObjectPtr());
		const FWorldContext* WorldContext = GEngine->GetWorldContextFromGameViewport(GameViewportClient);

		if (GameViewportClient && WorldContext && !Context.ShouldApplyToWorldContext(*WorldContext))
		{
			continue;
		}

		int32& VoteCount = GlobalDisableVotes.FindOrAdd(ViewportKey);
		if (VoteCount <= 1)
		{
			GlobalDisableVotes.Remove(ViewportKey);

			if (GameViewportClient && WorldContext)
			{
				GameViewportClient->SetForceDisableSplitscreen(false);
			}
		}
		else
		{
			--VoteCount;
		}

		LocalDisableVotes.RemoveAt(Index);
	}
}

void UExperienceGameFeatureAction_SplitscreenConfig::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	if (!bDisableSplitscreen)
	{
		return;
	}

	if (UGameInstance* GameInstance = WorldContext.OwningGameInstance)
	{
		if (UGameViewportClient* GameViewportClient = GameInstance->GetGameViewportClient())
		{
			FObjectKey ViewportKey(GameViewportClient);

			LocalDisableVotes.Add(ViewportKey);

			int32& VoteCount = GlobalDisableVotes.FindOrAdd(ViewportKey);
			++VoteCount;
			if (VoteCount == 1)
			{
				GameViewportClient->SetForceDisableSplitscreen(true);
			}
		}
	}
}
