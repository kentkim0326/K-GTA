// Copyright K-GTA. All Rights Reserved.

#include "GunDayPoliceResponseSubsystem.h"

#include "AI/NavigationSystemBase.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDayDebug.h"
#include "GunDayWantedSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

namespace
{
	/** 자리를 찾을 때 몇 번까지 시도할지. */
	constexpr int32 MaxSpawnAttempts = 12;

	/** 이 값보다 전방에 가까우면 시야 안으로 본다. 1 이 정면, -1 이 정반대. */
	constexpr float InFrontDotThreshold = 0.3f;
}

UGunDayPoliceResponseSubsystem* UGunDayPoliceResponseSubsystem::Get(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetSubsystem<UGunDayPoliceResponseSubsystem>();
	}

	return nullptr;
}

bool UGunDayPoliceResponseSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// 에디터 프리뷰 월드에는 만들지 않는다.
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}

	return false;
}

void UGunDayPoliceResponseSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UGunDayWantedSubsystem* Wanted = GetWantedSubsystem())
	{
		Wanted->OnWantedLevelChanged.AddDynamic(this, &UGunDayPoliceResponseSubsystem::HandleWantedLevelChanged);
		bBoundToWanted = true;

		CachedWantedLevel = Wanted->GetWantedLevel();
	}
	else
	{
		UE_LOG(LogGunDay, Warning, TEXT("경찰 대응: 수배 레벨 서브시스템을 찾지 못했다. 투입이 동작하지 않는다."));
	}
}

void UGunDayPoliceResponseSubsystem::Deinitialize()
{
	if (bBoundToWanted)
	{
		if (UGunDayWantedSubsystem* Wanted = GetWantedSubsystem())
		{
			Wanted->OnWantedLevelChanged.RemoveDynamic(this, &UGunDayPoliceResponseSubsystem::HandleWantedLevelChanged);
		}

		bBoundToWanted = false;
	}

	// 월드가 내려가는 중이라 액터를 건드리지 않는다. 목록만 비우고 목격자 몫을 걷어낸다.
	Responders.Reset();
	SyncWitnessCount();

	OnResponseTierChanged.Clear();
	OnResponderSpawned.Clear();
	OnResponderDismissed.Clear();
	OnResponderLost.Clear();

	Super::Deinitialize();
}

ETickableTickType UGunDayPoliceResponseSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UGunDayPoliceResponseSubsystem::IsTickable() const
{
	if (!bResponseEnabled)
	{
		return false;
	}

	// 수배가 없고 정리할 인원도 없으면 쉰다.
	return CachedWantedLevel > 0 || Responders.Num() > 0;
}

TStatId UGunDayPoliceResponseSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGunDayPoliceResponseSubsystem, STATGROUP_Tickables);
}

void UGunDayPoliceResponseSubsystem::Tick(float DeltaTime)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	FGunDayResponseTier Tier;
	const bool bHasTier = GetActiveTier(Tier);

	PruneResponders(Tier);

	TimeSinceLastSpawn += DeltaTime;

	if (bHasTier && Settings->bSpawnResponders && !Tier.ResponderClass.IsNull())
	{
		const bool bNeedsMore = Responders.Num() < Tier.DesiredCount;
		const bool bIntervalPassed = TimeSinceLastSpawn >= Tier.SpawnIntervalSeconds;

		if (bNeedsMore && bIntervalPassed && TrySpawnResponder(Tier))
		{
			TimeSinceLastSpawn = 0.0f;
		}
	}

	if (GEngine && GunDayDebug::IsHUDEnabled())
	{
		GEngine->AddOnScreenDebugMessage(7704, 1.0f, FColor(120, 190, 240),
			FString::Printf(TEXT("경찰 %d / %d 명"), GetAliveResponderCount(), GetDesiredResponderCount()));
	}
}

