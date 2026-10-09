// Copyright K-GTA. All Rights Reserved.

#include "GunDayDebug.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDayPoliceResponseSubsystem.h"
#include "GunDayWantedSubsystem.h"
#include "HAL/IConsoleManager.h"

namespace
{
	int32 GGunDayShowDebug = 0;

	FAutoConsoleVariableRef CVarGunDayShowDebug(
		TEXT("GunDay.ShowDebug"),
		GGunDayShowDebug,
		TEXT("1 이면 화면 좌상단에 수배 레벨과 열기를 띄운다."),
		ECVF_Cheat);

	UGunDayWantedSubsystem* FindSubsystem(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		UGameInstance* GameInstance = World->GetGameInstance();
		return GameInstance ? GameInstance->GetSubsystem<UGunDayWantedSubsystem>() : nullptr;
	}

	/** 콘솔 인자를 EGunDayCrime 으로 바꾼다. 이름("PoliceKilled")과 번호 둘 다 받는다. */
	bool ParseCrime(const FString& Token, EGunDayCrime& OutCrime)
	{
		const UEnum* CrimeEnum = StaticEnum<EGunDayCrime>();

		if (Token.IsNumeric())
		{
			const int64 Value = FCString::Atoi64(*Token);
			if (Value >= 0 && Value < static_cast<int64>(EGunDayCrime::MAX))
			{
				OutCrime = static_cast<EGunDayCrime>(Value);
				return true;
			}

			return false;
		}

		const int64 Value = CrimeEnum->GetValueByNameString(Token);
		if (Value == INDEX_NONE)
		{
			return false;
		}

		OutCrime = static_cast<EGunDayCrime>(Value);
		return true;
	}

	/** 쓸 수 있는 범죄 이름을 한 줄로 늘어놓는다. */
	FString ListCrimeNames()
	{
		const UEnum* CrimeEnum = StaticEnum<EGunDayCrime>();

		TArray<FString> Names;
		for (int32 Index = 0; Index < static_cast<int32>(EGunDayCrime::MAX); ++Index)
		{
			Names.Add(CrimeEnum->GetNameStringByValue(Index));
		}

		return FString::Join(Names, TEXT(", "));
	}

