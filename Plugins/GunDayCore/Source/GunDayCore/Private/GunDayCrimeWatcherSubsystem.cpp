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
#include "GunDayDisputeSubsystem.h"
#include "GunDayNewsSubsystem.h"
#include "GunDayPoliceResponseSubsystem.h"
#include "GunDaySocietySubsystem.h"
#include "GunDayWantedSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

void UGunDayKitEventListener::OnKitHit()
{
	// 인자는 ProcessEvent 에서 디스패처의 시그니처대로 읽는다. 여기는 비워 둔다.
}

void UGunDayKitEventListener::OnKitDeath()
{
}

void UGunDayKitEventListener::ProcessEvent(UFunction* Function, void* Parms)
{
	if (Function && Pawn.IsValid() && Watcher.IsValid())
	{
		if (Function->GetFName() == GET_FUNCTION_NAME_CHECKED(UGunDayKitEventListener, OnKitHit))
		{
			Watcher->HandleKitEvent(Pawn.Get(), HitSignature, Parms, false);
			return;
		}

		if (Function->GetFName() == GET_FUNCTION_NAME_CHECKED(UGunDayKitEventListener, OnKitDeath))
		{
			Watcher->HandleKitEvent(Pawn.Get(), DeathSignature, Parms, true);
			return;
		}
	}

	Super::ProcessEvent(Function, Parms);
}

namespace
{
	/** 킷 체력을 읽는 간격(초). 한 발 한 발을 놓치지 않을 만큼 짧게 둔다. */
	constexpr float CrimeHealthPollSeconds = 0.1f;

	/** 디스패처 시그니처를 "이름:타입, ..." 으로 적는다. 로그용. */
	FString DescribeSignature(const UFunction* Signature)
	{
		if (!Signature)
		{
			return TEXT("없음");
		}

		TArray<FString> Parts;
		for (TFieldIterator<FProperty> It(Signature); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			Parts.Add(FString::Printf(TEXT("%s:%s"), *It->GetName(), *It->GetCPPType()));
		}
		return FString::Join(Parts, TEXT(", "));
	}
}

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
	KilledReported.Reset();
	Listeners.Reset();
	LoggedClasses.Reset();
	HealthWatches.Reset();

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

	TimeSinceHealthPoll += DeltaTime;
	if (TimeSinceHealthPoll >= CrimeHealthPollSeconds)
	{
		TimeSinceHealthPoll = 0.0f;
		PollKitHealth();
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

	Listeners.RemoveAll([](const UGunDayKitEventListener* Listener)
	{
		return !Listener || !Listener->Pawn.IsValid();
	});

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

		// 킷 캐릭터는 엔진 피해 이벤트 대신 자체 디스패처로 맞고 죽는다. 그쪽도 듣는다.
		BindKitEvents(*Pawn);

		UE_LOG(LogGunDay, VeryVerbose, TEXT("감시 시작: %s"), *Pawn->GetName());
	}
}

FMulticastDelegateProperty* UGunDayCrimeWatcherSubsystem::FindKitDispatcher(APawn& Pawn, const TArray<FName>& Names, UObject*& OutOwner)
{
	for (const FName& Name : Names)
	{
		if (FMulticastDelegateProperty* Property = FindFProperty<FMulticastDelegateProperty>(Pawn.GetClass(), Name))
		{
			OutOwner = &Pawn;
			return Property;
		}
	}

	// 체력이나 피격 반응 컴포넌트에 디스패처를 두는 킷도 있다.
	TInlineComponentArray<UActorComponent*> Components(&Pawn);
	for (const FName& Name : Names)
	{
		for (UActorComponent* Component : Components)
		{
			if (FMulticastDelegateProperty* Property = Component ? FindFProperty<FMulticastDelegateProperty>(Component->GetClass(), Name) : nullptr)
			{
				OutOwner = Component;
				return Property;
			}
		}
	}

	return nullptr;
}

