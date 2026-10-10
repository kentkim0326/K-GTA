// Copyright K-GTA. All Rights Reserved.

#include "GunDayDisputeSubsystem.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDayCrowdSubsystem.h"
#include "GunDayDebug.h"
#include "GunDayDisputeSpot.h"
#include "GunDayNewsSubsystem.h"
#include "GunDayPoliceResponseSubsystem.h"
#include "GunDaySocietySubsystem.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** 배역 지점을 살피는 간격(초). 반응 속도가 아니라 비용을 위한 값이다. */
	constexpr float CastUpdateIntervalSeconds = 0.5f;

	/** 시선과 이 이상 같은 방향이면 시야 안으로 본다. 약 60도. */
	constexpr float InViewDotThreshold = 0.5f;
}

UGunDayDisputeSubsystem* UGunDayDisputeSubsystem::Get(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetSubsystem<UGunDayDisputeSubsystem>();
	}

	return nullptr;
}

bool UGunDayDisputeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}

	return false;
}

void UGunDayDisputeSubsystem::Deinitialize()
{
	Active.Reset();

	OnDisputeStageChanged.Clear();
	OnDisputeLine.Clear();
	OnDisputeShot.Clear();

	Super::Deinitialize();
}

ETickableTickType UGunDayDisputeSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UGunDayDisputeSubsystem::IsTickable() const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	return Settings && Settings->bEnableDisputes && bDisputesEnabled;
}

TStatId UGunDayDisputeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGunDayDisputeSubsystem, STATGROUP_Tickables);
}

void UGunDayDisputeSubsystem::Tick(float DeltaTime)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	ElapsedSeconds += DeltaTime;
	TimeSinceLastAttempt += DeltaTime;

	for (int32 Index = Active.Num() - 1; Index >= 0; --Index)
	{
		FGunDayActiveDispute& Dispute = Active[Index];

		// 한쪽이 사라졌으면 시비도 끝이다.
		if (!IsValid(Dispute.First.Get()) || !IsValid(Dispute.Second.Get()))
		{
			Active.RemoveAt(Index);
			continue;
		}

		if (ElapsedSeconds >= Dispute.NextStageAtSeconds)
		{
			AdvanceDispute(Dispute);
		}

		if (Dispute.Stage == EGunDayDisputeStage::Resolved)
		{
			Active.RemoveAt(Index);
		}
	}

	// 배역을 세우는 지점은 주기를 기다리지 않는다. 플레이어가 다가오면 바로 시작한다.
	TimeSinceCastUpdate += DeltaTime;
	if (TimeSinceCastUpdate >= CastUpdateIntervalSeconds)
	{
		TimeSinceCastUpdate = 0.0f;
		UpdateCastSpots();
	}

	// 새 시비를 일으킬 때가 됐는지 본다.
	if (TimeSinceLastAttempt >= Settings->DisputeIntervalSeconds && Active.Num() < Settings->MaxActiveDisputes)
	{
		TimeSinceLastAttempt = 0.0f;
		TryStartDisputeAtSpot();
	}

	if (GEngine && GunDayDebug::IsHUDEnabled() && Active.Num() > 0)
	{
		GEngine->AddOnScreenDebugMessage(7707, 1.0f, FColor(230, 150, 90),
			FString::Printf(TEXT("시비 %d건 진행 중"), Active.Num()));
	}
}