	FAutoConsoleCommandWithWorldAndArgs CmdReportCrime(
		TEXT("GunDay.ReportCrime"),
		TEXT("범죄 한 건을 신고한다. 인자는 이름 또는 번호. 예: GunDay.ReportCrime PoliceKilled"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGunDayWantedSubsystem* Subsystem = FindSubsystem(World);
			if (!Subsystem)
			{
				return;
			}

			EGunDayCrime Crime;
			if (Args.Num() == 0 || !ParseCrime(Args[0], Crime))
			{
				UE_LOG(LogGunDay, Warning, TEXT("사용법: GunDay.ReportCrime <%s>"), *ListCrimeNames());
				return;
			}

			Subsystem->ReportCrime(Crime);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdFire(
		TEXT("GunDay.Fire"),
		TEXT("공공장소 발포 한 번. GunDay.ReportCrime PublicGunfire 와 같다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UGunDayWantedSubsystem* Subsystem = FindSubsystem(World))
			{
				Subsystem->ReportCrime(EGunDayCrime::PublicGunfire);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAddHeat(
		TEXT("GunDay.AddHeat"),
		TEXT("열기를 직접 더한다. 음수면 깎인다. 예: GunDay.AddHeat 50"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGunDayWantedSubsystem* Subsystem = FindSubsystem(World);
			if (!Subsystem || Args.Num() == 0)
			{
				return;
			}

			Subsystem->AddHeat(FCString::Atof(*Args[0]));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSetWanted(
		TEXT("GunDay.SetWanted"),
		TEXT("수배 레벨을 강제로 맞춘다. 예: GunDay.SetWanted 3"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGunDayWantedSubsystem* Subsystem = FindSubsystem(World);
			if (!Subsystem || Args.Num() == 0)
			{
				return;
			}

			Subsystem->SetWantedLevel(FCString::Atoi(*Args[0]));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdWitness(
		TEXT("GunDay.Witness"),
		TEXT("목격자 수를 바꾼다. 1 이면 늘리고 -1 이면 줄인다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGunDayWantedSubsystem* Subsystem = FindSubsystem(World);
			if (!Subsystem)
			{
				return;
			}

			const int32 Delta = (Args.Num() > 0) ? FCString::Atoi(*Args[0]) : 1;
			for (int32 Step = 0; Step < FMath::Abs(Delta); ++Step)
			{
				if (Delta > 0)
				{
					Subsystem->AddWitness();
				}
				else
				{
					Subsystem->RemoveWitness();
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdClear(
		TEXT("GunDay.Clear"),
		TEXT("수배와 목격자를 전부 지운다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UGunDayWantedSubsystem* Subsystem = FindSubsystem(World))
			{
				Subsystem->ClearWitnesses();
				Subsystem->ClearWanted();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdPoliceDismiss(
		TEXT("GunDay.Police.Dismiss"),
		TEXT("투입된 경찰을 전부 치운다. 수배 레벨은 그대로 둔다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UGunDayPoliceResponseSubsystem* Police = World ? World->GetSubsystem<UGunDayPoliceResponseSubsystem>() : nullptr)
			{
				Police->DismissAllResponders(true);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdPoliceEnabled(
		TEXT("GunDay.Police.Enabled"),
		TEXT("경찰 투입을 켜고 끈다. 예: GunDay.Police.Enabled 0"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGunDayPoliceResponseSubsystem* Police = World ? World->GetSubsystem<UGunDayPoliceResponseSubsystem>() : nullptr;
			if (!Police)
			{
				return;
			}

			const bool bEnabled = (Args.Num() == 0) || (FCString::Atoi(*Args[0]) != 0);
			Police->SetResponseEnabled(bEnabled);
		}));

	/** 수배 레벨별 색. 0 은 회색, 올라갈수록 붉어진다. */
	FColor LevelColor(int32 Level)
	{
		switch (Level)
		{
		case 0:  return FColor(150, 150, 150);
		case 1:  return FColor(240, 220, 120);
		case 2:  return FColor(240, 170, 80);
		case 3:  return FColor(240, 120, 60);
		case 4:  return FColor(230, 70, 60);
		default: return FColor(220, 40, 110);
		}
	}
}

namespace GunDayDebug
{
	bool IsHUDEnabled()
	{
		return GGunDayShowDebug != 0;
	}

	void DrawHUD(const UGunDayWantedSubsystem& Subsystem)
	{
		if (!GEngine || !IsHUDEnabled())
		{
			return;
		}

		const int32 Level = Subsystem.GetWantedLevel();
		const int32 MaxLevel = Subsystem.GetMaxWantedLevel();

		// ★☆☆☆☆ 모양으로 수배 레벨을 보여 준다.
		FString Stars;
		for (int32 Index = 0; Index < MaxLevel; ++Index)
		{
			Stars += (Index < Level) ? TEXT("★") : TEXT("☆");
		}

		const float TimeUntilDecay = Subsystem.GetTimeUntilDecay();
		const FString DecayText = Subsystem.IsSpotted()
			? TEXT("목격 중 — 감소 없음")
			: (TimeUntilDecay > 0.0f
				? FString::Printf(TEXT("%.1f초 뒤 감소 시작"), TimeUntilDecay)
				: TEXT("감소 중"));

		const FColor Color = LevelColor(Level);

		// 키를 고정해 두면 매 프레임 같은 줄이 덮어써진다.
		GEngine->AddOnScreenDebugMessage(7701, 1.0f, Color,
			FString::Printf(TEXT("수배 %s  (레벨 %d / %d)"), *Stars, Level, MaxLevel));
		GEngine->AddOnScreenDebugMessage(7702, 1.0f, Color,
			FString::Printf(TEXT("열기 %.0f   다음 레벨까지 %.0f%%"), Subsystem.GetHeat(), Subsystem.GetHeatFractionToNextLevel() * 100.0f));
		GEngine->AddOnScreenDebugMessage(7703, 1.0f, Color, DecayText);
	}
}
