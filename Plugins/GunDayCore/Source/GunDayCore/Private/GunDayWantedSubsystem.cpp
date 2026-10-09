// Copyright K-GTA. All Rights Reserved.

#include "GunDayWantedSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDayDebug.h"

UGunDayWantedSubsystem* UGunDayWantedSubsystem::Get(const UObject* WorldContextObject)
{
	if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UGunDayWantedSubsystem>();
		}
	}

	return nullptr;
}

void UGunDayWantedSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Heat = 0.0f;
	WantedLevel = 0;
	WitnessCount = 0;
	TimeSinceLastSeenSeconds = 0.0f;
	bEnabled = true;
}

void UGunDayWantedSubsystem::Deinitialize()
{
	OnWantedLevelChanged.Clear();
	OnSpottedChanged.Clear();

	Super::Deinitialize();
}

ETickableTickType UGunDayWantedSubsystem::GetTickableTickType() const
{
	// CDO 는 틱하면 안 된다.
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UGunDayWantedSubsystem::IsTickable() const
{
	// 열기가 0 이면 할 일이 없지만, 디버그 HUD 가 켜져 있으면 계속 그려야 한다.
	return bEnabled && (Heat > 0.0f || GunDayDebug::IsHUDEnabled());
}

TStatId UGunDayWantedSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGunDayWantedSubsystem, STATGROUP_Tickables);
}

void UGunDayWantedSubsystem::Tick(float DeltaTime)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	// 아래 감소 로직이 중간에 빠져나가도 HUD 는 매 프레임 그려야 한다.
	GunDayDebug::DrawHUD(*this);

	const bool bSpotted = IsSpotted();
	if (bSpotted)
	{
		TimeSinceLastSeenSeconds = 0.0f;

		if (Settings->bDecayOnlyWhenUnseen)
		{
			// 보고 있는 눈이 있는 한 열기는 그대로다.
			return;
		}
	}
	else
	{
		TimeSinceLastSeenSeconds += DeltaTime;
	}

	if (GetTimeUntilDecay() > 0.0f)
	{
		return;
	}

	SetHeat(Heat - Settings->HeatDecayPerSecond * DeltaTime);
}

void UGunDayWantedSubsystem::ReportCrime(EGunDayCrime Crime)
{
	if (!bEnabled)
	{
		return;
	}

	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	const float* Amount = Settings->CrimeHeat.Find(Crime);
	if (!Amount)
	{
		UE_LOG(LogGunDay, Warning, TEXT("ReportCrime: %s 에 대한 열기 값이 설정에 없다."),
			*StaticEnum<EGunDayCrime>()->GetNameStringByValue(static_cast<int64>(Crime)));
		return;
	}

	AddHeat(*Amount);
}

void UGunDayWantedSubsystem::AddHeat(float Amount)
{
	if (!bEnabled || FMath::IsNearlyZero(Amount))
	{
		return;
	}

	if (Amount > 0.0f)
	{
		// 새 범죄가 생기면 회피 시계를 처음부터 다시 잰다.
		TimeSinceLastSeenSeconds = 0.0f;
	}

	SetHeat(Heat + Amount);
}

void UGunDayWantedSubsystem::AddWitness()
{
	++WitnessCount;

	if (WitnessCount == 1)
	{
		TimeSinceLastSeenSeconds = 0.0f;
		OnSpottedChanged.Broadcast(true);
	}
}

void UGunDayWantedSubsystem::RemoveWitness()
{
	if (WitnessCount <= 0)
	{
		UE_LOG(LogGunDay, Warning, TEXT("RemoveWitness: 목격자가 없는데 호출됐다. AddWitness 와 짝이 맞는지 확인할 것."));
		return;
	}

	--WitnessCount;

	if (WitnessCount == 0)
	{
		TimeSinceLastSeenSeconds = 0.0f;
		OnSpottedChanged.Broadcast(false);
	}
}

void UGunDayWantedSubsystem::ClearWitnesses()
{
	if (WitnessCount == 0)
	{
		return;
	}

	WitnessCount = 0;
	TimeSinceLastSeenSeconds = 0.0f;
	OnSpottedChanged.Broadcast(false);
}