bool UGunDayDisputeSubsystem::TryStartDisputeAtSpot()
{
	const UGunDayCoreSettings* Settings = GetSettings();
	UWorld* World = GetWorld();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Settings || !World || !Player)
	{
		return false;
	}

	const float MaxDistanceSquared = Settings->DisputeSearchRadius * Settings->DisputeSearchRadius;

	// 레벨에 놓인 시비 지점부터 본다. 골목마다 다른 상황이 나오는 것은 여기서 갈린다.
	// 배역을 세우는 지점은 UpdateCastSpots 가 따로 돌본다.
	bool bLevelHasSpots = false;
	TArray<AGunDayDisputeSpot*> Candidates;
	for (TActorIterator<AGunDayDisputeSpot> It(World); It; ++It)
	{
		AGunDayDisputeSpot* Spot = *It;
		if (!IsValid(Spot))
		{
			continue;
		}

		bLevelHasSpots = true;

		if (!Spot->bEnabled || UsesOwnCast(*Spot))
		{
			continue;
		}

		if (ElapsedSeconds - Spot->LastUsedSeconds < Spot->CooldownSeconds)
		{
			continue;
		}

		if (FVector::DistSquared(Spot->GetActorLocation(), Player->GetActorLocation()) > MaxDistanceSquared)
		{
			continue;
		}

		Candidates.Add(Spot);
	}

	if (Candidates.Num() > 0)
	{
		AGunDayDisputeSpot* Spot = Candidates[FMath::RandHelper(Candidates.Num())];

		TArray<APawn*> People;
		GatherCandidates(Spot->GetActorLocation(), Spot->Radius, People);
		if (People.Num() < 2)
		{
			return false;
		}

		SortByFriction(People);

		if (BeginDispute(*People[0], *People[1], PickScenarioForSpot(*Spot), Spot))
		{
			Spot->LastUsedSeconds = ElapsedSeconds;
			return true;
		}

		return false;
	}

	// 레벨에 시비 지점이 하나도 없을 때만 플레이어 주변 아무나로 시작한다.
	// 지점이 있는 맵에서 아무나 붙으면 골목마다 정해 둔 상황이 흐려진다.
	return !bLevelHasSpots && Settings->bStartDisputesWithoutSpots && StartDisputeNearPlayer(-1);
}

void UGunDayDisputeSubsystem::UpdateCastSpots()
{
	const UGunDayCoreSettings* Settings = GetSettings();
	UWorld* World = GetWorld();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Settings || !World || !Player || !Settings->bSpawnDisputeCast)
	{
		return;
	}

	const FVector PlayerLocation = Player->GetActorLocation();

	for (TActorIterator<AGunDayDisputeSpot> It(World); It; ++It)
	{
		AGunDayDisputeSpot* Spot = *It;
		if (!IsValid(Spot) || !UsesOwnCast(*Spot))
		{
			continue;
		}

		const float Distance = FVector::Dist(Spot->GetActorLocation(), PlayerLocation);

		// 멀어지면 치운다. 총이 오간 자리도 이때 풀린다.
		if (Distance > Settings->DisputeCastDespawnDistance)
		{
			DismissCast(*Spot);
			continue;
		}

		// 한쪽만 남았으면 이 배역으로는 더 못 한다. 플레이어가 떠나면 새로 세운다.
		if (!Spot->HasCast())
		{
			const bool bNoCastYet = !Spot->CastFirst.IsValid() && !Spot->CastSecond.IsValid();
			if (bNoCastYet && !Spot->bCastSpent
				&& Distance <= Settings->DisputeCastSpawnDistance
				&& !IsSpotInView(*Spot))
			{
				SpawnCast(*Spot);
			}
			continue;
		}

		if (Spot->bCastSpent || Distance > Settings->DisputeCastStartDistance)
		{
			continue;
		}

		if (ElapsedSeconds - Spot->LastUsedSeconds < Spot->CooldownSeconds || Active.Num() >= Settings->MaxActiveDisputes)
		{
			continue;
		}

		APawn* First = Spot->CastFirst.Get();
		APawn* Second = Spot->CastSecond.Get();
		if (IsInDispute(First) || IsInDispute(Second))
		{
			continue;
		}

		// 컨트롤러가 떨어졌으면 쓰러진 것으로 본다. 플레이어가 배역을 쏜 경우다.
		if (!First->GetController() || !Second->GetController())
		{
			Spot->bCastSpent = true;
			continue;
		}

		if (BeginDispute(*First, *Second, Spot->CastScenarioIndex, Spot))
		{
			Spot->LastUsedSeconds = ElapsedSeconds;
		}
	}
}

