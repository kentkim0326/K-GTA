// Copyright K-GTA. All Rights Reserved.

#include "GunDayCivilianReactionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDaySocietySubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
	FAutoConsoleCommandWithWorldAndArgs CmdProvokeCivilians(
		TEXT("GunDay.Civilians.Provoke"),
		TEXT("가까운 행인 n 명(기본 1)을 플레이어가 쏜 것처럼 맞힌다. 반격형은 되쏘고 나머지는 도망간다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGunDayCivilianReactionSubsystem* Subsystem = World ? World->GetSubsystem<UGunDayCivilianReactionSubsystem>() : nullptr)
			{
				const int32 Count = Args.Num() > 0 ? FMath::Max(1, FCString::Atoi(*Args[0])) : 1;
				Subsystem->ProvokeNearest(Count);
			}
		}));

	FString DescribeParams(const UFunction& Function)
	{
		TArray<FString> Parts;
		for (TFieldIterator<FProperty> It(&Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			Parts.Add(FString::Printf(TEXT("%s:%s%s"), *It->GetName(), *It->GetCPPType(), It->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm) ? TEXT("(out)") : TEXT("")));
		}
		return FString::Join(Parts, TEXT(", "));
	}
}

UGunDayCivilianReactionSubsystem* UGunDayCivilianReactionSubsystem::Get(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetSubsystem<UGunDayCivilianReactionSubsystem>();
	}
	return nullptr;
}

bool UGunDayCivilianReactionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}
	return false;
}

void UGunDayCivilianReactionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWorld* World = GetWorld())
	{
		ActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleActorSpawned));

		// -GunDayProvokeTest=초: 그 시간 뒤 가까운 행인 다섯을 맞힌다. 키 입력 없이 반응을 재 보는 테스트용.
		float ProvokeAfter = 0.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("GunDayProvokeTest="), ProvokeAfter) && ProvokeAfter > 0.0f)
		{
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]() { ProvokeNearest(5); }), ProvokeAfter, false);
		}
	}
}

void UGunDayCivilianReactionSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	ActorSpawnedHandle.Reset();
	Decided.Reset();
	Fighters.Reset();
	LoadedWeapons.Reset();

	Super::Deinitialize();
}

bool UGunDayCivilianReactionSubsystem::IsCivilian(const APawn& Pawn) const
{
	const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();
	for (const TSoftClassPtr<APawn>& CivilianClass : Settings->CivilianClasses)
	{
		if (const UClass* Class = CivilianClass.Get())
		{
			if (Pawn.IsA(Class))
			{
				return true;
			}
		}
	}
	return false;
}

float UGunDayCivilianReactionSubsystem::GetFightBackChance() const
{
	const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();
	float JeongFraction = 0.5f;
	if (const UGunDaySocietySubsystem* Society = UGunDaySocietySubsystem::Get(this))
	{
		JeongFraction = Society->GetJeongFraction();
	}
	return FMath::Lerp(Settings->FightBackChanceAtZeroJeong, Settings->FightBackChanceAtHighJeong, FMath::Clamp(JeongFraction, 0.0f, 1.0f));
}

void UGunDayCivilianReactionSubsystem::HandleActorSpawned(AActor* Actor)
{
	APawn* Pawn = Cast<APawn>(Actor);
	UWorld* World = GetWorld();
	if (!Pawn || !World || Pawn->IsPlayerControlled())
	{
		return;
	}

	// 킷이 무기 목록을 세우는 BeginPlay 뒤에 손댄다.
	TWeakObjectPtr<ThisClass> WeakThis(this);
	TWeakObjectPtr<APawn> WeakPawn(Pawn);
	World->GetTimerManager().SetTimerForNextTick([WeakThis, WeakPawn]()
	{
		if (WeakThis.IsValid() && WeakPawn.IsValid())
		{
			WeakThis->DecideRole(WeakPawn.Get());
		}
	});
}

void UGunDayCivilianReactionSubsystem::DecideRole(APawn* Civilian)
{
	if (!Civilian || Decided.Contains(Civilian) || !IsCivilian(*Civilian))
	{
		return;
	}
	Decided.Add(Civilian);

	const float Chance = GetFightBackChance();
	if (FMath::FRand() < Chance)
	{
		MakeFighter(Civilian);
	}
	else
	{
		UE_LOG(LogGunDay, Verbose, TEXT("행인 반응: %s 는 도망형 (반격 확률 %.0f%%)"), *Civilian->GetName(), Chance * 100.0f);
	}
}

