// Copyright K-GTA. All Rights Reserved.

#include "GunDayDisputeSubsystem.h"

#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDayCrowdSubsystem.h"
#include "GunDayDebug.h"
#include "GunDayDisputeSpot.h"
#include "GunDayPoliceResponseSubsystem.h"
#include "Kismet/GameplayStatics.h"

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
	TArray<AGunDayDisputeSpot*> Candidates;
	for (TActorIterator<AGunDayDisputeSpot> It(World); It; ++It)
	{
		AGunDayDisputeSpot* Spot = *It;
		if (!IsValid(Spot) || !Spot->bEnabled)
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

		const int32 ScenarioIndex = (Spot->ScenarioIndices.Num() > 0)
			? Spot->ScenarioIndices[FMath::RandHelper(Spot->ScenarioIndices.Num())]
			: FMath::RandHelper(FMath::Max(1, Settings->DisputeScenarios.Num()));

		if (BeginDispute(*People[0], *People[1], ScenarioIndex))
		{
			Spot->LastUsedSeconds = ElapsedSeconds;
			return true;
		}

		return false;
	}

	// 시비 지점이 하나도 없으면 플레이어 주변 아무나로 시작한다.
	return Settings->bStartDisputesWithoutSpots && StartDisputeNearPlayer(-1);
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

	const int32 Index = (ScenarioIndex >= 0)
		? ScenarioIndex
		: FMath::RandHelper(FMath::Max(1, Settings->DisputeScenarios.Num()));

	return BeginDispute(*People[0], *People[1], Index);
}

bool UGunDayDisputeSubsystem::BeginDispute(APawn& First, APawn& Second, int32 ScenarioIndex)
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
	Active.Add(Dispute);

	UE_LOG(LogGunDay, Log, TEXT("시비 발생: %s (%s vs %s)"),
		*Scenario->Name, *First.GetName(), *Second.GetName());

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

	// 확률을 못 넘으면 그 자리에서 가라앉는다.
	if (FMath::FRand() > Scenario->EscalateChance)
	{
		Dispute.Stage = EGunDayDisputeStage::Resolved;
		OnDisputeStageChanged.Broadcast(Dispute.First.Get(), Dispute.Second.Get(), Dispute.Stage);
		return;
	}

	Dispute.Stage = (Dispute.Stage == EGunDayDisputeStage::Verbal)
		? EGunDayDisputeStage::Shoving
		: EGunDayDisputeStage::Drawn;

	OnDisputeStageChanged.Broadcast(Dispute.First.Get(), Dispute.Second.Get(), Dispute.Stage);
	SpeakLine(Dispute);
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

	// 먼저 꺼낸 쪽이 먼저 쏜다고 본다. 번갈아 말하다 끝난 쪽이 쏘게 해도 된다.
	UGameplayStatics::ApplyDamage(Victim, Scenario->ShotDamage, Shooter->GetController(), Shooter, nullptr);

	UE_LOG(LogGunDay, Log, TEXT("시비 발포: %s 가 %s 를 쐈다 (%s)"),
		*Shooter->GetName(), *Victim->GetName(), *Scenario->Name);

	OnDisputeShot.Broadcast(Shooter, Victim);

	// 총성에 주변이 흩어진다. 플레이어가 쏜 것이 아니므로 수배는 오르지 않는다.
	if (UGunDayCrowdSubsystem* Crowd = GetWorld() ? GetWorld()->GetSubsystem<UGunDayCrowdSubsystem>() : nullptr)
	{
		Crowd->NotifyGunshot(Shooter->GetActorLocation());
	}
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
