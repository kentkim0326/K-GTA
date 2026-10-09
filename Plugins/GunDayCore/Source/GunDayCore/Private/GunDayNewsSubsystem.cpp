// Copyright K-GTA. All Rights Reserved.

#include "GunDayNewsSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDayDebug.h"
#include "GunDaySocietySubsystem.h"

UGunDayNewsSubsystem* UGunDayNewsSubsystem::Get(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetSubsystem<UGunDayNewsSubsystem>();
	}

	return nullptr;
}

bool UGunDayNewsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}

	return false;
}

void UGunDayNewsSubsystem::Deinitialize()
{
	OnHeadline.Clear();
	OnDayRollover.Clear();

	Super::Deinitialize();
}

ETickableTickType UGunDayNewsSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UGunDayNewsSubsystem::IsTickable() const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	return Settings && Settings->bEnableNews;
}

TStatId UGunDayNewsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGunDayNewsSubsystem, STATGROUP_Tickables);
}

void UGunDayNewsSubsystem::Tick(float DeltaTime)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	TimeSinceHeadline += DeltaTime;
	TimeSinceDayStart += DeltaTime;

	if (TimeSinceDayStart >= Settings->DayLengthSeconds)
	{
		AdvanceDay();
	}

	// 사건이 없어도 뉴스는 계속 나온다. 그 무심함이 이 게임의 톤이다.
	if (TimeSinceHeadline >= Settings->NewsIntervalSeconds)
	{
		PushFiller();
	}

	if (GEngine && GunDayDebug::IsHUDEnabled() && !LastHeadline.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(7709, 1.0f, FColor(200, 200, 200),
			FString::Printf(TEXT("[뉴스] %s"), *LastHeadline));
	}
}

void UGunDayNewsSubsystem::ReportShooting(bool bFatal, bool bByPlayer)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings || !Settings->bEnableNews)
	{
		return;
	}

	++TodayShootings;

	if (bFatal)
	{
		++TodayDeaths;
		++TotalDeaths;
	}

	// 속보는 매번 내보내지 않는다. 매번 나오면 무심함이 사라진다.
	if (FMath::FRand() <= Settings->BreakingNewsChance)
	{
		PushFrom(bByPlayer ? Settings->PlayerBreakingTemplates : Settings->BreakingTemplates);
	}
}

void UGunDayNewsSubsystem::ReportDispute()
{
	++TodayDisputes;
}

void UGunDayNewsSubsystem::AdvanceDay()
{
	const UGunDayCoreSettings* Settings = GetSettings();

	TimeSinceDayStart = 0.0f;

	// 집계를 먼저 내보내고 날짜를 넘긴다. 어제 숫자가 필요하다.
	if (Settings)
	{
		PushFrom(Settings->DailyTemplates);
	}

	OnDayRollover.Broadcast(DayNumber, TodayDeaths);

	YesterdayDeaths = TodayDeaths;
	TodayDeaths = 0;
	TodayShootings = 0;
	TodayDisputes = 0;
	++DayNumber;
}

void UGunDayNewsSubsystem::PushHeadline(const FString& RawText)
{
	if (RawText.IsEmpty())
	{
		return;
	}

	LastHeadline = Format(RawText);
	TimeSinceHeadline = 0.0f;

	UE_LOG(LogGunDay, Log, TEXT("[뉴스] %s"), *LastHeadline);
	OnHeadline.Broadcast(LastHeadline);
}

void UGunDayNewsSubsystem::PushFiller()
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	// 정이 바닥이면 전용 문구를 섞는다. 사회가 어떤 상태인지는 뉴스가 먼저 안다.
	const UGunDaySocietySubsystem* Society = GetSocietySubsystem();
	const bool bLowJeong = Society && Society->GetJeong() <= Settings->LowJeongNewsThreshold;

	if (bLowJeong && Settings->LowJeongTemplates.Num() > 0 && FMath::FRand() <= 0.5f)
	{
		PushFrom(Settings->LowJeongTemplates);
		return;
	}

	PushFrom(Settings->FillerTemplates);
}

void UGunDayNewsSubsystem::PushFrom(const TArray<FString>& Templates)
{
	if (Templates.Num() == 0)
	{
		return;
	}

	PushHeadline(Templates[FMath::RandHelper(Templates.Num())]);
}

FString UGunDayNewsSubsystem::Format(const FString& Template) const
{
	const int32 Difference = TodayDeaths - YesterdayDeaths;
	const UGunDaySocietySubsystem* Society = GetSocietySubsystem();

	FString Result = Template;
	Result = Result.Replace(TEXT("{오늘}"), *FString::FromInt(TodayDeaths));
	Result = Result.Replace(TEXT("{어제}"), *FString::FromInt(YesterdayDeaths));
	Result = Result.Replace(TEXT("{차이}"), *FString::FromInt(FMath::Abs(Difference)));
	Result = Result.Replace(TEXT("{증감}"), (Difference >= 0) ? TEXT("증가") : TEXT("감소"));
	Result = Result.Replace(TEXT("{총}"), *FString::FromInt(TotalDeaths));
	Result = Result.Replace(TEXT("{시비}"), *FString::FromInt(TodayDisputes));
	Result = Result.Replace(TEXT("{날}"), *FString::FromInt(DayNumber));
	Result = Result.Replace(TEXT("{정}"), Society ? *FString::Printf(TEXT("%.0f"), Society->GetJeong()) : TEXT("?"));

	return Result;
}

const UGunDayCoreSettings* UGunDayNewsSubsystem::GetSettings() const
{
	return GetDefault<UGunDayCoreSettings>();
}

UGunDaySocietySubsystem* UGunDayNewsSubsystem::GetSocietySubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGunDaySocietySubsystem>() : nullptr;
}
