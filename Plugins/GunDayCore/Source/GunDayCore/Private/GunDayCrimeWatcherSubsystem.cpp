// Copyright K-GTA. All Rights Reserved.

#include "GunDayCrimeWatcherSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDayCrowdSubsystem.h"
#include "GunDayDebug.h"
#include "GunDayPoliceResponseSubsystem.h"
#include "GunDayWantedSubsystem.h"
#include "Kismet/GameplayStatics.h"

UGunDayCrimeWatcherSubsystem* UGunDayCrimeWatcherSubsystem::Get(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetSubsystem<UGunDayCrimeWatcherSubsystem>();
	}

	return nullptr;
}

bool UGunDayCrimeWatcherSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}

	return false;
}

void UGunDayCrimeWatcherSubsystem::Deinitialize()
{
	WatchedPawns.Reset();
	WoundedByPlayer.Reset();
	LastInjuryReportTime.Reset();

	Super::Deinitialize();
}

ETickableTickType UGunDayCrimeWatcherSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UGunDayCrimeWatcherSubsystem::IsTickable() const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	return bWatchEnabled && Settings && Settings->bAutoReportCrimes;
}

TStatId UGunDayCrimeWatcherSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGunDayCrimeWatcherSubsystem, STATGROUP_Tickables);
}

void UGunDayCrimeWatcherSubsystem::Tick(float DeltaTime)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	ElapsedSeconds += DeltaTime;
	TimeSinceRescan += DeltaTime;
	TimeSinceGunfireReport += DeltaTime;

	if (TimeSinceRescan >= Settings->PawnRescanIntervalSeconds)
	{
		TimeSinceRescan = 0.0f;
		RefreshWatchedPawns();
	}

	if (GEngine && GunDayDebug::IsHUDEnabled())
	{
		GEngine->AddOnScreenDebugMessage(7705, 1.0f, FColor(160, 160, 160),
			FString::Printf(TEXT("감시 중인 폰 %d"), WatchedPawns.Num()));
	}
}

void UGunDayCrimeWatcherSubsystem::RefreshWatchedPawns()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	// 사라진 폰은 목록에서 걷어낸다.
	for (auto It = WatchedPawns.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		if (!IsValid(Pawn) || Pawn == PlayerPawn)
		{
			continue;
		}

		if (WatchedPawns.Contains(Pawn))
		{
			continue;
		}

		Pawn->OnTakeAnyDamage.AddDynamic(this, &UGunDayCrimeWatcherSubsystem::HandlePawnDamaged);
		Pawn->OnDestroyed.AddDynamic(this, &UGunDayCrimeWatcherSubsystem::HandlePawnDestroyed);
		WatchedPawns.Add(Pawn);
	}
}

void UGunDayCrimeWatcherSubsystem::HandlePawnDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	UGunDayWantedSubsystem* Wanted = GetWantedSubsystem();
	if (!Settings || !Wanted || !bWatchEnabled || !Settings->bAutoReportCrimes)
	{
		return;
	}

	if (!IsValid(DamagedActor) || Damage <= 0.0f)
	{
		return;
	}

	if (!IsPlayerInstigator(InstigatedBy, DamageCauser))
	{
		return;
	}

	// 한 발 한 발을 전부 세면 연사 한 번에 수배가 최고까지 오른다.
	const TWeakObjectPtr<AActor> Key(DamagedActor);
	if (const float* LastTime = LastInjuryReportTime.Find(Key))
	{
		if (ElapsedSeconds - *LastTime < Settings->InjuryReportCooldownSeconds)
		{
			WoundedByPlayer.Add(Key);
			return;
		}
	}

	LastInjuryReportTime.Add(Key, ElapsedSeconds);
	WoundedByPlayer.Add(Key);

	// 맞은 자리에서 총성이 난 것으로 치고 주변 시민을 흩어지게 한다.
	if (UGunDayCrowdSubsystem* Crowd = GetWorld() ? GetWorld()->GetSubsystem<UGunDayCrowdSubsystem>() : nullptr)
	{
		Crowd->NotifyGunshot(DamagedActor->GetActorLocation());
	}

	const bool bPolice = IsPoliceActor(*DamagedActor);
	Wanted->ReportCrime(bPolice ? EGunDayCrime::PoliceInjured : EGunDayCrime::CivilianInjured);
}

