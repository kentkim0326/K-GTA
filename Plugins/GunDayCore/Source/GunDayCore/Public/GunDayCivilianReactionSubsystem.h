// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GunDayCivilianReactionSubsystem.generated.h"

class APawn;

/**
 * 맞은 행인의 반응.
 *
 * 누구나 총을 가졌다. 그러니 쏘면 되쏘는 사람이 있다.
 * 행인이 생길 때 정(情)에 따라 반격형인지 정한다. 정이 낮을수록 반격형이 많다.
 *
 *   반격형 — 킷의 '맞으면 전투 모드'를 켜고 권총을 쥐여 둔다. 맞는 순간 킷 전투 AI 로
 *            바뀌어 플레이어를 노린다. 그 전까지는 그냥 걷는 행인이다.
 *   도망형 — 손대지 않는다. 총성이 나면 군중 반응(UGunDayCrowdSubsystem)으로 흩어진다.
 *
 * 킷 블루프린트는 고치지 않는다. 변수와 함수 이름은 프로젝트 세팅 > GunDay Core > 행인 반응.
 * 콘솔: GunDay.Civilians.Provoke [n] — 가까운 행인 n 명을 플레이어가 쏜 것처럼 맞힌다.
 */
UCLASS()
class GUNDAYCORE_API UGunDayCivilianReactionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GunDay|Civilians", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Civilian Reaction Subsystem"))
	static UGunDayCivilianReactionSubsystem* Get(const UObject* WorldContextObject);

	// ~ USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 이 행인을 반격형으로 만든다. 킷 변수를 켜고 무기를 쥐여 준다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Civilians")
	bool MakeFighter(APawn* Civilian);

	/** 플레이어 가까이의 행인 Count 명을 플레이어가 쏜 것처럼 맞힌다. 반응을 확인하는 용도. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Civilians")
	int32 ProvokeNearest(int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "GunDay|Civilians")
	int32 GetFighterCount() const { return Fighters.Num(); }

	/** 반격형인가. 반격형은 총성에 흩어지지 않고 킷 전투 AI 에 맡긴다. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Civilians")
	bool IsFighter(const APawn* Civilian) const { return Civilian && Fighters.Contains(const_cast<APawn*>(Civilian)); }

private:
	void HandleActorSpawned(AActor* Actor);
	void DecideRole(APawn* Civilian);
	bool IsCivilian(const APawn& Pawn) const;
	float GetFightBackChance() const;

	FDelegateHandle ActorSpawnedHandle;

	TSet<TWeakObjectPtr<APawn>> Decided;
	TSet<TWeakObjectPtr<APawn>> Fighters;

	/** 무기 에셋. GC 가 치우지 않게 붙잡아 둔다. */
	UPROPERTY()
	TArray<TObjectPtr<UObject>> LoadedWeapons;

	bool bLoggedSignature = false;
};
