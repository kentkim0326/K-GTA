// Copyright K-GTA. All Rights Reserved.

#include "GunDayPedestrianSubsystem.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDayCrowdSubsystem.h"
#include "GunDayDebug.h"
#include "GunDayDisputeSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

namespace
{
	/** 걷는 상태를 살피는 간격(초). 반응 속도가 아니라 비용을 위한 값이다. */
	constexpr float WanderUpdateIntervalSeconds = 0.25f;

	/** 시선과 이 이상 같은 방향이면 시야 안으로 본다. 약 60도. */
	constexpr float InViewDotThreshold = 0.5f;

	/** 자리를 찾는 시도 횟수. 못 찾으면 다음 차례에 다시 한다. */
	constexpr int32 MaxSpawnAttempts = 8;
}

UGunDayPedestrianSubsystem* UGunDayPedestrianSubsystem::Get(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetSubsystem<UGunDayPedestrianSubsystem>();
	}

	return nullptr;
}

bool UGunDayPedestrianSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}

	return false;
}

void UGunDayPedestrianSubsystem::Deinitialize()
{
	Pedestrians.Reset();

	Super::Deinitialize();
}

ETickableTickType UGunDayPedestrianSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UGunDayPedestrianSubsystem::IsTickable() const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	return Settings && Settings->bSpawnPedestrians && bPedestriansEnabled;
}

TStatId UGunDayPedestrianSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGunDayPedestrianSubsystem, STATGROUP_Tickables);
}

void UGunDayPedestrianSubsystem::Tick(float DeltaTime)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Settings || !Player)
	{
		return;
	}

	ElapsedSeconds += DeltaTime;
	TimeSinceSpawn += DeltaTime;
	TimeSinceWander += DeltaTime;

	PrunePedestrians(Player->GetActorLocation());

	// 모자라면 한 명씩 채운다. 한꺼번에 생기면 티가 난다.
	if (Pedestrians.Num() < Settings->PedestrianCount && TimeSinceSpawn >= Settings->PedestrianSpawnIntervalSeconds)
	{
		TimeSinceSpawn = 0.0f;
		TrySpawnPedestrian(*Player);
	}

	if (Settings->bDrivePedestrianWander && TimeSinceWander >= WanderUpdateIntervalSeconds)
	{
		TimeSinceWander = 0.0f;
		DriveWander();
	}

	if (GEngine && GunDayDebug::IsHUDEnabled())
	{
		GEngine->AddOnScreenDebugMessage(7708, 1.0f, FColor(170, 190, 220),
			FString::Printf(TEXT("행인 %d / %d"), Pedestrians.Num(), Settings->PedestrianCount));
	}
}

void UGunDayPedestrianSubsystem::PrunePedestrians(const FVector& PlayerLocation)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	const UGunDayDisputeSubsystem* Disputes = GetWorld() ? GetWorld()->GetSubsystem<UGunDayDisputeSubsystem>() : nullptr;
	const float DespawnDistanceSquared = Settings->PedestrianDespawnDistance * Settings->PedestrianDespawnDistance;

	for (int32 Index = Pedestrians.Num() - 1; Index >= 0; --Index)
	{
		APawn* Pawn = Pedestrians[Index].Pawn.Get();

		// 사라졌거나 쓰러졌다. 쓰러진 몸은 킷이 처리하도록 두고 명단에서만 뺀다.
		if (!IsValid(Pawn) || !Pawn->GetController())
		{
			Pedestrians.RemoveAt(Index);
			continue;
		}

		if (FVector::DistSquared(Pawn->GetActorLocation(), PlayerLocation) <= DespawnDistanceSquared)
		{
			continue;
		}

		// 시비 중인 사람은 멀어져도 남겨 둔다. 끝나면 다음 차례에 치운다.
		if (Disputes && Disputes->IsInDispute(Pawn))
		{
			continue;
		}

		Pedestrians.RemoveAt(Index);
		if (AController* Controller = Pawn->GetController())
		{
			Controller->Destroy();
		}
		Pawn->Destroy();
	}
}