void UGunDayCrimeWatcherSubsystem::BindKitEvents(APawn& Pawn)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	UObject* HitOwner = nullptr;
	UObject* DeathOwner = nullptr;
	FMulticastDelegateProperty* HitDispatcher = FindKitDispatcher(Pawn, Settings->KitHitEventNames, HitOwner);
	FMulticastDelegateProperty* DeathDispatcher = FindKitDispatcher(Pawn, Settings->KitDeathEventNames, DeathOwner);

	// 클래스마다 처음 한 번 무엇을 찾았는지 남긴다. 킷이 바뀌었을 때 여기부터 본다.
	if (!LoggedClasses.Contains(Pawn.GetClass()))
	{
		LoggedClasses.Add(Pawn.GetClass());
		UE_LOG(LogGunDay, Log, TEXT("킷 디스패처: %s — 맞음 %s(%s), 죽음 %s(%s)"),
			*Pawn.GetClass()->GetName(),
			HitDispatcher ? *HitDispatcher->GetName() : TEXT("없음"),
			*DescribeSignature(HitDispatcher ? HitDispatcher->SignatureFunction : nullptr),
			DeathDispatcher ? *DeathDispatcher->GetName() : TEXT("없음"),
			*DescribeSignature(DeathDispatcher ? DeathDispatcher->SignatureFunction : nullptr));
		LogKitClassLayout(Pawn);
	}

	// 맞음 디스패처가 없을 때만 체력을 지켜본다. 체력에는 누가 쐈는지가 없어 어림이 섞인다.
	if (!HitDispatcher)
	{
		WatchKitHealth(Pawn);
	}

	if (!HitDispatcher && !DeathDispatcher)
	{
		return;
	}

	UGunDayKitEventListener* Listener = NewObject<UGunDayKitEventListener>(this);
	Listener->Pawn = &Pawn;
	Listener->Watcher = this;

	if (HitDispatcher)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(Listener, GET_FUNCTION_NAME_CHECKED(UGunDayKitEventListener, OnKitHit));
		HitDispatcher->AddDelegate(MoveTemp(Delegate), HitOwner);
		Listener->HitSignature = HitDispatcher->SignatureFunction;
	}

	if (DeathDispatcher)
	{
		FScriptDelegate Delegate;
		Delegate.BindUFunction(Listener, GET_FUNCTION_NAME_CHECKED(UGunDayKitEventListener, OnKitDeath));
		DeathDispatcher->AddDelegate(MoveTemp(Delegate), DeathOwner);
		Listener->DeathSignature = DeathDispatcher->SignatureFunction;
	}

	Listeners.Add(Listener);
}

void UGunDayCrimeWatcherSubsystem::LogKitClassLayout(APawn& Pawn)
{
	// 디스패처 전부와, 이름에 체력·죽음·피해가 들어간 변수를 적는다.
	auto Describe = [](const UStruct* Type) -> FString
	{
		static const TCHAR* Keys[] = { TEXT("health"), TEXT("dead"), TEXT("die"), TEXT("death"), TEXT("damage"), TEXT("hit"), TEXT("kill") };

		TArray<FString> Parts;
		for (TFieldIterator<FProperty> It(Type); It; ++It)
		{
			const FString Name = It->GetName();
			if (const FMulticastDelegateProperty* Dispatcher = CastField<FMulticastDelegateProperty>(*It))
			{
				Parts.Add(FString::Printf(TEXT("[디스패처] %s(%s)"), *Name, *DescribeSignature(Dispatcher->SignatureFunction)));
				continue;
			}

			for (const TCHAR* Key : Keys)
			{
				if (Name.Contains(Key))
				{
					Parts.Add(FString::Printf(TEXT("%s:%s"), *Name, *It->GetCPPType()));
					break;
				}
			}
		}
		return FString::Join(Parts, TEXT(", "));
	};

	UE_LOG(LogGunDay, Log, TEXT("킷 구성: %s — %s"), *Pawn.GetClass()->GetName(), *Describe(Pawn.GetClass()));

	TInlineComponentArray<UActorComponent*> Components(&Pawn);
	for (const UActorComponent* Component : Components)
	{
		// 엔진 컴포넌트는 볼 것이 없다. 블루프린트로 만든 컴포넌트만 적는다.
		if (Component && Component->GetClass()->ClassGeneratedBy)
		{
			UE_LOG(LogGunDay, Log, TEXT("킷 구성:   └ %s (%s) — %s"),
				*Component->GetName(), *Component->GetClass()->GetName(), *Describe(Component->GetClass()));
		}
	}
}

void UGunDayCrimeWatcherSubsystem::WatchKitHealth(APawn& Pawn)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!Settings)
	{
		return;
	}

	auto FindIn = [Settings](UObject* Object) -> const FNumericProperty*
	{
		for (const FName& Name : Settings->KitHealthPropertyNames)
		{
			if (const FNumericProperty* Property = FindFProperty<FNumericProperty>(Object->GetClass(), Name))
			{
				return Property;
			}
		}
		return nullptr;
	};

	UObject* Owner = &Pawn;
	const FNumericProperty* Property = FindIn(&Pawn);

	if (!Property)
	{
		TInlineComponentArray<UActorComponent*> Components(&Pawn);
		for (UActorComponent* Component : Components)
		{
			if (Component && (Property = FindIn(Component)) != nullptr)
			{
				Owner = Component;
				break;
			}
		}
	}

	if (!Property)
	{
		return;
	}

	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	FHealthWatch Watch;
	Watch.Pawn = &Pawn;
	Watch.Owner = Owner;
	Watch.Property = Property;
	Watch.LastValue = Property->IsFloatingPoint()
		? Property->GetFloatingPointPropertyValue(ValuePtr)
		: static_cast<double>(Property->GetSignedIntPropertyValue(ValuePtr));
	HealthWatches.Add(Watch);
}

