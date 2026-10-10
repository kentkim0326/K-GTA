// Copyright K-GTA. All Rights Reserved.

#include "GunDayCrowdSubsystem.h"

#include "AIController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDayDebug.h"
#include "GunDayPoliceResponseSubsystem.h"
#include "GunDayWantedSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

UGunDayCrowdSubsystem* UGunDayCrowdSubsystem::Get(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetSubsystem<UGunDayCrowdSubsystem>();
	}

	return nullptr;
}

bool UGunDayCrowdSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}

	return false;
}

void UGunDayCrowdSubsystem::Deinitialize()
{
	CalmAll();

	OnCivilianAlerted.Clear();
	OnCivilianReported.Clear();

	Super::Deinitialize();
}

ETickableTickType UGunDayCrowdSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UGunDayCrowdSubsystem::IsTickable() const
{
	return Alerted.Num() > 0;
}

TStatId UGunDayCrowdSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGunDayCrowdSubsystem, STATGROUP_Tickables);
}

void UGunDayCrowdSubsystem::Tick(float DeltaTime)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	UGunDayWantedSubsystem* Wanted = GetWantedSubsystem();
	if (!Settings)
	{
		return;
	}

	ElapsedSeconds += DeltaTime;
	TimeSinceLastGunshot += DeltaTime;

	for (int32 Index = Alerted.Num() - 1; Index >= 0; --Index)
	{
		FGunDayAlertedCivilian& Entry = Alerted[Index];
		APawn* Civilian = Entry.Civilian.Get();

		if (!IsValid(Civilian))
		{
			SetWitnessRegistered(Entry, false);
			Alerted.RemoveAt(Index);
			continue;
		}

		// 달아난 지 얼마쯤 지나면 신고한다.
		if (!Entry.bReported && ElapsedSeconds >= Entry.ReportAtSeconds)
		{
			Entry.bReported = true;

			// 남이 쏜 총성만 들었다면 신고는 해도 플레이어를 쫓지 않는다.
			if (Entry.bHeardPlayer)
			{
				if (Wanted)
				{
					Wanted->AddHeat(Settings->CivilianReportHeat);
				}

				SetWitnessRegistered(Entry, true);
			}

			OnCivilianReported.Broadcast(Civilian);
		}

		// 남의 총성을 듣고 이미 신고한 뒤에 플레이어 총성을 들었다. 목격자로 잡는다.
		if (Entry.bReported && Entry.bHeardPlayer && !Entry.bWitnessRegistered)
		{
			SetWitnessRegistered(Entry, true);
		}

		// 목격 시간이 끝나면 진정한 것으로 본다.
		if (ElapsedSeconds >= Entry.WitnessUntilSeconds)
		{
			SetWitnessRegistered(Entry, false);
			Alerted.RemoveAt(Index);
		}
	}

	if (GEngine && GunDayDebug::IsHUDEnabled() && Alerted.Num() > 0)
	{
		GEngine->AddOnScreenDebugMessage(7706, 1.0f, FColor(200, 200, 120),
			FString::Printf(TEXT("놀란 시민 %d명"), Alerted.Num()));
	}
}

void UGunDayCrowdSubsystem::NotifyGunshot(FVector NoiseLocation, bool bPlayerCaused)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	UWorld* World = GetWorld();
	if (!Settings || !World || !Settings->bEnableCrowdReaction)
	{
		return;
	}

	// 연사 한 번에 같은 시민을 수십 번 놀라게 하지 않는다.
	if (TimeSinceLastGunshot < Settings->CrowdAlertCooldownSeconds)
	{
		return;
	}

	TimeSinceLastGunshot = 0.0f;

	const float RadiusSquared = Settings->GunshotAlertRadius * Settings->GunshotAlertRadius;
	int32 NewlyAlerted = 0;

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		if (!IsValid(Pawn) || !IsCivilian(*Pawn))
		{
			continue;
		}

		if (FVector::DistSquared(Pawn->GetActorLocation(), NoiseLocation) > RadiusSquared)
		{
			continue;
		}

		if (Alerted.Num() >= Settings->MaxAlertedCivilians)
		{
			break;
		}

		// 이미 놀란 시민이면 목격 시간만 늘린다.
		const int32 Existing = Alerted.IndexOfByPredicate([Pawn](const FGunDayAlertedCivilian& Entry)
		{
			return Entry.Civilian.Get() == Pawn;
		});

		if (Existing != INDEX_NONE)
		{
			Alerted[Existing].WitnessUntilSeconds = ElapsedSeconds + Settings->CivilianWitnessSeconds;
			Alerted[Existing].bHeardPlayer |= bPlayerCaused;
			continue;
		}

		FGunDayAlertedCivilian Entry;
		Entry.Civilian = Pawn;
		Entry.bHeardPlayer = bPlayerCaused;
		Entry.ReportAtSeconds = ElapsedSeconds + Settings->CivilianReportDelaySeconds;
		Entry.WitnessUntilSeconds = ElapsedSeconds + Settings->CivilianWitnessSeconds;
		Alerted.Add(Entry);
		++NewlyAlerted;

		if (Settings->bDriveCivilianFlee)
		{
			DriveFlee(*Pawn, NoiseLocation);
		}

		OnCivilianAlerted.Broadcast(Pawn, NoiseLocation);
	}

	if (NewlyAlerted > 0)
	{
		UE_LOG(LogGunDay, Verbose, TEXT("총성: 시민 %d명이 흩어진다."), NewlyAlerted);
	}
}

