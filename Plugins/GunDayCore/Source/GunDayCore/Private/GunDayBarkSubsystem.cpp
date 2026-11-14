// Copyright K-GTA. All Rights Reserved.

#include "GunDayBarkSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GunDayAppearanceSubsystem.h"
#include "GunDayCivilianReactionSubsystem.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDayCrowdSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UGunDayBarkSubsystem* UGunDayBarkSubsystem::Get(const UObject* WorldContextObject)
{
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetSubsystem<UGunDayBarkSubsystem>();
	}
	return nullptr;
}

bool UGunDayBarkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}
	return false;
}

void UGunDayBarkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWorld* World = GetWorld())
	{
		ActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleActorSpawned));
	}
}

void UGunDayBarkSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
		World->GetTimerManager().ClearTimer(ChatterTimer);
	}
	LastBarkByPawn.Reset();
	SpottedPolice.Reset();
	LoadedSounds.Reset();
	Attenuation = nullptr;

	Super::Deinitialize();
}

void UGunDayBarkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UGunDayCrowdSubsystem* Crowd = InWorld.GetSubsystem<UGunDayCrowdSubsystem>())
	{
		Crowd->OnCivilianAlerted.AddUniqueDynamic(this, &ThisClass::HandleCivilianAlerted);
	}

	const float Interval = GetDefault<UGunDayCoreSettings>()->BarkPoliceChatterSeconds;
	InWorld.GetTimerManager().SetTimer(ChatterTimer, FTimerDelegate::CreateUObject(this, &ThisClass::PoliceChatter), Interval, true);
}

bool UGunDayBarkSubsystem::IsVeteran(const APawn* Pawn) const
{
	const UGunDayAppearanceSubsystem* Appearance = GetWorld() ? GetWorld()->GetSubsystem<UGunDayAppearanceSubsystem>() : nullptr;
	const FName VeteranName = GetDefault<UGunDayCoreSettings>()->VeteranAppearanceName;
	return Appearance && !VeteranName.IsNone() && Appearance->GetAppearanceName(Pawn) == VeteranName;
}

const TArray<TSoftObjectPtr<USoundBase>>& UGunDayBarkSubsystem::GetList(EGunDayBark Kind, const APawn* Speaker) const
{
	const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();
	const TArray<TSoftObjectPtr<USoundBase>>* List = nullptr;

	// 두 번째 형사는 목소리가 다르다. 같은 상황에서 고참 목록을 먼저 본다.
	if (IsVeteran(Speaker))
	{
		switch (Kind)
		{
		case EGunDayBark::PoliceArrive: List = &Settings->BarkVeteranArrive; break;
		case EGunDayBark::PoliceSpot:   List = &Settings->BarkVeteranSpot; break;
		case EGunDayBark::PoliceCombat: List = &Settings->BarkVeteranCombat; break;
		case EGunDayBark::PoliceHit:    List = &Settings->BarkVeteranHit; break;
		case EGunDayBark::PoliceDown:   List = &Settings->BarkVeteranDown; break;
		default: break;
		}
		if (List && List->Num() > 0)
		{
			return *List;
		}
		List = nullptr;
	}

	switch (Kind)
	{
	case EGunDayBark::PoliceArrive:   List = &Settings->BarkPoliceArrive; break;
	case EGunDayBark::PoliceSpot:     List = &Settings->BarkPoliceSpot; break;
	case EGunDayBark::PoliceCombat:   List = &Settings->BarkPoliceCombat; break;
	case EGunDayBark::PoliceHit:      List = &Settings->BarkPoliceHit; break;
	case EGunDayBark::PoliceDown:     List = &Settings->BarkPoliceDown; break;
	case EGunDayBark::CivilianFemale: List = &Settings->BarkCivilianFemale; break;
	case EGunDayBark::CivilianFight:  List = &Settings->BarkCivilianFight; break;
	case EGunDayBark::DisputeMale:    List = &Settings->BarkDisputeMale; break;
	default: break;
	}
	return (List && List->Num() > 0) ? *List : Settings->BarkSounds;
}

bool UGunDayBarkSubsystem::IsPolice(const APawn& Pawn) const
{
	const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();
	for (const FGunDayResponseTier& Tier : Settings->ResponseTiers)
	{
		if (const UClass* Class = Tier.ResponderClass.Get())
		{
			if (Pawn.IsA(Class))
			{
				return true;
			}
		}
	}
	for (const TSoftClassPtr<AActor>& PoliceClass : Settings->PoliceClasses)
	{
		if (const UClass* Class = PoliceClass.Get())
		{
			if (Pawn.IsA(Class))
			{
				return true;
			}
		}
	}
	return false;
}