void UGunDayPoliceResponseSubsystem::HandleWantedLevelChanged(int32 OldLevel, int32 NewLevel)
{
	CachedWantedLevel = NewLevel;

	// 레벨이 오르면 바로 한 명 나갈 수 있게 간격을 비운다.
	if (NewLevel > OldLevel)
	{
		TimeSinceLastSpawn = 1.0e9f;
	}

	const UGunDayCoreSettings* Settings = GetSettings();
	if (NewLevel == 0 && Settings && Settings->bDismissRespondersOnClear)
	{
		DismissAllResponders(true);
	}

	OnResponseTierChanged.Broadcast(NewLevel, GetDesiredResponderCount());
}

void UGunDayPoliceResponseSubsystem::RegisterResponder(AActor* Responder)
{
	if (!IsValid(Responder))
	{
		return;
	}

	if (Responders.Contains(Responder))
	{
		return;
	}

	AddToRoster(Responder);
}

void UGunDayPoliceResponseSubsystem::UnregisterResponder(AActor* Responder)
{
	const int32 Index = Responders.IndexOfByKey(Responder);
	if (Index == INDEX_NONE)
	{
		return;
	}

	RemoveFromRosterAt(Index);
	OnResponderDismissed.Broadcast(Responder);
}

void UGunDayPoliceResponseSubsystem::AddToRoster(AActor* Responder)
{
	Responders.Add(Responder);
	SyncWitnessCount();
	OnResponderSpawned.Broadcast(Responder);
}

AActor* UGunDayPoliceResponseSubsystem::RemoveFromRosterAt(int32 Index)
{
	AActor* Responder = Responders[Index].Get();
	Responders.RemoveAt(Index);
	SyncWitnessCount();

	return Responder;
}

void UGunDayPoliceResponseSubsystem::SyncWitnessCount()
{
	const UGunDayCoreSettings* Settings = GetSettings();
	UGunDayWantedSubsystem* Wanted = GetWantedSubsystem();
	if (!Settings || !Wanted)
	{
		return;
	}

	// 경찰을 목격자로 치지 않는 설정이면 올려 둔 몫만 걷어낸다.
	const int32 Target = Settings->bRespondersCountAsWitnesses ? Responders.Num() : 0;

	while (RegisteredWitnesses < Target)
	{
		Wanted->AddWitness();
		++RegisteredWitnesses;
	}

	while (RegisteredWitnesses > Target)
	{
		Wanted->RemoveWitness();
		--RegisteredWitnesses;
	}
}

void UGunDayPoliceResponseSubsystem::DismissAllResponders(bool bDestroyActors)
{
	TArray<TWeakObjectPtr<AActor>> Dismissed = MoveTemp(Responders);
	Responders.Reset();
	SyncWitnessCount();

	for (const TWeakObjectPtr<AActor>& Weak : Dismissed)
	{
		AActor* Responder = Weak.Get();
		if (!IsValid(Responder))
		{
			continue;
		}

		OnResponderDismissed.Broadcast(Responder);

		if (bDestroyActors)
		{
			Responder->Destroy();
		}
	}
}

int32 UGunDayPoliceResponseSubsystem::GetAliveResponderCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Weak : Responders)
	{
		if (IsValid(Weak.Get()))
		{
			++Count;
		}
	}

	return Count;
}

int32 UGunDayPoliceResponseSubsystem::GetDesiredResponderCount() const
{
	FGunDayResponseTier Tier;
	return GetActiveTier(Tier) ? Tier.DesiredCount : 0;
}

bool UGunDayPoliceResponseSubsystem::GetActiveTier(FGunDayResponseTier& OutTier) const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings || CachedWantedLevel <= 0)
	{
		return false;
	}

	for (const FGunDayResponseTier& Tier : Settings->ResponseTiers)
	{
		if (Tier.WantedLevel == CachedWantedLevel)
		{
			OutTier = Tier;
			return true;
		}
	}

	return false;
}

void UGunDayPoliceResponseSubsystem::SetResponseEnabled(bool bEnabled)
{
	if (bResponseEnabled == bEnabled)
	{
		return;
	}

	bResponseEnabled = bEnabled;

	if (!bResponseEnabled)
	{
		DismissAllResponders(true);
	}
}

