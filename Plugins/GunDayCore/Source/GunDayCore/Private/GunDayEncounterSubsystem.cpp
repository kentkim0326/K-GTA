// Copyright K-GTA. All Rights Reserved.

#include "GunDayEncounterSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GunDayCore.h"
#include "GunDayCrowdSubsystem.h"
#include "GunDayPoliceResponseSubsystem.h"
#include "GunDayWantedSubsystem.h"
#include "Kismet/GameplayStatics.h"

UGunDayEncounterSubsystem* UGunDayEncounterSubsystem::Get(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetSubsystem<UGunDayEncounterSubsystem>();
	}

	return nullptr;
}

bool UGunDayEncounterSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}

	return false;
}

void UGunDayEncounterSubsystem::Deinitialize()
{
	if (bBoundToPolice)
	{
		if (UGunDayPoliceResponseSubsystem* Police = GetPoliceSubsystem())
		{
			Police->OnResponderLost.RemoveDynamic(this, &UGunDayEncounterSubsystem::HandleResponderLost);
		}

		bBoundToPolice = false;
	}

	bRunning = false;

	OnEncounterStarted.Clear();
	OnEncounterFinished.Clear();

	Super::Deinitialize();
}

ETickableTickType UGunDayEncounterSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

TStatId UGunDayEncounterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGunDayEncounterSubsystem, STATGROUP_Tickables);
}

void UGunDayEncounterSubsystem::StartEncounter(float InDurationSeconds, int32 WantedLevel)
{
	UGunDayWantedSubsystem* Wanted = GetWantedSubsystem();
	UGunDayPoliceResponseSubsystem* Police = GetPoliceSubsystem();
	if (!Wanted || !Police)
	{
		UE_LOG(LogGunDay, Warning, TEXT("총격전 측정: 필요한 서브시스템을 찾지 못했다."));
		return;
	}

	// 지난 판의 흔적을 지운다. 같은 조건에서 시작해야 비교가 된다.
	Police->DismissAllResponders(true);
	Wanted->ClearWitnesses();
	Wanted->ClearWanted();

	if (UGunDayCrowdSubsystem* Crowd = GetWorld() ? GetWorld()->GetSubsystem<UGunDayCrowdSubsystem>() : nullptr)
	{
		Crowd->CalmAll();
	}

	if (!bBoundToPolice)
	{
		Police->OnResponderLost.AddDynamic(this, &UGunDayEncounterSubsystem::HandleResponderLost);
		bBoundToPolice = true;
	}

	DurationSeconds = FMath::Max(1.0f, InDurationSeconds);
	ElapsedSeconds = 0.0f;
	Kills = 0;
	bRunning = true;

	Wanted->SetWantedLevel(FMath::Max(1, WantedLevel));

	UE_LOG(LogGunDay, Log, TEXT("총격전 측정 시작: %.0f초, 수배 레벨 %d"), DurationSeconds, WantedLevel);
	OnEncounterStarted.Broadcast();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(7710, 3.0f, FColor::White,
			FString::Printf(TEXT("총격전 시작 — %.0f초 버텨라"), DurationSeconds));
	}
}

void UGunDayEncounterSubsystem::StopEncounter()
{
	if (!bRunning)
	{
		return;
	}

	bRunning = false;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(7710, 3.0f, FColor::White, TEXT("총격전 측정 중단"));
	}
}

float UGunDayEncounterSubsystem::GetRemainingSeconds() const
{
	return bRunning ? FMath::Max(0.0f, DurationSeconds - ElapsedSeconds) : 0.0f;
}

void UGunDayEncounterSubsystem::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}

	ElapsedSeconds += DeltaTime;

	if (IsPlayerDown())
	{
		FinishEncounter(false);
		return;
	}

	if (ElapsedSeconds >= DurationSeconds)
	{
		FinishEncounter(true);
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(7711, 1.0f, FColor::White,
			FString::Printf(TEXT("남은 시간 %.0f초   처치 %d명"), GetRemainingSeconds(), Kills));
	}
}

void UGunDayEncounterSubsystem::HandleResponderLost()
{
	// 거리 때문에 정리된 인원은 OnResponderDismissed 로 빠지므로 여기 오지 않는다.
	if (bRunning)
	{
		++Kills;
	}
}

bool UGunDayEncounterSubsystem::IsPlayerDown() const
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!IsValid(PlayerPawn))
	{
		return true;
	}

	// 킷이 사망 시 폰을 남겨 두고 컨트롤러만 떼는 경우가 있다.
	return PlayerPawn->GetController() == nullptr;
}

void UGunDayEncounterSubsystem::FinishEncounter(bool bSurvived)
{
	bRunning = false;

	bHasLastResult = true;
	bLastSurvived = bSurvived;
	LastKills = Kills;
	LastSeconds = ElapsedSeconds;

	UE_LOG(LogGunDay, Log, TEXT("총격전 측정 끝: %s, %.1f초, 처치 %d명"),
		bSurvived ? TEXT("생존") : TEXT("사망"), LastSeconds, LastKills);

	ShowLastResult();

	OnEncounterFinished.Broadcast(bSurvived, LastKills, LastSeconds);
}

void UGunDayEncounterSubsystem::ShowLastResult()
{
	if (!GEngine || !bHasLastResult)
	{
		return;
	}

	// 화면에 오래 남겨 둔다. 60초를 버틴 뒤 결과를 놓치면 다시 돌려야 한다.
	const FColor Color = bLastSurvived ? FColor(120, 220, 120) : FColor(230, 80, 70);
	GEngine->AddOnScreenDebugMessage(7710, 30.0f, Color,
		FString::Printf(TEXT("%s — %.1f초 버팀, 경찰 %d명 처치"),
			bLastSurvived ? TEXT("생존") : TEXT("사망"), LastSeconds, LastKills));
}

UGunDayWantedSubsystem* UGunDayEncounterSubsystem::GetWantedSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGunDayWantedSubsystem>() : nullptr;
}

UGunDayPoliceResponseSubsystem* UGunDayEncounterSubsystem::GetPoliceSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGunDayPoliceResponseSubsystem>() : nullptr;
}