bool UGunDayBarkSubsystem::IsFemale(const APawn* Pawn) const
{
	const UGunDayAppearanceSubsystem* Appearance = GetWorld() ? GetWorld()->GetSubsystem<UGunDayAppearanceSubsystem>() : nullptr;
	return Appearance && Appearance->IsFemale(Pawn);
}

bool UGunDayBarkSubsystem::IsFighter(const APawn* Pawn) const
{
	const UGunDayCivilianReactionSubsystem* Reaction = GetWorld() ? GetWorld()->GetSubsystem<UGunDayCivilianReactionSubsystem>() : nullptr;
	return Reaction && Reaction->IsFighter(Pawn);
}

USoundAttenuation* UGunDayBarkSubsystem::GetAttenuation()
{
	if (!Attenuation)
	{
		const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();
		Attenuation = NewObject<USoundAttenuation>(this);
		FSoundAttenuationSettings& Attenuate = Attenuation->Attenuation;
		Attenuate.bAttenuate = true;
		Attenuate.bSpatialize = true;
		Attenuate.AttenuationShape = EAttenuationShape::Sphere;
		Attenuate.AttenuationShapeExtents = FVector(Settings->BarkInnerRadius, 0.0f, 0.0f);
		Attenuate.FalloffDistance = Settings->BarkFalloffDistance;
		Attenuate.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	}
	return Attenuation;
}

bool UGunDayBarkSubsystem::TryBark(APawn* Speaker, EGunDayBark Kind, float Chance)
{
	const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();
	UWorld* World = GetWorld();
	if (!IsValid(Speaker) || !World || Speaker->IsPlayerControlled())
	{
		return false;
	}

	const TArray<TSoftObjectPtr<USoundBase>>& List = GetList(Kind, Speaker);
	if (List.Num() == 0)
	{
		return false;
	}

	const double Now = World->GetTimeSeconds();
	if (Now - LastBarkSeconds < Settings->BarkGlobalCooldown)
	{
		return false;
	}
	if (const double* Last = LastBarkByPawn.Find(Speaker))
	{
		if (Now - *Last < Settings->BarkCooldownPerPawn)
		{
			return false;
		}
	}
	if (FMath::FRand() > Chance)
	{
		return false;
	}

	// 바로 앞에 나온 말은 연달아 고르지 않는다.
	const int32 Count = List.Num();
	int32& LastIndex = LastIndexByKind.FindOrAdd(Kind, INDEX_NONE);
	int32 Index = FMath::RandHelper(Count);
	if (Count > 1 && Index == LastIndex)
	{
		Index = (Index + 1 + FMath::RandHelper(Count - 1)) % Count;
	}

	USoundBase* Sound = List[Index].LoadSynchronous();
	if (!Sound)
	{
		return false;
	}
	LoadedSounds.AddUnique(Sound);

	UGameplayStatics::SpawnSoundAttached(Sound, Speaker->GetRootComponent(), NAME_None, FVector(0.0f, 0.0f, 60.0f),
		EAttachLocation::KeepRelativeOffset, true, Settings->BarkVolume, FMath::FRandRange(0.97f, 1.03f), 0.0f, GetAttenuation());

	LastBarkSeconds = Now;
	LastBarkByPawn.Add(Speaker, Now);
	LastIndex = Index;
	UE_LOG(LogGunDay, Verbose, TEXT("외침: %s 가 '%s' (%s)"), *Speaker->GetName(), *Sound->GetName(), *UEnum::GetValueAsString(Kind));
	return true;
}

void UGunDayBarkSubsystem::BarkOnHit(APawn* Victim)
{
	if (!IsValid(Victim))
	{
		return;
	}

	const float Chance = GetDefault<UGunDayCoreSettings>()->BarkChanceOnHit;
	if (IsPolice(*Victim))
	{
		TryBark(Victim, EGunDayBark::PoliceHit, Chance);
	}
	else if (IsFemale(Victim))
	{
		TryBark(Victim, EGunDayBark::CivilianFemale, Chance);
	}
	else if (IsFighter(Victim))
	{
		TryBark(Victim, EGunDayBark::CivilianFight, Chance);
	}
	else
	{
		TryBark(Victim, EGunDayBark::Generic, Chance);
	}
}