bool UGunDayPedestrianSubsystem::TrySpawnPedestrian(const APawn& Player)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	UWorld* World = GetWorld();
	if (!Settings || !World || Settings->CivilianClasses.Num() == 0)
	{
		return false;
	}

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSystem)
	{
		return false;
	}

	UClass* Class = Settings->CivilianClasses[FMath::RandHelper(Settings->CivilianClasses.Num())].LoadSynchronous();
	if (!Class)
	{
		UE_LOG(LogGunDay, Warning, TEXT("행인: Civilian Classes 의 클래스를 불러오지 못했다."));
		return false;
	}

	const FVector PlayerLocation = Player.GetActorLocation();
	const float MinDistanceSquared = Settings->PedestrianMinSpawnDistance * Settings->PedestrianMinSpawnDistance;

	FNavLocation Candidate;
	for (int32 Attempt = 0; Attempt < MaxSpawnAttempts; ++Attempt)
	{
		if (!NavSystem->GetRandomReachablePointInRadius(PlayerLocation, Settings->PedestrianMaxSpawnDistance, Candidate))
		{
			continue;
		}

		if (FVector::DistSquared(Candidate.Location, PlayerLocation) < MinDistanceSquared)
		{
			continue;
		}

		// 눈앞에서 생기면 안 된다. 경찰과 달리 조건을 풀지 않고 다음 차례로 미룬다.
		if (IsPointInView(Candidate.Location))
		{
			continue;
		}

		FVector Location = Candidate.Location;
		if (const ACharacter* Defaults = Cast<ACharacter>(Class->GetDefaultObject()))
		{
			Location.Z += Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

		APawn* Pawn = World->SpawnActor<APawn>(Class, Location, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Params);
		if (!Pawn)
		{
			continue;
		}

		if (!Pawn->GetController())
		{
			Pawn->SpawnDefaultController();
		}

		FGunDayPedestrian Entry;
		Entry.Pawn = Pawn;
		Entry.NextMoveAtSeconds = ElapsedSeconds + FMath::FRandRange(0.0f, Settings->PedestrianIdleMaxSeconds);
		Pedestrians.Add(Entry);

		UE_LOG(LogGunDay, Verbose, TEXT("행인: %s (%d / %d)"), *Pawn->GetName(), Pedestrians.Num(), Settings->PedestrianCount);
		return true;
	}

	return false;
}

void UGunDayPedestrianSubsystem::DriveWander()
{
	const UGunDayCoreSettings* Settings = GetSettings();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Settings || !NavSystem)
	{
		return;
	}

	const UGunDayCrowdSubsystem* Crowd = World->GetSubsystem<UGunDayCrowdSubsystem>();
	const UGunDayDisputeSubsystem* Disputes = World->GetSubsystem<UGunDayDisputeSubsystem>();

	for (FGunDayPedestrian& Entry : Pedestrians)
	{
		APawn* Pawn = Entry.Pawn.Get();
		AAIController* Controller = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
		if (!Controller)
		{
			continue;
		}

		// 달아나는 중이거나 시비 중이면 다른 쪽이 몰고 있다. 끝난 뒤 잠깐 서 있다 다시 걷는다.
		if ((Crowd && Crowd->IsAlerted(Pawn)) || (Disputes && Disputes->IsInDispute(Pawn)))
		{
			Entry.bWasMoving = false;
			Entry.NextMoveAtSeconds = ElapsedSeconds + Settings->PedestrianIdleMaxSeconds;
			continue;
		}

		const bool bMoving = Controller->GetMoveStatus() != EPathFollowingStatus::Idle;
		if (bMoving)
		{
			Entry.bWasMoving = true;
			continue;
		}

		// 막 멈췄다. 잠깐 서 있게 한다.
		if (Entry.bWasMoving)
		{
			Entry.bWasMoving = false;
			Entry.NextMoveAtSeconds = ElapsedSeconds + FMath::FRandRange(Settings->PedestrianIdleMinSeconds, Settings->PedestrianIdleMaxSeconds);
			continue;
		}

		if (ElapsedSeconds < Entry.NextMoveAtSeconds)
		{
			continue;
		}

		FNavLocation Target;
		if (NavSystem->GetRandomReachablePointInRadius(Pawn->GetActorLocation(), Settings->PedestrianWanderRadius, Target))
		{
			Controller->MoveToLocation(Target.Location, 50.0f);
		}
		else
		{
			Entry.NextMoveAtSeconds = ElapsedSeconds + Settings->PedestrianIdleMaxSeconds;
		}
	}
}

bool UGunDayPedestrianSubsystem::IsPointInView(const FVector& Point) const
{
	UWorld* World = GetWorld();
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	if (!World || !Controller)
	{
		return false;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	Controller->GetPlayerViewPoint(EyeLocation, EyeRotation);

	// 사람 머리 높이를 본다.
	const FVector Target = Point + FVector(0.0f, 0.0f, 150.0f);
	if (FVector::DotProduct((Target - EyeLocation).GetSafeNormal(), EyeRotation.Vector()) < InViewDotThreshold)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GunDayPedestrianSight), false);
	Params.AddIgnoredActor(Controller->GetPawn());

	FHitResult Hit;
	return !World->LineTraceSingleByChannel(Hit, EyeLocation, Target, ECC_Visibility, Params);
}

void UGunDayPedestrianSubsystem::ClearPedestrians()
{
	for (const FGunDayPedestrian& Entry : Pedestrians)
	{
		if (APawn* Pawn = Entry.Pawn.Get())
		{
			if (AController* Controller = Pawn->GetController())
			{
				Controller->Destroy();
			}
			Pawn->Destroy();
		}
	}

	Pedestrians.Reset();
}

void UGunDayPedestrianSubsystem::SetPedestriansEnabled(bool bEnabled)
{
	bPedestriansEnabled = bEnabled;

	if (!bPedestriansEnabled)
	{
		ClearPedestrians();
	}
}

const UGunDayCoreSettings* UGunDayPedestrianSubsystem::GetSettings() const
{
	return GetDefault<UGunDayCoreSettings>();
}