bool UGunDayDisputeSubsystem::SpawnCast(AGunDayDisputeSpot& Spot)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	UWorld* World = GetWorld();
	if (!Settings || !World)
	{
		return false;
	}

	const int32 ScenarioIndex = PickScenarioForSpot(Spot);
	const FGunDayDisputeScenario* Scenario = GetScenario(ScenarioIndex);
	if (!Scenario)
	{
		UE_LOG(LogGunDay, Warning, TEXT("시비 배역: %s 의 %d번 상황이 설정에 없다."), *Spot.GetName(), ScenarioIndex);
		Spot.bCastSpent = true;
		return false;
	}

	// 상황에 정한 배역이 없으면 시민 목록에서 아무나 데려온다.
	auto ResolveClass = [Settings](const TSoftClassPtr<APawn>& Wanted) -> UClass*
	{
		if (!Wanted.IsNull())
		{
			return Wanted.LoadSynchronous();
		}

		if (Settings->CivilianClasses.Num() > 0)
		{
			return Settings->CivilianClasses[FMath::RandHelper(Settings->CivilianClasses.Num())].LoadSynchronous();
		}

		return nullptr;
	};

	UClass* FirstClass = ResolveClass(Scenario->FirstClass);
	UClass* SecondClass = ResolveClass(Scenario->SecondClass);
	if (!FirstClass || !SecondClass)
	{
		UE_LOG(LogGunDay, Warning, TEXT("시비 배역: '%s' 에 세울 클래스가 없다. 상황의 First/Second Class 나 Civilian Classes 를 채울 것."), *Scenario->Name);
		Spot.bCastSpent = true;
		return false;
	}

	// 지점을 가운데 두고 좌우로 마주 선다.
	const FVector Center = Spot.GetActorLocation();
	const FVector Side = Spot.GetActorRightVector() * (Settings->DisputeCastSpacing * 0.5f);

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	auto SpawnOne = [&](UClass* Class, const FVector& Desired, const FVector& FaceTowards) -> APawn*
	{
		FVector Location = Desired;

		FNavLocation OnNav;
		if (NavSystem && NavSystem->ProjectPointToNavigation(Desired, OnNav, FVector(200.0f, 200.0f, 300.0f)))
		{
			Location = OnNav.Location;
		}

		// 지점은 바닥에 놓으므로 캡슐 반 높이만큼 올려 세운다.
		if (const ACharacter* Defaults = Cast<ACharacter>(Class->GetDefaultObject()))
		{
			Location.Z += Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}

		const FRotator Facing = (FaceTowards - Location).GetSafeNormal2D().Rotation();

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		APawn* Pawn = World->SpawnActor<APawn>(Class, Location, Facing, Params);
		if (Pawn && !Pawn->GetController())
		{
			Pawn->SpawnDefaultController();
		}
		return Pawn;
	};

	APawn* First = SpawnOne(FirstClass, Center - Side, Center + Side);
	APawn* Second = SpawnOne(SecondClass, Center + Side, Center - Side);
	if (!First || !Second)
	{
		if (First) { First->Destroy(); }
		if (Second) { Second->Destroy(); }
		return false;
	}

	Spot.CastFirst = First;
	Spot.CastSecond = Second;
	Spot.CastScenarioIndex = ScenarioIndex;

	UE_LOG(LogGunDay, Log, TEXT("시비 배역: %s 에 '%s' 배역을 세웠다 (%s, %s)"),
		*Spot.GetName(), *Scenario->Name, *First->GetName(), *Second->GetName());
	return true;
}