bool UGunDayCivilianReactionSubsystem::MakeFighter(APawn* Civilian)
{
	const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();
	if (!Civilian || Settings->FightBackWeapons.Num() == 0)
	{
		return false;
	}

	// 1. 맞으면 전투 모드로. 킷 행인의 설정 변수다.
	FBoolProperty* CombatFlag = FindFProperty<FBoolProperty>(Civilian->GetClass(), Settings->KitCombatOnDamagePropertyName);
	if (!CombatFlag)
	{
		UE_LOG(LogGunDay, Warning, TEXT("행인 반응: %s 에 '%s' 변수가 없다. 세팅의 이름을 확인한다."),
			*Civilian->GetClass()->GetName(), *Settings->KitCombatOnDamagePropertyName.ToString());
		return false;
	}
	CombatFlag->SetPropertyValue_InContainer(Civilian, true);

	// 2. 무기를 쥐여 준다. 킷의 무기 추가 함수를 이름으로 찾아 부른다.
	UObject* Weapon = Settings->FightBackWeapons[FMath::RandHelper(Settings->FightBackWeapons.Num())].LoadSynchronous();
	UFunction* AddWeapon = Civilian->FindFunction(Settings->KitAddWeaponFunctionName);
	if (!Weapon || !AddWeapon)
	{
		UE_LOG(LogGunDay, Warning, TEXT("행인 반응: 무기 %s 또는 함수 '%s' 를 못 찾았다."),
			Weapon ? *Weapon->GetName() : TEXT("없음"), *Settings->KitAddWeaponFunctionName.ToString());
		return false;
	}
	LoadedWeapons.AddUnique(Weapon);

	if (!bLoggedSignature)
	{
		bLoggedSignature = true;
		UE_LOG(LogGunDay, Log, TEXT("행인 반응: 킷 %s(%s)"), *AddWeapon->GetName(), *DescribeParams(*AddWeapon));
	}

	uint8* Params = static_cast<uint8*>(FMemory_Alloca(AddWeapon->ParmsSize));
	FMemory::Memzero(Params, AddWeapon->ParmsSize);
	for (TFieldIterator<FProperty> It(AddWeapon); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->InitializeValue_InContainer(Params);
	}

	bool bWeaponSet = false;
	for (TFieldIterator<FProperty> It(AddWeapon); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
		{
			continue;
		}
		if (FObjectPropertyBase* ObjectParam = CastField<FObjectPropertyBase>(*It))
		{
			if (Weapon->IsA(ObjectParam->PropertyClass))
			{
				ObjectParam->SetObjectPropertyValue_InContainer(Params, Weapon);
				bWeaponSet = true;
				break;
			}
		}
	}

	// 나머지 인자. 이름이 맞는 것만 채운다. 킷이 바뀌어 이름이 없으면 기본값으로 둔다.
	auto SetBool = [AddWeapon, Params](const TCHAR* Name, bool bValue)
	{
		if (FBoolProperty* Property = FindFProperty<FBoolProperty>(AddWeapon, Name))
		{
			Property->SetPropertyValue_InContainer(Params, bValue);
		}
	};
	auto SetInt = [AddWeapon, Params](const TCHAR* Name, int32 Value)
	{
		if (FIntProperty* Property = FindFProperty<FIntProperty>(AddWeapon, Name))
		{
			Property->SetPropertyValue_InContainer(Params, Value);
		}
	};
	// 꺼내 들고, 빈 슬롯을 찾고, 쏠 탄을 준다. 탄이 없으면 킷 AI 는 맨손 근접으로 바꾼다.
	SetBool(TEXT("AutoEquip"), true);
	SetBool(TEXT("SkipEquipAnimation"), true);
	SetBool(TEXT("SkipUnequipAnimation"), true);
	SetBool(TEXT("CheckOtherSlotsAvailability"), true);
	SetBool(TEXT("OverrideAmmoAmount"), true);
	SetInt(TEXT("AmmoAmountOverride"), 36);
	SetBool(TEXT("OverrideAmmoInClip"), true);
	SetInt(TEXT("AmmoAmountInClipOverride"), 6);

	bool bAdded = false;
	if (bWeaponSet)
	{
		Civilian->ProcessEvent(AddWeapon, Params);
		if (FBoolProperty* Result = FindFProperty<FBoolProperty>(AddWeapon, TEXT("WeaponSuccessfullyAdded")))
		{
			bAdded = Result->GetPropertyValue_InContainer(Params);
		}
		else
		{
			bAdded = true;
		}
	}

	for (TFieldIterator<FProperty> It(AddWeapon); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Params);
	}

	if (!bWeaponSet)
	{
		UE_LOG(LogGunDay, Warning, TEXT("행인 반응: '%s' 에 무기 에셋을 넣을 인자가 없다 (%s)."),
			*AddWeapon->GetName(), *DescribeParams(*AddWeapon));
		return false;
	}

	if (!bAdded)
	{
		UE_LOG(LogGunDay, Warning, TEXT("행인 반응: %s 에 %s 를 쥐여 주지 못했다. 맞으면 맨손으로 덤빈다."), *Civilian->GetName(), *Weapon->GetName());
	}

	Fighters.Add(Civilian);
	UE_LOG(LogGunDay, Log, TEXT("행인 반응: %s 는 반격형 (%s, 무기 %s)"), *Civilian->GetName(), *Weapon->GetName(), bAdded ? TEXT("들었다") : TEXT("실패"));
	return true;
}

int32 UGunDayCivilianReactionSubsystem::ProvokeNearest(int32 Count)
{
	UWorld* World = GetWorld();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!World || !Player)
	{
		return 0;
	}

	TArray<APawn*> Civilians;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (IsValid(*It) && *It != Player && IsCivilian(**It))
		{
			Civilians.Add(*It);
		}
	}

	const FVector PlayerLocation = Player->GetActorLocation();
	Civilians.Sort([&PlayerLocation](const APawn& A, const APawn& B)
	{
		return FVector::DistSquared(A.GetActorLocation(), PlayerLocation) < FVector::DistSquared(B.GetActorLocation(), PlayerLocation);
	});

	int32 Hit = 0;
	for (APawn* Civilian : Civilians)
	{
		if (Hit >= Count)
		{
			break;
		}
		const bool bFighter = Fighters.Contains(Civilian);
		UGameplayStatics::ApplyDamage(Civilian, 10.0f, Player->GetController(), Player, UDamageType::StaticClass());
		UE_LOG(LogGunDay, Log, TEXT("행인 반응: 시험 사격 → %s (%s, %.0fm)"),
			*Civilian->GetName(), bFighter ? TEXT("반격형") : TEXT("도망형"),
			FVector::Dist(Civilian->GetActorLocation(), PlayerLocation) / 100.0f);
		++Hit;
	}
	return Hit;
}