void UGunDayBarkSubsystem::BarkOnDeath(APawn* Victim)
{
	UWorld* World = GetWorld();
	if (!IsValid(Victim) || !World || !IsPolice(*Victim))
	{
		return;
	}

	// 가장 가까운 동료가 보고서 걱정을 한다.
	APawn* Nearest = nullptr;
	float NearestDistSq = FMath::Square(GetDefault<UGunDayCoreSettings>()->BarkFalloffDistance);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		if (Pawn == Victim || !IsValid(Pawn) || !IsPolice(*Pawn))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), Victim->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Pawn;
		}
	}
	if (Nearest)
	{
		TryBark(Nearest, EGunDayBark::PoliceDown, 1.0f);
	}
}

void UGunDayBarkSubsystem::BarkOnDisputeLine(APawn* Speaker, float Chance)
{
	if (!IsValid(Speaker))
	{
		return;
	}

	if (IsFemale(Speaker))
	{
		TryBark(Speaker, EGunDayBark::CivilianFemale, Chance);
		return;
	}

	// 남자는 반말 대사와 공용 외침을 섞는다.
	TryBark(Speaker, FMath::FRand() < 0.6f ? EGunDayBark::DisputeMale : EGunDayBark::Generic, Chance);
}

void UGunDayBarkSubsystem::HandleCivilianAlerted(APawn* Civilian, FVector NoiseLocation)
{
	if (!IsValid(Civilian) || IsFighter(Civilian))
	{
		return;
	}

	const float Chance = GetDefault<UGunDayCoreSettings>()->BarkChanceOnAlert;
	// 여자는 아줌마 목소리로 비명. 남자 도망형은 공용 외침을 가끔.
	if (IsFemale(Civilian))
	{
		TryBark(Civilian, EGunDayBark::CivilianFemale, Chance);
	}
	else
	{
		TryBark(Civilian, EGunDayBark::Generic, Chance * 0.5f);
	}
}

void UGunDayBarkSubsystem::HandleActorSpawned(AActor* Actor)
{
	APawn* Pawn = Cast<APawn>(Actor);
	UWorld* World = GetWorld();
	if (!Pawn || !World || !IsPolice(*Pawn))
	{
		return;
	}

	// 현장에 도착한 경찰은 조금 걸어 들어온 뒤 신고 접수를 알린다.
	TWeakObjectPtr<ThisClass> WeakThis(this);
	TWeakObjectPtr<APawn> WeakPawn(Pawn);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [WeakThis, WeakPawn]()
	{
		// 막힌 경찰을 다시 투입할 때도 새로 생긴다. 도착 인사는 골목 전체로 간격을 둔다.
		ThisClass* Self = WeakThis.Get();
		UWorld* SelfWorld = Self ? Self->GetWorld() : nullptr;
		if (Self && SelfWorld && WeakPawn.IsValid()
			&& SelfWorld->GetTimeSeconds() - Self->LastArriveSeconds >= GetDefault<UGunDayCoreSettings>()->BarkPoliceArriveCooldown
			&& Self->TryBark(WeakPawn.Get(), EGunDayBark::PoliceArrive, 0.7f))
		{
			Self->LastArriveSeconds = SelfWorld->GetTimeSeconds();
		}
	}), 1.5f, false);
}

void UGunDayBarkSubsystem::PoliceChatter()
{
	UWorld* World = GetWorld();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!World || !Player)
	{
		return;
	}

	const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();
	const float SpotDistSq = FMath::Square(Settings->BarkPoliceSpotDistance);
	const float ChatterDistSq = FMath::Square(Settings->BarkInnerRadius + Settings->BarkFalloffDistance);

	TArray<APawn*> NearPolice;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		if (!IsValid(Pawn) || Pawn == Player || !IsPolice(*Pawn))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), Player->GetActorLocation());
		// 처음 가까이 본 경찰은 "저기 있다!". 경찰은 계속 새로 투입되니 골목 전체로 간격을 둔다.
		// 간격 안에 들어온 경찰은 외치지 않고 본 것으로만 친다.
		if (DistSq < SpotDistSq && !SpottedPolice.Contains(Pawn))
		{
			SpottedPolice.Add(Pawn);
			if (World->GetTimeSeconds() - LastSpotSeconds >= Settings->BarkPoliceSpotCooldown
				&& TryBark(Pawn, EGunDayBark::PoliceSpot, 1.0f))
			{
				LastSpotSeconds = World->GetTimeSeconds();
				return;
			}
		}
		if (DistSq < ChatterDistSq)
		{
			NearPolice.Add(Pawn);
		}
	}

	// 나머지는 가끔 민원 창구 말투로 한마디.
	if (NearPolice.Num() > 0)
	{
		TryBark(NearPolice[FMath::RandHelper(NearPolice.Num())], EGunDayBark::PoliceCombat, Settings->BarkPoliceChatterChance);
	}
}