void UGunDayDisputeSubsystem::DismissCast(AGunDayDisputeSpot& Spot)
{
	APawn* First = Spot.CastFirst.Get();
	APawn* Second = Spot.CastSecond.Get();

	if (IsInDispute(First) || IsInDispute(Second))
	{
		return;
	}

	const bool bHadAnyone = First || Second || Spot.bCastSpent;

	for (APawn* Pawn : { First, Second })
	{
		if (IsValid(Pawn))
		{
			if (AController* Controller = Pawn->GetController())
			{
				Controller->Destroy();
			}
			Pawn->Destroy();
		}
	}

	Spot.CastFirst.Reset();
	Spot.CastSecond.Reset();
	Spot.CastScenarioIndex = INDEX_NONE;
	Spot.bCastSpent = false;

	if (bHadAnyone)
	{
		UE_LOG(LogGunDay, Verbose, TEXT("시비 배역: %s 의 배역을 치웠다"), *Spot.GetName());
	}
}

bool UGunDayDisputeSubsystem::IsSpotInView(const AGunDayDisputeSpot& Spot) const
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

	// 사람 머리 높이를 본다. 바닥만 가려져 있으면 보이는 것으로 친다.
	const FVector Target = Spot.GetActorLocation() + FVector(0.0f, 0.0f, 150.0f);
	const FVector ToTarget = (Target - EyeLocation).GetSafeNormal();
	if (FVector::DotProduct(ToTarget, EyeRotation.Vector()) < InViewDotThreshold)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GunDayDisputeCastSight), false);
	Params.AddIgnoredActor(Controller->GetPawn());

	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, EyeLocation, Target, ECC_Visibility, Params);
	return !bBlocked;
}

bool UGunDayDisputeSubsystem::UsesOwnCast(const AGunDayDisputeSpot& Spot) const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	return Settings && Settings->bSpawnDisputeCast && Spot.bEnabled && Spot.bBringOwnCast;
}

int32 UGunDayDisputeSubsystem::PickScenarioForSpot(const AGunDayDisputeSpot& Spot) const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (Spot.ScenarioIndices.Num() > 0)
	{
		return Spot.ScenarioIndices[FMath::RandHelper(Spot.ScenarioIndices.Num())];
	}

	return FMath::RandHelper(FMath::Max(1, Settings ? Settings->DisputeScenarios.Num() : 1));
}

bool UGunDayDisputeSubsystem::StartDisputeNearPlayer(int32 ScenarioIndex)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Settings || !Player)
	{
		return false;
	}

	TArray<APawn*> People;
	GatherCandidates(Player->GetActorLocation(), Settings->DisputeSearchRadius, People);
	if (People.Num() < 2)
	{
		UE_LOG(LogGunDay, Warning, TEXT("시비: 주변에 시민이 둘 이상 없다."));
		return false;
	}

	SortByFriction(People);

	const int32 Index = (ScenarioIndex >= 0)
		? ScenarioIndex
		: FMath::RandHelper(FMath::Max(1, Settings->DisputeScenarios.Num()));

	return BeginDispute(*People[0], *People[1], Index);
}

bool UGunDayDisputeSubsystem::BeginDispute(APawn& First, APawn& Second, int32 ScenarioIndex, AGunDayDisputeSpot* Spot)
{
	const FGunDayDisputeScenario* Scenario = GetScenario(ScenarioIndex);
	if (!Scenario)
	{
		UE_LOG(LogGunDay, Warning, TEXT("시비: %d번 상황이 설정에 없다."), ScenarioIndex);
		return false;
	}

	// 서로 마주 본 채 멈춘다.
	for (APawn* Pawn : { &First, &Second })
	{
		if (AAIController* Controller = Cast<AAIController>(Pawn->GetController()))
		{
			Controller->StopMovement();
		}
	}

	const FVector ToSecond = (Second.GetActorLocation() - First.GetActorLocation()).GetSafeNormal2D();
	if (!ToSecond.IsNearlyZero())
	{
		First.SetActorRotation(ToSecond.Rotation());
		Second.SetActorRotation((-ToSecond).Rotation());
	}

	FGunDayActiveDispute Dispute;
	Dispute.First = &First;
	Dispute.Second = &Second;
	Dispute.ScenarioIndex = ScenarioIndex;
	Dispute.Stage = EGunDayDisputeStage::Verbal;
	Dispute.NextStageAtSeconds = ElapsedSeconds + Scenario->StageSeconds;
	Dispute.Spot = Spot;

	if (UGunDaySocietySubsystem* Society = GetWorld() ? GetWorld()->GetSubsystem<UGunDaySocietySubsystem>() : nullptr)
	{
		Dispute.Friction = Society->GetFriction(&First, &Second);
	}

	Active.Add(Dispute);

	if (UGunDayNewsSubsystem* News = GetWorld() ? GetWorld()->GetSubsystem<UGunDayNewsSubsystem>() : nullptr)
	{
		News->ReportDispute();
	}

	UE_LOG(LogGunDay, Log, TEXT("시비 발생: %s (%s vs %s, 마찰 %.2f)"),
		*Scenario->Name, *First.GetName(), *Second.GetName(), Dispute.Friction);

	OnDisputeStageChanged.Broadcast(&First, &Second, EGunDayDisputeStage::Verbal);
	SpeakLine(Active.Last());

	return true;
}