void UGunDayWantedSubsystem::ClearWanted()
{
	TimeSinceLastSeenSeconds = 0.0f;
	SetHeat(0.0f);
}

void UGunDayWantedSubsystem::SetWantedLevel(int32 NewLevel)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	const int32 ClampedLevel = FMath::Clamp(NewLevel, 0, GetMaxWantedLevel());
	if (ClampedLevel == 0)
	{
		ClearWanted();
		return;
	}

	// 해당 레벨의 문턱값에 딱 맞춰 둔다. 바로 아래로 떨어지지 않게 최소치를 쓴다.
	TimeSinceLastSeenSeconds = 0.0f;
	SetHeat(Settings->WantedLevelThresholds[ClampedLevel - 1]);
}

int32 UGunDayWantedSubsystem::GetMaxWantedLevel() const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	return Settings ? Settings->WantedLevelThresholds.Num() : 0;
}

float UGunDayWantedSubsystem::GetHeatFractionToNextLevel() const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings || Settings->WantedLevelThresholds.Num() == 0)
	{
		return 0.0f;
	}

	if (WantedLevel >= GetMaxWantedLevel())
	{
		return 1.0f;
	}

	const float LowerBound = (WantedLevel > 0) ? Settings->WantedLevelThresholds[WantedLevel - 1] : 0.0f;
	const float UpperBound = Settings->WantedLevelThresholds[WantedLevel];
	const float Span = UpperBound - LowerBound;

	return (Span > KINDA_SMALL_NUMBER) ? FMath::Clamp((Heat - LowerBound) / Span, 0.0f, 1.0f) : 0.0f;
}

float UGunDayWantedSubsystem::GetTimeUntilDecay() const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return 0.0f;
	}

	if (IsSpotted() && Settings->bDecayOnlyWhenUnseen)
	{
		// 시야에 잡혀 있는 동안은 회피 시계가 아예 돌지 않는다.
		return Settings->EvasionDelaySeconds + Settings->EvasionDelayPerLevel * WantedLevel;
	}

	const float Delay = Settings->EvasionDelaySeconds + Settings->EvasionDelayPerLevel * WantedLevel;
	return FMath::Max(0.0f, Delay - TimeSinceLastSeenSeconds);
}

void UGunDayWantedSubsystem::SetWantedSystemEnabled(bool bInEnabled)
{
	if (bEnabled == bInEnabled)
	{
		return;
	}

	bEnabled = bInEnabled;

	if (!bEnabled)
	{
		ClearWitnesses();
		ClearWanted();
	}
}

void UGunDayWantedSubsystem::SetHeat(float NewHeat)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	const float MaxHeat = Settings ? Settings->MaxHeat : 0.0f;

	const float ClampedHeat = FMath::Clamp(NewHeat, 0.0f, MaxHeat);
	if (FMath::IsNearlyEqual(ClampedHeat, Heat))
	{
		return;
	}

	Heat = ClampedHeat;
	UpdateWantedLevel();
}

int32 UGunDayWantedSubsystem::HeatToWantedLevel(float InHeat) const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return 0;
	}

	int32 Level = 0;
	for (int32 Index = 0; Index < Settings->WantedLevelThresholds.Num(); ++Index)
	{
		if (InHeat >= Settings->WantedLevelThresholds[Index])
		{
			Level = Index + 1;
		}
		else
		{
			break;
		}
	}

	return Level;
}

void UGunDayWantedSubsystem::UpdateWantedLevel()
{
	const int32 NewLevel = HeatToWantedLevel(Heat);
	if (NewLevel == WantedLevel)
	{
		return;
	}

	const int32 OldLevel = WantedLevel;
	WantedLevel = NewLevel;

	UE_LOG(LogGunDay, Log, TEXT("수배 레벨 %d -> %d (열기 %.1f)"), OldLevel, NewLevel, Heat);
	OnWantedLevelChanged.Broadcast(OldLevel, NewLevel);
}

const UGunDayCoreSettings* UGunDayWantedSubsystem::GetSettings() const
{
	return GetDefault<UGunDayCoreSettings>();
}
