// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GunDayBarkSubsystem.generated.h"

class APawn;
class USoundAttenuation;
class USoundBase;

/** 외침의 종류. 종류마다 소리 목록이 따로 있다. 목록이 비면 공용 외침을 쓴다. */
UENUM(BlueprintType)
enum class EGunDayBark : uint8
{
	Generic,
	PoliceArrive,
	PoliceSpot,
	PoliceCombat,
	PoliceHit,
	PoliceDown,
	CivilianFemale,
	CivilianFight,
	DisputeMale,
};

/**
 * 외침.
 *
 * 행인과 형사가 맞거나, 놀라거나, 시비가 격해질 때 짧은 한마디를 내뱉는다.
 * 경찰은 민원 창구 말투로 총을 쏜다. "저는 전달만 합니다."
 *
 *   경찰  — 나타날 때 / 플레이어를 처음 가까이 볼 때 / 교전 중 가끔 / 맞을 때 / 동료가 쓰러질 때
 *   행인  — 여자는 아줌마 목소리, 남자는 반격할 때 남자 목소리, 시비에서는 반말
 *
 * 말한 사람 몸에 붙인 3D 소리다. 같은 사람은 잠시 쉬고, 골목 전체로도 간격을 둔다.
 * 바로 앞에 나온 말은 연달아 고르지 않는다. 설정은 프로젝트 세팅 > GunDay Core > 외침.
 */
UCLASS()
class GUNDAYCORE_API UGunDayBarkSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GunDay|Bark", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Bark Subsystem"))
	static UGunDayBarkSubsystem* Get(const UObject* WorldContextObject);

	// ~ USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ~ UWorldSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Chance 확률로 이 사람이 Kind 종류의 한마디를 한다. 간격 안이면 말하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Bark")
	bool TryBark(APawn* Speaker, EGunDayBark Kind, float Chance = 1.0f);

	/** 맞은 사람이 한마디. 경찰·반격형·도망형·성별에 맞춰 고른다. */
	void BarkOnHit(APawn* Victim);

	/** 쓰러진 사람이 경찰이면 가까운 동료가 한마디. */
	void BarkOnDeath(APawn* Victim);

	/** 시비에서 반말로 넘어간 사람이 한마디. */
	void BarkOnDisputeLine(APawn* Speaker, float Chance);

	bool IsPolice(const APawn& Pawn) const;

private:
	UFUNCTION()
	void HandleCivilianAlerted(APawn* Civilian, FVector NoiseLocation);

	void HandleActorSpawned(AActor* Actor);
	void PoliceChatter();

	const TArray<TSoftObjectPtr<USoundBase>>& GetList(EGunDayBark Kind, const APawn* Speaker) const;
	bool IsVeteran(const APawn* Pawn) const;
	bool IsFemale(const APawn* Pawn) const;
	bool IsFighter(const APawn* Pawn) const;
	USoundAttenuation* GetAttenuation();

	UPROPERTY()
	TObjectPtr<USoundAttenuation> Attenuation;

	/** 불러 둔 소리. GC 가 치우지 않게 붙잡아 둔다. */
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> LoadedSounds;

	TMap<TWeakObjectPtr<APawn>, double> LastBarkByPawn;
	TSet<TWeakObjectPtr<APawn>> SpottedPolice;
	TMap<EGunDayBark, int32> LastIndexByKind;
	double LastBarkSeconds = -1000.0;
	double LastSpotSeconds = -1000.0;
	double LastArriveSeconds = -1000.0;

	FDelegateHandle ActorSpawnedHandle;
	FTimerHandle ChatterTimer;
};
