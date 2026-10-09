// Copyright K-GTA. All Rights Reserved.

#include "GunDaySocietySubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDayDebug.h"

namespace
{
	/** 한 축의 입장을 뽑는다. 폰 이름과 축 번호를 섞어 늘 같은 값이 나오게 한다. */
	uint8 DrawPosition(const FString& Seed, int32 Axis, float NeutralChance)
	{
		const uint32 Hash = FCrc::StrCrc32(*FString::Printf(TEXT("%s#%d"), *Seed, Axis));

		// 0~999 로 펼친다.
		const uint32 Roll = (Hash ^ (Hash >> 13)) % 1000;
		const uint32 NeutralCut = static_cast<uint32>(FMath::Clamp(NeutralChance, 0.0f, 1.0f) * 1000.0f);

		if (Roll < NeutralCut)
		{
			return 0;
		}

		return ((Roll & 1) == 0) ? 1 : 2;
	}

	const TCHAR* FaultLineName(EGunDayFaultLine FaultLine)
	{
		switch (FaultLine)
		{
		case EGunDayFaultLine::Politics:   return TEXT("정치");
		case EGunDayFaultLine::Property:   return TEXT("부동산");
		case EGunDayFaultLine::Generation: return TEXT("세대");
		case EGunDayFaultLine::Region:     return TEXT("지역");
		case EGunDayFaultLine::Gender:     return TEXT("성별");
		default:                           return TEXT("?");
		}
	}
}

UGunDaySocietySubsystem* UGunDaySocietySubsystem::Get(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetSubsystem<UGunDaySocietySubsystem>();
	}

	return nullptr;
}

bool UGunDaySocietySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}

	return false;
}

void UGunDaySocietySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UGunDayCoreSettings* Settings = GetSettings();
	Jeong = Settings ? Settings->StartingJeong : 50.0f;
}

void UGunDaySocietySubsystem::Deinitialize()
{
	Profiles.Reset();
	OnJeongChanged.Clear();

	Super::Deinitialize();
}

ETickableTickType UGunDaySocietySubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UGunDaySocietySubsystem::IsTickable() const
{
	return true;
}

TStatId UGunDaySocietySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGunDaySocietySubsystem, STATGROUP_Tickables);
}

void UGunDaySocietySubsystem::Tick(float DeltaTime)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	// 아무 일이 없으면 기준값으로 천천히 돌아간다. 사람은 잊는다.
	if (!FMath::IsNearlyEqual(Jeong, Settings->BaselineJeong, 0.01f))
	{
		const float Step = Settings->JeongRecoveryPerSecond * DeltaTime;
		Jeong = (Jeong < Settings->BaselineJeong)
			? FMath::Min(Settings->BaselineJeong, Jeong + Step)
			: FMath::Max(Settings->BaselineJeong, Jeong - Step);
	}

	// 사라진 폰의 진영은 들고 있을 필요가 없다.
	TimeSincePrune += DeltaTime;
	if (TimeSincePrune >= 10.0f)
	{
		TimeSincePrune = 0.0f;

		for (auto It = Profiles.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	if (GEngine && GunDayDebug::IsHUDEnabled())
	{
		const FColor Color = (Jeong >= 60.0f) ? FColor(140, 210, 150)
			: (Jeong >= 30.0f) ? FColor(220, 200, 120)
			: FColor(220, 110, 100);

		GEngine->AddOnScreenDebugMessage(7708, 1.0f, Color,
			FString::Printf(TEXT("정(情) %.0f"), Jeong));
	}
}

void UGunDaySocietySubsystem::AddJeong(float Delta)
{
	if (FMath::IsNearlyZero(Delta))
	{
		return;
	}

	const float Previous = Jeong;
	Jeong = FMath::Clamp(Jeong + Delta, 0.0f, 100.0f);

	const float Applied = Jeong - Previous;
	if (!FMath::IsNearlyZero(Applied))
	{
		OnJeongChanged.Broadcast(Jeong, Applied);
	}
}

void UGunDaySocietySubsystem::SetJeong(float NewJeong)
{
	AddJeong(FMath::Clamp(NewJeong, 0.0f, 100.0f) - Jeong);
}

float UGunDaySocietySubsystem::GetJeongFraction() const
{
	return FMath::Clamp(Jeong / 100.0f, 0.0f, 1.0f);
}

FGunDayPersonProfile UGunDaySocietySubsystem::GetProfile(const APawn* Pawn)
{
	if (!IsValid(Pawn))
	{
		return FGunDayPersonProfile();
	}

	const TWeakObjectPtr<APawn> Key(const_cast<APawn*>(Pawn));
	if (const FGunDayPersonProfile* Found = Profiles.Find(Key))
	{
		return *Found;
	}

	const FGunDayPersonProfile Profile = MakeProfile(*Pawn);
	Profiles.Add(Key, Profile);

	return Profile;
}

float UGunDaySocietySubsystem::GetFriction(const APawn* First, const APawn* Second)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings || !IsValid(First) || !IsValid(Second) || First == Second)
	{
		return 0.0f;
	}

	const FGunDayPersonProfile A = GetProfile(First);
	const FGunDayPersonProfile B = GetProfile(Second);
	if (!A.IsValidProfile() || !B.IsValidProfile())
	{
		return 0.0f;
	}

	float Sum = 0.0f;
	float TotalWeight = 0.0f;

	for (int32 Axis = 0; Axis < static_cast<int32>(EGunDayFaultLine::MAX); ++Axis)
	{
		const float Weight = Settings->FaultLineWeights.IsValidIndex(Axis) ? Settings->FaultLineWeights[Axis] : 1.0f;
		TotalWeight += Weight;

		// 둘 다 어느 쪽도 아니면 부딪힐 일이 없다. 반대편이면 그 축이 그대로 마찰이 된다.
		if (A.Positions[Axis] != 0 && B.Positions[Axis] != 0 && A.Positions[Axis] != B.Positions[Axis])
		{
			Sum += Weight;
		}
	}

	return (TotalWeight > KINDA_SMALL_NUMBER) ? FMath::Clamp(Sum / TotalWeight, 0.0f, 1.0f) : 0.0f;
}