void UGunDayPoliceResponseSubsystem::PruneResponders(const FGunDayResponseTier& Tier)
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bCheckDistance = Player != nullptr && Tier.DespawnDistance > 0.0f;
	const float DespawnDistanceSquared = Tier.DespawnDistance * Tier.DespawnDistance;

	for (int32 Index = Responders.Num() - 1; Index >= 0; --Index)
	{
		AActor* Responder = Responders[Index].Get();

		if (!IsValid(Responder))
		{
			// 이미 죽었거나 사라졌다. 넘길 포인터가 없어 인자 없는 신호를 쓴다.
			RemoveFromRosterAt(Index);
			OnResponderLost.Broadcast();
			continue;
		}

		if (bCheckDistance && FVector::DistSquared(Responder->GetActorLocation(), Player->GetActorLocation()) > DespawnDistanceSquared)
		{
			RemoveFromRosterAt(Index);
			OnResponderDismissed.Broadcast(Responder);
			Responder->Destroy();
		}
	}
}

bool UGunDayPoliceResponseSubsystem::TrySpawnResponder(const FGunDayResponseTier& Tier)
{
	UWorld* World = GetWorld();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!World || !Player)
	{
		return false;
	}

	UClass* ResponderClass = Tier.ResponderClass.LoadSynchronous();
	if (!ResponderClass)
	{
		UE_LOG(LogGunDay, Warning, TEXT("경찰 대응: 레벨 %d 의 ResponderClass 를 불러오지 못했다."), Tier.WantedLevel);
		return false;
	}

	FVector SpawnLocation;
	if (!FindSpawnLocation(Tier, *Player, SpawnLocation))
	{
		return false;
	}

	// 플레이어 쪽을 보고 나타난다.
	const FRotator SpawnRotation = (Player->GetActorLocation() - SpawnLocation).GetSafeNormal().Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Spawned = World->SpawnActor<AActor>(ResponderClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (!Spawned)
	{
		return false;
	}

	AddToRoster(Spawned);

	UE_LOG(LogGunDay, Verbose, TEXT("경찰 투입: %s (%d / %d)"), *Spawned->GetName(), Responders.Num(), Tier.DesiredCount);
	return true;
}

bool UGunDayPoliceResponseSubsystem::FindSpawnLocation(const FGunDayResponseTier& Tier, const AActor& Player, FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!NavSystem)
	{
		UE_LOG(LogGunDay, Warning, TEXT("경찰 대응: 내비게이션 시스템이 없다. 레벨에 NavMeshBoundsVolume 이 있는지 확인할 것."));
		return false;
	}

	const FVector PlayerLocation = Player.GetActorLocation();
	const FVector PlayerForward = Player.GetActorForwardVector();
	const float MinDistanceSquared = Tier.MinSpawnDistance * Tier.MinSpawnDistance;

	FNavLocation Candidate;
	for (int32 Attempt = 0; Attempt < MaxSpawnAttempts; ++Attempt)
	{
		if (!NavSystem->GetRandomReachablePointInRadius(PlayerLocation, Tier.MaxSpawnDistance, Candidate))
		{
			continue;
		}

		const FVector Offset = Candidate.Location - PlayerLocation;
		if (Offset.SizeSquared() < MinDistanceSquared)
		{
			continue;
		}

		// 마지막 몇 번은 시야 조건을 버린다. 좁은 골목에서 영영 못 찾는 것을 막는다.
		const bool bRelaxed = Attempt >= MaxSpawnAttempts - 3;
		if (!bRelaxed && FVector::DotProduct(Offset.GetSafeNormal(), PlayerForward) > InFrontDotThreshold)
		{
			continue;
		}

		OutLocation = Candidate.Location;
		return true;
	}

	return false;
}

const UGunDayCoreSettings* UGunDayPoliceResponseSubsystem::GetSettings() const
{
	return GetDefault<UGunDayCoreSettings>();
}

UGunDayWantedSubsystem* UGunDayPoliceResponseSubsystem::GetWantedSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGunDayWantedSubsystem>() : nullptr;
}
