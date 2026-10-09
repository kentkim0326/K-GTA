// Copyright K-GTA. All Rights Reserved.

#include "GunDayDebug.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDayCrimeWatcherSubsystem.h"
#include "GunDayCrowdSubsystem.h"
#include "GunDayDisputeSubsystem.h"
#include "GunDayEncounterSubsystem.h"
#include "GunDayPoliceResponseSubsystem.h"
#include "GunDaySocietySubsystem.h"
#include "GunDayWantedSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

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

	FAutoConsoleCommandWithWorldAndArgs CmdJeong(
		TEXT("GunDay.Jeong"),
		TEXT("사회의 정을 0~100 으로 맞춘다. 인자가 없으면 지금 값을 로그에 찍는다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGunDaySocietySubsystem* Society = World ? World->GetSubsystem<UGunDaySocietySubsystem>() : nullptr;
			if (!Society)
			{
				return;
			}

			if (Args.Num() == 0)
			{
				UE_LOG(LogGunDay, Log, TEXT("정(情) %.1f"), Society->GetJeong());
				return;
			}

			Society->SetJeong(FCString::Atof(*Args[0]));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdProfile(
		TEXT("GunDay.Profile"),
		TEXT("주변 사람들의 진영을 로그에 찍는다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			UGunDaySocietySubsystem* Society = World ? World->GetSubsystem<UGunDaySocietySubsystem>() : nullptr;
			const APawn* Player = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
			if (!Society || !Player)
			{
				return;
			}

			for (TActorIterator<APawn> It(World); It; ++It)
			{
				APawn* Pawn = *It;
				if (!IsValid(Pawn) || Pawn == Player)
				{
					continue;
				}

				if (FVector::Dist(Pawn->GetActorLocation(), Player->GetActorLocation()) > 4000.0f)
				{
					continue;
				}

				UE_LOG(LogGunDay, Log, TEXT("%s — %s"), *Pawn->GetName(), *Society->DescribeProfile(Pawn));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDisputeStart(
		TEXT("GunDay.Dispute.Start"),
		TEXT("주변 시민 둘로 시비를 일으킨다. 인자로 상황 번호를 줄 수 있다. 예: GunDay.Dispute.Start 1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGunDayDisputeSubsystem* Dispute = World ? World->GetSubsystem<UGunDayDisputeSubsystem>() : nullptr;
			if (!Dispute)
			{
				return;
			}

			const int32 ScenarioIndex = (Args.Num() > 0) ? FCString::Atoi(*Args[0]) : -1;
			Dispute->StartDisputeNearPlayer(ScenarioIndex);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDisputeClear(
		TEXT("GunDay.Dispute.Clear"),
		TEXT("진행 중인 시비를 전부 끝낸다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UGunDayDisputeSubsystem* Dispute = World ? World->GetSubsystem<UGunDayDisputeSubsystem>() : nullptr)
			{
				Dispute->ClearDisputes();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDisputeEnabled(
		TEXT("GunDay.Dispute.Enabled"),
		TEXT("시비 발생을 켜고 끈다. 예: GunDay.Dispute.Enabled 0"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGunDayDisputeSubsystem* Dispute = World ? World->GetSubsystem<UGunDayDisputeSubsystem>() : nullptr;
			if (!Dispute)
			{
				return;
			}

			const bool bEnabled = (Args.Num() == 0) || (FCString::Atoi(*Args[0]) != 0);
			Dispute->SetDisputesEnabled(bEnabled);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAlleyStart(
		TEXT("GunDay.Alley.Start"),
		TEXT("골목 총격전 측정을 시작한다. 인자: 제한 시간(초), 수배 레벨. 예: GunDay.Alley.Start 60 1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGunDayEncounterSubsystem* Encounter = World ? World->GetSubsystem<UGunDayEncounterSubsystem>() : nullptr;
			if (!Encounter)
			{
				return;
			}

			const float Duration = (Args.Num() > 0) ? FCString::Atof(*Args[0]) : 60.0f;
			const int32 Level = (Args.Num() > 1) ? FCString::Atoi(*Args[1]) : 1;
			Encounter->StartEncounter(Duration, Level);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAlleyStop(
		TEXT("GunDay.Alley.Stop"),
		TEXT("골목 총격전 측정을 중단한다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UGunDayEncounterSubsystem* Encounter = World ? World->GetSubsystem<UGunDayEncounterSubsystem>() : nullptr)
			{
				Encounter->StopEncounter();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAlleyResult(
		TEXT("GunDay.Alley.Result"),
		TEXT("지난 판의 결과를 다시 띄운다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UGunDayEncounterSubsystem* Encounter = World ? World->GetSubsystem<UGunDayEncounterSubsystem>() : nullptr)
			{
				Encounter->ShowLastResult();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdCrowdGunshot(
		TEXT("GunDay.Crowd.Gunshot"),
		TEXT("플레이어 위치에서 총성이 난 것으로 쳐 주변 시민을 흩어지게 한다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			UGunDayCrowdSubsystem* Crowd = World ? World->GetSubsystem<UGunDayCrowdSubsystem>() : nullptr;
			const APawn* PlayerPawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
			if (Crowd && PlayerPawn)
			{
				Crowd->NotifyGunshot(PlayerPawn->GetActorLocation());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdCrowdCalm(
		TEXT("GunDay.Crowd.Calm"),
		TEXT("놀란 시민을 전부 진정시킨다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UGunDayCrowdSubsystem* Crowd = World ? World->GetSubsystem<UGunDayCrowdSubsystem>() : nullptr)
			{
				Crowd->CalmAll();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAutoReport(
		TEXT("GunDay.AutoReport"),
		TEXT("플레이어가 입힌 피해의 자동 신고를 켜고 끈다. 예: GunDay.AutoReport 0"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGunDayCrimeWatcherSubsystem* Watcher = World ? World->GetSubsystem<UGunDayCrimeWatcherSubsystem>() : nullptr;
			if (!Watcher)
			{
				return;
			}

			const bool bEnabled = (Args.Num() == 0) || (FCString::Atoi(*Args[0]) != 0);
			Watcher->SetWatchEnabled(bEnabled);
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
			? FString::Printf(TEXT("목격자 %d명 — 감소 없음"), Subsystem.GetWitnessCount())
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