void UGunDayCrimeWatcherSubsystem::HandlePawnDestroyed(AActor* DestroyedActor)
{
	UGunDayWantedSubsystem* Wanted = GetWantedSubsystem();
	if (!Wanted || !IsValid(DestroyedActor))
	{
		return;
	}

	const TWeakObjectPtr<AActor> Key(DestroyedActor);

	WatchedPawns.Remove(Key);
	LastInjuryReportTime.Remove(Key);

	// 플레이어가 때린 적이 있는 상대가 사라졌다면 사망으로 친다.
	if (WoundedByPlayer.Remove(Key) > 0)
	{
		const bool bPolice = IsPoliceActor(*DestroyedActor);
		Wanted->ReportCrime(bPolice ? EGunDayCrime::PoliceKilled : EGunDayCrime::CivilianKilled);
	}
}

void UGunDayCrimeWatcherSubsystem::ReportPlayerGunfire()
{
	const UGunDayCoreSettings* Settings = GetSettings();
	UGunDayWantedSubsystem* Wanted = GetWantedSubsystem();
	if (!Settings || !Wanted || !bWatchEnabled)
	{
		return;
	}

	if (TimeSinceGunfireReport < Settings->GunfireReportCooldownSeconds)
	{
		return;
	}

	TimeSinceGunfireReport = 0.0f;
	Wanted->ReportCrime(EGunDayCrime::PublicGunfire);

	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (UGunDayCrowdSubsystem* Crowd = GetWorld() ? GetWorld()->GetSubsystem<UGunDayCrowdSubsystem>() : nullptr)
		{
			Crowd->NotifyGunshot(PlayerPawn->GetActorLocation());
		}
	}
}

void UGunDayCrimeWatcherSubsystem::SetWatchEnabled(bool bEnabled)
{
	bWatchEnabled = bEnabled;
}

bool UGunDayCrimeWatcherSubsystem::IsPoliceActor(const AActor& Actor) const
{
	// 우리가 투입한 인원이면 경찰이다.
	if (const UGunDayPoliceResponseSubsystem* Police = GetWorld() ? GetWorld()->GetSubsystem<UGunDayPoliceResponseSubsystem>() : nullptr)
	{
		FGunDayResponseTier Tier;
		if (Police->GetActiveTier(Tier) && !Tier.ResponderClass.IsNull())
		{
			if (const UClass* ResponderClass = Tier.ResponderClass.Get())
			{
				if (Actor.IsA(ResponderClass))
				{
					return true;
				}
			}
		}
	}

	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return false;
	}

	for (const TSoftClassPtr<AActor>& PoliceClass : Settings->PoliceClasses)
	{
		if (const UClass* Loaded = PoliceClass.Get())
		{
			if (Actor.IsA(Loaded))
			{
				return true;
			}
		}
	}

	return false;
}

bool UGunDayCrimeWatcherSubsystem::IsPlayerInstigator(const AController* InstigatedBy, const AActor* DamageCauser) const
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return false;
	}

	if (InstigatedBy && InstigatedBy == PlayerPawn->GetController())
	{
		return true;
	}

	// 투사체처럼 컨트롤러가 비어 오는 경우가 있다. 피해를 준 액터 쪽을 따라가 본다.
	if (DamageCauser)
	{
		if (DamageCauser == PlayerPawn)
		{
			return true;
		}

		if (const AActor* Owner = DamageCauser->GetOwner())
		{
			if (Owner == PlayerPawn)
			{
				return true;
			}
		}

		if (DamageCauser->GetInstigator() == PlayerPawn)
		{
			return true;
		}
	}

	return false;
}

const UGunDayCoreSettings* UGunDayCrimeWatcherSubsystem::GetSettings() const
{
	return GetDefault<UGunDayCoreSettings>();
}

UGunDayWantedSubsystem* UGunDayCrimeWatcherSubsystem::GetWantedSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGunDayWantedSubsystem>() : nullptr;
}
