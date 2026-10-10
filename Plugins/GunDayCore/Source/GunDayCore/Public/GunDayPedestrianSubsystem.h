// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GunDayPedestrianSubsystem.generated.h"

/** 행인 한 명의 상태. */
USTRUCT()
struct FGunDayPedestrian
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<APawn> Pawn;

	/** 이 시각이 지나면 다음 걸음을 뗀다. */
	float NextMoveAtSeconds = 0.0f;

	/** 지난번에 봤을 때 걷고 있었는가. 멈춘 순간을 잡아 쉬는 시간을 준다. */
	bool bWasMoving = false;
};

/**
 * 행인.
 *
 * 플레이어 주변에 정해진 수의 시민을 유지한다. 시야 밖에서 나타나고,
 * 멀어지면 사라지고, 그 사이에는 근처를 걷다 서다 한다.
 *
 * 이 사람들이 총성에 흩어지고 신고하는 목격자가 된다(UGunDayCrowdSubsystem).
 * 시비 지점이 없는 맵에서는 이 중 둘이 시비에 끌려 들어가기도 한다.
 *
 * 킷의 civilian 프리셋을 그대로 쓴다. 새 AI 를 만들지 않는다.
 * 놀라 달아나는 중이거나 시비에 끼어 있는 동안은 걷게 하지 않는다.
 */
UCLASS(BlueprintType)
class GUNDAYCORE_API UGunDayPedestrianSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GunDay|Pedestrian", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Pedestrian Subsystem"))
	static UGunDayPedestrianSubsystem* Get(const UObject* WorldContextObject);

	// ~ USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	// ~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return false; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;

	/** 지금 유지 중인 행인 수. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Pedestrian")
	int32 GetPedestrianCount() const { return Pedestrians.Num(); }

	/** 행인을 전부 치운다. 켜져 있으면 다시 채운다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Pedestrian")
	void ClearPedestrians();

	UFUNCTION(BlueprintCallable, Category = "GunDay|Pedestrian")
	void SetPedestriansEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "GunDay|Pedestrian")
	bool ArePedestriansEnabled() const { return bPedestriansEnabled; }

private:
	/** 죽었거나 너무 멀어진 행인을 명단에서 뺀다. */
	void PrunePedestrians(const FVector& PlayerLocation);

	/** 한 명을 시야 밖에 세운다. */
	bool TrySpawnPedestrian(const APawn& Player);

	/** 서 있는 행인에게 다음 걸음을 준다. */
	void DriveWander();

	/** 그 자리가 지금 플레이어 눈에 보이는가. */
	bool IsPointInView(const FVector& Point) const;

	const class UGunDayCoreSettings* GetSettings() const;

	UPROPERTY()
	TArray<FGunDayPedestrian> Pedestrians;

	float ElapsedSeconds = 0.0f;

	float TimeSinceSpawn = 0.0f;

	float TimeSinceWander = 0.0f;

	bool bPedestriansEnabled = true;
};