void UGunDayCrimeWatcherSubsystem::PollKitHealth()
{
	const UGunDayCoreSettings* Settings = GetSettings();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const UGunDayDisputeSubsystem* Disputes = GetWorld() ? GetWorld()->GetSubsystem<UGunDayDisputeSubsystem>() : nullptr;
	if (!Settings || !PlayerPawn)
	{
		return;
	}

	const float AttributionRadiusSquared = Settings->KitDamageAttributionRadius * Settings->KitDamageAttributionRadius;

	for (int32 Index = HealthWatches.Num() - 1; Index >= 0; --Index)
	{
		FHealthWatch& Watch = HealthWatches[Index];
		APawn* Pawn = Watch.Pawn.Get();
		UObject* Owner = Watch.Owner.Get();
		if (!IsValid(Pawn) || !IsValid(Owner))
		{
			HealthWatches.RemoveAt(Index);
			continue;
		}

		const void* ValuePtr = Watch.Property->ContainerPtrToValuePtr<void>(Owner);
		const double Value = Watch.Property->IsFloatingPoint()
			? Watch.Property->GetFloatingPointPropertyValue(ValuePtr)
			: static_cast<double>(Watch.Property->GetSignedIntPropertyValue(ValuePtr));

		const double Previous = Watch.LastValue;
		Watch.LastValue = Value;

		if (Value >= Previous - KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// 체력에는 누가 쐈는지가 없다. 시비 총격이 아니고 플레이어 근처에서 일어났으면 플레이어로 본다.
		const bool bDisputeShot = Disputes && Disputes->WasShotInDisputeRecently(Pawn, 1.0f);
		const bool bNearPlayer = FVector::DistSquared(Pawn->GetActorLocation(), PlayerPawn->GetActorLocation()) <= AttributionRadiusSquared;
		const bool bByPlayer = !bDisputeShot && bNearPlayer;
		const bool bDead = Value <= 0.0;

		UE_LOG(LogGunDay, Verbose, TEXT("체력: %s %.0f -> %.0f%s 플레이어=%s"),
			*Pawn->GetName(), Previous, Value, bDead ? TEXT(" (쓰러짐)") : TEXT(""),
			bByPlayer ? TEXT("예") : (bDisputeShot ? TEXT("아니오(시비)") : TEXT("아니오(멂)")));

		if (bByPlayer)
		{
			ReportInjury(*Pawn);
		}

		if (bDead && (bByPlayer || WoundedByPlayer.Contains(Pawn)))
		{
			ReportKill(*Pawn);
		}

		// 쓰러진 사람은 더 볼 일이 없다.
		if (bDead)
		{
			HealthWatches.RemoveAt(Index);
		}
	}
}

void UGunDayCrimeWatcherSubsystem::HandleKitEvent(APawn* Pawn, const UFunction* Signature, void* Parms, bool bDeath)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	if (!IsValid(Pawn) || !Settings || !bWatchEnabled || !Settings->bAutoReportCrimes)
	{
		return;
	}

	// 인자 중에 플레이어 쪽 객체가 있으면 플레이어가 한 짓이다.
	bool bByPlayer = false;
	TArray<FString> Dump;

	if (Signature && Parms)
	{
		for (TFieldIterator<FProperty> It(Signature); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			const void* ValuePtr = It->ContainerPtrToValuePtr<void>(Parms);

			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(*It))
			{
				const UObject* Value = ObjectProperty->GetObjectPropertyValue(ValuePtr);
				bByPlayer |= IsPlayerSide(Value);
				Dump.Add(FString::Printf(TEXT("%s=%s"), *It->GetName(), Value ? *Value->GetName() : TEXT("없음")));
				continue;
			}

			FString Text;
			It->ExportTextItem_Direct(Text, ValuePtr, nullptr, nullptr, PPF_None);
			Dump.Add(FString::Printf(TEXT("%s=%s"), *It->GetName(), *Text.Left(40)));
		}
	}

	UE_LOG(LogGunDay, Verbose, TEXT("킷 %s: %s (%s) 플레이어=%s"),
		bDeath ? TEXT("죽음") : TEXT("맞음"), *Pawn->GetName(), *FString::Join(Dump, TEXT(", ")),
		bByPlayer ? TEXT("예") : TEXT("아니오"));

	if (bDeath)
	{
		if (bByPlayer || WoundedByPlayer.Contains(Pawn))
		{
			ReportKill(*Pawn);
		}
		return;
	}

	if (bByPlayer)
	{
		ReportInjury(*Pawn);
	}
}