void UGunDayDisputeSubsystem::AdvanceDispute(FGunDayActiveDispute& Dispute)
{
	const FGunDayDisputeScenario* Scenario = GetScenario(Dispute.ScenarioIndex);
	if (!Scenario)
	{
		Dispute.Stage = EGunDayDisputeStage::Resolved;
		return;
	}

	Dispute.NextStageAtSeconds = ElapsedSeconds + Scenario->StageSeconds;

	// 발포 단계면 쏘고 끝낸다.
	if (Dispute.Stage == EGunDayDisputeStage::Drawn)
	{
		if (FMath::FRand() <= Scenario->FireChance)
		{
			Dispute.Stage = EGunDayDisputeStage::Shooting;
			FireShot(Dispute);
		}
		else
		{
			// 꺼냈다가 집어넣는다. 이쪽이 더 무서울 때가 있다.
			UE_LOG(LogGunDay, Log, TEXT("시비: 총을 꺼냈다가 물러섰다."));
			Dispute.Stage = EGunDayDisputeStage::Resolved;
		}

		OnDisputeStageChanged.Broadcast(Dispute.First.Get(), Dispute.Second.Get(), Dispute.Stage);
		return;
	}

	if (Dispute.Stage == EGunDayDisputeStage::Shooting)
	{
		Dispute.Stage = EGunDayDisputeStage::Resolved;
		return;
	}

	UGunDaySocietySubsystem* Society = GetWorld() ? GetWorld()->GetSubsystem<UGunDaySocietySubsystem>() : nullptr;
	const UGunDayCoreSettings* Settings = GetSettings();

	// 상대가 잘못을 인정하면 멎는다. 대기형 시비가 멎는 거의 유일한 길이다.
	if (Scenario->ApologyChance > 0.0f && FMath::FRand() <= Scenario->ApologyChance)
	{
		if (Scenario->ApologyLines.Num() > 0)
		{
			APawn* Speaker = Dispute.Second.Get();
			if (IsValid(Speaker))
			{
				const FString& Line = Scenario->ApologyLines[FMath::RandHelper(Scenario->ApologyLines.Num())];
				OnDisputeLine.Broadcast(Speaker, Line, Dispute.Stage);

				if (GunDayDebug::IsHUDEnabled())
				{
					if (UWorld* World = GetWorld())
					{
						DrawDebugString(World, FVector(0.0f, 0.0f, 120.0f), Line, Speaker,
							FColor(150, 210, 150), 3.0f, true);
					}
				}
			}
		}

		if (Society && Settings)
		{
			Society->AddJeong(Settings->JeongOnMediationSuccess);
		}

		UE_LOG(LogGunDay, Log, TEXT("시비: 상대가 인정해서 가라앉았다."));

		Dispute.Stage = EGunDayDisputeStage::Resolved;
		OnDisputeStageChanged.Broadcast(Dispute.First.Get(), Dispute.Second.Get(), Dispute.Stage);
		return;
	}

	// 말리는 사람이 붙어 있으면 가라앉을 기회가 한 번 더 있다.
	if (Dispute.Stage == EGunDayDisputeStage::Shoving && IsValid(Dispute.Mediator.Get()) && Society)
	{
		if (FMath::FRand() <= Society->GetJeongFraction() * Scenario->MediationEffectiveness)
		{
			Society->AddJeong(Settings ? Settings->JeongOnMediationSuccess : 0.0f);
			UE_LOG(LogGunDay, Log, TEXT("시비: 누가 말려서 가라앉았다."));

			Dispute.Stage = EGunDayDisputeStage::Resolved;
			OnDisputeStageChanged.Broadcast(Dispute.First.Get(), Dispute.Second.Get(), Dispute.Stage);
			return;
		}
	}

	// 진영이 다를수록 끝까지 가고, 정이 높을수록 가라앉는다.
	float EscalateChance = Scenario->EscalateChance;
	if (Settings)
	{
		EscalateChance *= (1.0f + Settings->FrictionEscalationWeight * Dispute.Friction * Scenario->FrictionInfluence);

		if (Society)
		{
			EscalateChance *= FMath::Lerp(1.0f, Settings->JeongCalmFactor, Society->GetJeongFraction());
		}
	}

	if (FMath::FRand() > FMath::Clamp(EscalateChance, 0.0f, 1.0f))
	{
		UE_LOG(LogGunDay, Log, TEXT("시비: 더 가지 않고 가라앉았다 (%s)."), *Scenario->Name);
		Dispute.Stage = EGunDayDisputeStage::Resolved;
		OnDisputeStageChanged.Broadcast(Dispute.First.Get(), Dispute.Second.Get(), Dispute.Stage);
		return;
	}

	const bool bWasVerbal = (Dispute.Stage == EGunDayDisputeStage::Verbal);
	Dispute.Stage = bWasVerbal ? EGunDayDisputeStage::Shoving : EGunDayDisputeStage::Drawn;
	UE_LOG(LogGunDay, Log, TEXT("시비: %s (%s)."), bWasVerbal ? TEXT("몸싸움이 됐다") : TEXT("총을 꺼냈다"), *Scenario->Name);

	OnDisputeStageChanged.Broadcast(Dispute.First.Get(), Dispute.Second.Get(), Dispute.Stage);
	SpeakLine(Dispute);

	// 몸싸움이 되면 누가 말리러 나설 수 있다.
	if (bWasVerbal)
	{
		TryMediation(Dispute);
	}
}