bool UGunDaySocietySubsystem::GetSharpestFaultLine(const APawn* First, const APawn* Second, EGunDayFaultLine& OutFaultLine)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings || !IsValid(First) || !IsValid(Second))
	{
		return false;
	}

	const FGunDayPersonProfile A = GetProfile(First);
	const FGunDayPersonProfile B = GetProfile(Second);
	if (!A.IsValidProfile() || !B.IsValidProfile())
	{
		return false;
	}

	float BestWeight = 0.0f;
	bool bFound = false;

	for (int32 Axis = 0; Axis < static_cast<int32>(EGunDayFaultLine::MAX); ++Axis)
	{
		if (A.Positions[Axis] == 0 || B.Positions[Axis] == 0 || A.Positions[Axis] == B.Positions[Axis])
		{
			continue;
		}

		const float Weight = Settings->FaultLineWeights.IsValidIndex(Axis) ? Settings->FaultLineWeights[Axis] : 1.0f;
		if (Weight > BestWeight)
		{
			BestWeight = Weight;
			OutFaultLine = static_cast<EGunDayFaultLine>(Axis);
			bFound = true;
		}
	}

	return bFound;
}

FString UGunDaySocietySubsystem::DescribeProfile(const APawn* Pawn)
{
	const FGunDayPersonProfile Profile = GetProfile(Pawn);
	if (!Profile.IsValidProfile())
	{
		return TEXT("(없음)");
	}

	TArray<FString> Parts;
	for (int32 Axis = 0; Axis < static_cast<int32>(EGunDayFaultLine::MAX); ++Axis)
	{
		const uint8 Position = Profile.Positions[Axis];
		if (Position == 0)
		{
			continue;
		}

		Parts.Add(FString::Printf(TEXT("%s%d"), FaultLineName(static_cast<EGunDayFaultLine>(Axis)), Position));
	}

	return Parts.Num() > 0 ? FString::Join(Parts, TEXT(" ")) : TEXT("무색");
}

FGunDayPersonProfile UGunDaySocietySubsystem::MakeProfile(const APawn& Pawn) const
{
	const UGunDayCoreSettings* Settings = GetSettings();
	const float NeutralChance = Settings ? Settings->NeutralPositionChance : 0.3f;

	// 이름을 씨앗으로 쓴다. 다시 불러도 같은 사람은 같은 진영이다.
	const FString Seed = Pawn.GetName();

	FGunDayPersonProfile Profile;
	Profile.Positions.Reserve(static_cast<int32>(EGunDayFaultLine::MAX));

	for (int32 Axis = 0; Axis < static_cast<int32>(EGunDayFaultLine::MAX); ++Axis)
	{
		Profile.Positions.Add(DrawPosition(Seed, Axis, NeutralChance));
	}

	return Profile;
}

const UGunDayCoreSettings* UGunDaySocietySubsystem::GetSettings() const
{
	return GetDefault<UGunDayCoreSettings>();
}