void UGunDayCrimeWatcherSubsystem::ReportInjury(AActor& Victim)
{
	const UGunDayCoreSettings* Settings = GetSettings();
	UGunDayWantedSubsystem* Wanted = GetWantedSubsystem();
	if (!Settings || !Wanted)
	{
		return;
	}

	// 한 발 한 발을 전부 세면 연사 한 번에 수배가 최고까지 오른다.
	const TWeakObjectPtr<AActor> Key(&Victim);
	WoundedByPlayer.Add(Key);

	if (const float* LastTime = LastInjuryReportTime.Find(Key))
	{
		if (ElapsedSeconds - *LastTime < Settings->InjuryReportCooldownSeconds)
		{
			return;
		}
	}

	LastInjuryReportTime.Add(Key, ElapsedSeconds);

	// 맞은 자리에서 총성이 난 것으로 치고 주변 시민을 흩어지게 한다.
	if (UGunDayCrowdSubsystem* Crowd = GetWorld() ? GetWorld()->GetSubsystem<UGunDayCrowdSubsystem>() : nullptr)
	{
		Crowd->NotifyGunshot(Victim.GetActorLocation());
	}

	Wanted->ReportCrime(IsPoliceActor(Victim) ? EGunDayCrime::PoliceInjured : EGunDayCrime::CivilianInjured);
}

void UGunDayCrimeWatcherSubsystem::ReportKill(AActor& Victim)
{
	UGunDayWantedSubsystem* Wanted = GetWantedSubsystem();
	const TWeakObjectPtr<AActor> Key(&Victim);
	if (!Wanted || KilledReported.Contains(Key))
	{
		return;
	}

	KilledReported.Add(Key);
	Wanted->ReportCrime(IsPoliceActor(Victim) ? EGunDayCrime::PoliceKilled : EGunDayCrime::CivilianKilled);

	// 사람이 죽을 때마다 사회의 온도가 내려간다.
	const UGunDayCoreSettings* Settings = GetSettings();
	UGunDaySocietySubsystem* Society = GetWorld() ? GetWorld()->GetSubsystem<UGunDaySocietySubsystem>() : nullptr;
	if (Settings && Society)
	{
		Society->AddJeong(Settings->JeongOnPlayerKill);
	}

	if (UGunDayNewsSubsystem* News = GetWorld() ? GetWorld()->GetSubsystem<UGunDayNewsSubsystem>() : nullptr)
	{
		News->ReportShooting(true, true);
	}
}

bool UGunDayCrimeWatcherSubsystem::IsPlayerSide(const UObject* Object) const
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Object || !PlayerPawn)
	{
		return false;
	}

	if (Object == PlayerPawn || Object == PlayerPawn->GetController())
	{
		return true;
	}

	// 컴포넌트면 그 주인을 본다.
	const AActor* Actor = Cast<AActor>(Object);
	if (!Actor)
	{
		if (const UActorComponent* Component = Cast<UActorComponent>(Object))
		{
			Actor = Component->GetOwner();
		}
	}

	// 무기, 투사체처럼 플레이어가 가진 것이면 플레이어 쪽이다. 몇 단계까지 따라간다.
	for (int32 Depth = 0; Actor && Depth < 4; ++Depth)
	{
		if (Actor == PlayerPawn || Actor->GetInstigator() == PlayerPawn)
		{
			return true;
		}
		Actor = Actor->GetOwner();
	}

	return false;
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

	// 신고가 안 들어갈 때 어디서 끊기는지 보려고 남긴다. log LogGunDay Verbose 로 켠다.
	UE_LOG(LogGunDay, Verbose, TEXT("피해: %s %.0f (가해 %s, 원인 %s)"),
		*DamagedActor->GetName(), Damage,
		InstigatedBy ? *InstigatedBy->GetName() : TEXT("없음"),
		DamageCauser ? *DamageCauser->GetName() : TEXT("없음"));

	if (!IsPlayerInstigator(InstigatedBy, DamageCauser))
	{
		UE_LOG(LogGunDay, Verbose, TEXT("피해: 플레이어가 준 피해로 보지 않았다."));
		return;
	}

	ReportInjury(*DamagedActor);
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
	// 킷 디스패처로 이미 사망을 신고했으면 ReportKill 이 거른다.
	if (WoundedByPlayer.Remove(Key) > 0)
	{
		ReportKill(*DestroyedActor);
	}

	KilledReported.Remove(Key);
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

	// 투사체처럼 컨트롤러가 비어 오는 경우가 있다. 피해를 준 액터 쪽도 따라가 본다.
	return IsPlayerSide(InstigatedBy) || IsPlayerSide(DamageCauser);
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