void UGunDayDisputeSubsystem::SpeakLine(FGunDayActiveDispute& Dispute)
{
	const FGunDayDisputeScenario* Scenario = GetScenario(Dispute.ScenarioIndex);
	if (!Scenario)
	{
		return;
	}

	const TArray<FString>* Lines = nullptr;
	switch (Dispute.Stage)
	{
	case EGunDayDisputeStage::Verbal:  Lines = &Scenario->VerbalLines; break;
	case EGunDayDisputeStage::Shoving: Lines = &Scenario->ShovingLines; break;
	case EGunDayDisputeStage::Drawn:   Lines = &Scenario->DrawnLines; break;
	default: return;
	}

	if (!Lines || Lines->Num() == 0)
	{
		return;
	}

	APawn* Speaker = Dispute.bFirstSpeaks ? Dispute.First.Get() : Dispute.Second.Get();
	Dispute.bFirstSpeaks = !Dispute.bFirstSpeaks;

	if (!IsValid(Speaker))
	{
		return;
	}

	const FString& Line = (*Lines)[FMath::RandHelper(Lines->Num())];
	OnDisputeLine.Broadcast(Speaker, Line, Dispute.Stage);

	// 자막 시스템이 붙기 전까지는 머리 위에 띄워 둔다.
	if (GunDayDebug::IsHUDEnabled())
	{
		if (UWorld* World = GetWorld())
		{
			DrawDebugString(World, FVector(0.0f, 0.0f, 120.0f), Line, Speaker,
				FColor(240, 220, 120), 3.0f, true);
		}
	}
}