bool UGunDayCrowdSubsystem::IsAlerted(const APawn* Civilian) const
{
	return Civilian && Alerted.ContainsByPredicate([Civilian](const FGunDayAlertedCivilian& Entry)
	{
		return Entry.Civilian.Get() == Civilian;
	});
}

void UGunDayCrowdSubsystem::CalmAll()
{
	for (FGunDayAlertedCivilian& Entry : Alerted)
	{
		SetWitnessRegistered(Entry, false);
	}

	Alerted.Reset();
}

void UGunDayCrowdSubsystem::DriveFlee(APawn& Civilian, const FVector& NoiseLocation) const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Settings || !NavSystem)
	{
		return;
	}

	AAIController* Controller = Cast<AAIController>(Civilian.GetController());
	if (!Controller)
	{
		return;
	}

	const FVector Away = (Civilian.GetActorLocation() - NoiseLocation).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		return;
	}

	const FVector Desired = Civilian.GetActorLocation() + Away * Settings->CivilianFleeDistance;

	// 원하는 지점이 내비메시 밖일 수 있다. 가까운 통행 가능한 자리로 끌어온다.
	FNavLocation Projected;
	if (!NavSystem->ProjectPointToNavigation(Desired, Projected, FVector(500.0f, 500.0f, 500.0f)))
	{
		return;
	}

	Controller->MoveToLocation(Projected.Location, 100.0f);
}

bool UGunDayCrowdSubsystem::IsCivilian(const APawn& Pawn) const
{
	if (&Pawn == UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return false;
	}

	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return false;
	}

	// 설정에 시민 클래스가 지정돼 있으면 그것만 본다.
	if (Settings->CivilianClasses.Num() > 0)
	{
		for (const TSoftClassPtr<APawn>& CivilianClass : Settings->CivilianClasses)
		{
			if (const UClass* Loaded = CivilianClass.Get())
			{
				if (Pawn.IsA(Loaded))
				{
					return true;
				}
			}
		}

		return false;
	}

	// 지정이 없으면 경찰로 칠 클래스만 빼고 전부 시민으로 본다.
	for (const TSoftClassPtr<AActor>& PoliceClass : Settings->PoliceClasses)
	{
		if (const UClass* Loaded = PoliceClass.Get())
		{
			if (Pawn.IsA(Loaded))
			{
				return false;
			}
		}
	}

	if (const UGunDayPoliceResponseSubsystem* Police = GetWorld() ? GetWorld()->GetSubsystem<UGunDayPoliceResponseSubsystem>() : nullptr)
	{
		FGunDayResponseTier Tier;
		if (Police->GetActiveTier(Tier))
		{
			if (const UClass* ResponderClass = Tier.ResponderClass.Get())
			{
				if (Pawn.IsA(ResponderClass))
				{
					return false;
				}
			}
		}
	}

	return true;
}

void UGunDayCrowdSubsystem::SetWitnessRegistered(FGunDayAlertedCivilian& Entry, bool bRegistered)
{
	if (Entry.bWitnessRegistered == bRegistered)
	{
		return;
	}

	UGunDayWantedSubsystem* Wanted = GetWantedSubsystem();
	if (!Wanted)
	{
		// 수배 시스템이 없으면 등록 상태만 맞춰 둔다. 숫자가 어긋나는 것을 막는다.
		Entry.bWitnessRegistered = false;
		return;
	}

	Entry.bWitnessRegistered = bRegistered;

	if (bRegistered)
	{
		Wanted->AddWitness();
	}
	else
	{
		Wanted->RemoveWitness();
	}
}

const UGunDayCoreSettings* UGunDayCrowdSubsystem::GetSettings() const
{
	return GetDefault<UGunDayCoreSettings>();
}

UGunDayWantedSubsystem* UGunDayCrowdSubsystem::GetWantedSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGunDayWantedSubsystem>() : nullptr;
}