void UGunDayDisputeSubsystem::FireShot(FGunDayActiveDispute& Dispute)
{
	const FGunDayDisputeScenario* Scenario = GetScenario(Dispute.ScenarioIndex);
	APawn* Shooter = Dispute.First.Get();
	APawn* Victim = Dispute.Second.Get();
	if (!Scenario || !IsValid(Shooter) || !IsValid(Victim))
	{
		return;
	}

	const UGunDayCoreSettings* Settings = GetSettings();
	UGunDaySocietySubsystem* Society = GetWorld() ? GetWorld()->GetSubsystem<UGunDaySocietySubsystem>() : nullptr;

	// 말리던 사람이 대신 맞기도 한다. 선의는 가끔 처벌받는다.
	APawn* Mediator = Dispute.Mediator.Get();
	if (Settings && IsValid(Mediator) && FMath::FRand() <= Settings->MediatorShotChance)
	{
		Victim = Mediator;
		UE_LOG(LogGunDay, Log, TEXT("시비: 말리던 사람이 맞았다."));
	}

	// 먼저 꺼낸 쪽이 먼저 쏜다고 본다. 번갈아 말하다 끝난 쪽이 쏘게 해도 된다.
	UGameplayStatics::ApplyDamage(Victim, Scenario->ShotDamage, Shooter->GetController(), Shooter, nullptr);

	// 총이 오간 자리는 같은 배역으로 다시 시작하지 않는다.
	if (AGunDayDisputeSpot* Spot = Dispute.Spot.Get())
	{
		Spot->bCastSpent = true;
	}

	if (Society && Settings)
	{
		Society->AddJeong(Settings->JeongOnDisputeShot);
	}

	if (UGunDayNewsSubsystem* News = GetWorld() ? GetWorld()->GetSubsystem<UGunDayNewsSubsystem>() : nullptr)
	{
		// 피해량이 충분하면 사망으로 친다. 킷의 체력 수치와 맞춰 둔다.
		News->ReportShooting(Scenario->ShotDamage >= 100.0f, false);
	}

	UE_LOG(LogGunDay, Log, TEXT("시비 발포: %s 가 %s 를 쐈다 (%s)"),
		*Shooter->GetName(), *Victim->GetName(), *Scenario->Name);

	OnDisputeShot.Broadcast(Shooter, Victim);

	// 총성에 주변이 흩어진다. 플레이어가 쏜 것이 아니므로 수배는 오르지 않는다.
	if (UGunDayCrowdSubsystem* Crowd = GetWorld() ? GetWorld()->GetSubsystem<UGunDayCrowdSubsystem>() : nullptr)
	{
		Crowd->NotifyGunshot(Shooter->GetActorLocation());
	}
}

void UGunDayDisputeSubsystem::TryMediation(FGunDayActiveDispute& Dispute)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	UGunDaySocietySubsystem* Society = GetWorld() ? GetWorld()->GetSubsystem<UGunDaySocietySubsystem>() : nullptr;
	APawn* First = Dispute.First.Get();
	if (!Settings || !Society || Dispute.bMediationTried || !IsValid(First))
	{
		return;
	}

	Dispute.bMediationTried = true;

	// 정이 낮으면 아무도 나서지 않는다. 그게 이 사회의 상태다.
	const float Chance = Settings->MediationChanceAtFullJeong * Society->GetJeongFraction();
	if (FMath::FRand() > Chance)
	{
		return;
	}

	TArray<APawn*> Nearby;
	GatherCandidates(First->GetActorLocation(), Settings->DisputeSearchRadius * 0.3f, Nearby);
	if (Nearby.Num() == 0)
	{
		return;
	}

	APawn* Mediator = Nearby[0];
	Dispute.Mediator = Mediator;

	if (AAIController* Controller = Cast<AAIController>(Mediator->GetController()))
	{
		Controller->MoveToActor(First, 150.0f);
	}

	if (Settings->MediationLines.Num() > 0)
	{
		const FString& Line = Settings->MediationLines[FMath::RandHelper(Settings->MediationLines.Num())];
		OnDisputeLine.Broadcast(Mediator, Line, Dispute.Stage);

		if (GunDayDebug::IsHUDEnabled())
		{
			if (UWorld* World = GetWorld())
			{
				DrawDebugString(World, FVector(0.0f, 0.0f, 120.0f), Line, Mediator,
					FColor(150, 210, 150), 3.0f, true);
			}
		}
	}
}

void UGunDayDisputeSubsystem::SortByFriction(TArray<APawn*>& People) const
{
	UGunDaySocietySubsystem* Society = GetWorld() ? GetWorld()->GetSubsystem<UGunDaySocietySubsystem>() : nullptr;
	if (!Society || People.Num() < 3)
	{
		return;
	}

	// 첫 사람과 가장 세게 부딪히는 상대를 두 번째 자리로 끌어온다.
	APawn* First = People[0];
	int32 BestIndex = 1;
	float BestFriction = -1.0f;

	for (int32 Index = 1; Index < People.Num(); ++Index)
	{
		const float Friction = Society->GetFriction(First, People[Index]);
		if (Friction > BestFriction)
		{
			BestFriction = Friction;
			BestIndex = Index;
		}
	}

	People.Swap(1, BestIndex);
}

void UGunDayDisputeSubsystem::GatherCandidates(const FVector& Center, float Radius, TArray<APawn*>& OutPawns) const
{
	UWorld* World = GetWorld();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!World)
	{
		return;
	}

	const float RadiusSquared = Radius * Radius;

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		if (!IsValid(Pawn) || Pawn == Player || IsInDispute(Pawn))
		{
			continue;
		}

		if (FVector::DistSquared(Pawn->GetActorLocation(), Center) > RadiusSquared)
		{
			continue;
		}

		// 경찰은 시비에 끌어들이지 않는다.
		if (const UGunDayPoliceResponseSubsystem* Police = World->GetSubsystem<UGunDayPoliceResponseSubsystem>())
		{
			FGunDayResponseTier Tier;
			if (Police->GetActiveTier(Tier))
			{
				if (const UClass* ResponderClass = Tier.ResponderClass.Get())
				{
					if (Pawn->IsA(ResponderClass))
					{
						continue;
					}
				}
			}
		}

		OutPawns.Add(Pawn);
	}

	// 가까운 둘이 붙는 편이 자연스럽다.
	OutPawns.Sort([&Center](const APawn& A, const APawn& B)
	{
		return FVector::DistSquared(A.GetActorLocation(), Center) < FVector::DistSquared(B.GetActorLocation(), Center);
	});
}

bool UGunDayDisputeSubsystem::IsInDispute(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}

	for (const FGunDayActiveDispute& Dispute : Active)
	{
		if (Dispute.First.Get() == Pawn || Dispute.Second.Get() == Pawn)
		{
			return true;
		}
	}

	return false;
}

void UGunDayDisputeSubsystem::ClearDisputes()
{
	Active.Reset();
}

void UGunDayDisputeSubsystem::SetDisputesEnabled(bool bEnabled)
{
	bDisputesEnabled = bEnabled;

	if (!bDisputesEnabled)
	{
		ClearDisputes();
	}
}

const FGunDayDisputeScenario* UGunDayDisputeSubsystem::GetScenario(int32 Index) const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings || !Settings->DisputeScenarios.IsValidIndex(Index))
	{
		return nullptr;
	}

	return &Settings->DisputeScenarios[Index];
}

const UGunDayCoreSettings* UGunDayDisputeSubsystem::GetSettings() const
{
	return GetDefault<UGunDayCoreSettings>();
}
