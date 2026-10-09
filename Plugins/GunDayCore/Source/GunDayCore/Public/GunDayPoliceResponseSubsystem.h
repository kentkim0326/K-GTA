// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GunDayTypes.h"
#include "GunDayPoliceResponseSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGunDayResponseTierChanged, int32, WantedLevel, int32, DesiredCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGunDayResponderChanged, AActor*, Responder);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGunDayResponderLost);

/**
 * 경찰 대응 배선.
 *
 * 수배 레벨이 바뀌면 그 레벨의 규칙(FGunDayResponseTier)을 찾아
 * 살아 있어야 할 인원을 맞춘다. 모자라면 간격을 두고 한 명씩 투입하고,
 * 죽거나 너무 멀어진 인원은 목록에서 뺀다.
 *
 * 쓰는 방법은 둘 중 하나다.
 *
 * 1. 직접 스폰 — 프로젝트 세팅에서 레벨별 ResponderClass 에 킷의 적 블루프린트를 지정한다.
 *    내비메시 위의 빈 자리를 찾아 플레이어 시야 밖에 생성한다.
 *
 * 2. 킷 스포너 사용 — bSpawnResponders 를 끄고 OnResponseTierChanged 에 블루프린트를 붙인다.
 *    킷의 AI 스포너를 호출한 뒤 RegisterResponder 로 결과를 등록하면 인원 계산에 반영된다.
 *
 * 월드 서브시스템이라 레벨마다 새로 만들어진다. 수배 레벨 자체는 게임 인스턴스에 산다.
 */
UCLASS(BlueprintType)
class GUNDAYCORE_API UGunDayPoliceResponseSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** 어디서든 서브시스템을 잡는다. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Police", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Police Response Subsystem"))
	static UGunDayPoliceResponseSubsystem* Get(const UObject* WorldContextObject);

	// ~ USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	// ~ UWorldSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return false; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;

	/**
	 * 외부에서 만든 액터를 투입 인원으로 등록한다.
	 * 킷의 AI 스포너를 쓸 때 스폰 결과를 이걸로 넘기면 인원 계산에 들어간다.
	 */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Police")
	void RegisterResponder(AActor* Responder);

	/** 등록을 해제한다. 액터를 없애지는 않는다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Police")
	void UnregisterResponder(AActor* Responder);

	/** 투입했던 인원을 전부 정리한다. 등록만 해제할지, 액터까지 없앨지 고른다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Police")
	void DismissAllResponders(bool bDestroyActors = true);

	/** 지금 살아 있는 투입 인원 수. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Police")
	int32 GetAliveResponderCount() const;

	/** 현재 수배 레벨에서 유지해야 할 인원 수. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Police")
	int32 GetDesiredResponderCount() const;

	/** 현재 수배 레벨에 적용되는 규칙을 가져온다. 해당 레벨 규칙이 없으면 false. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Police")
	bool GetActiveTier(FGunDayResponseTier& OutTier) const;

	/** 스폰을 잠시 멈추거나 재개한다. 컷신과 디버그용. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Police")
	void SetResponseEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "GunDay|Police")
	bool IsResponseEnabled() const { return bResponseEnabled; }

	/** 수배 레벨이 바뀌어 유지 인원이 달라졌다. 킷 스포너를 쓸 때 여기에 붙인다. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|Police")
	FGunDayResponseTierChanged OnResponseTierChanged;

	/** 한 명이 투입됐다. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|Police")
	FGunDayResponderChanged OnResponderSpawned;

	/** 한 명이 목록에서 빠졌다. 이탈과 정리를 포함한다. 액터는 아직 살아 있다. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|Police")
	FGunDayResponderChanged OnResponderDismissed;

	/**
	 * 한 명이 사라졌다. 대개 쓰러져서 액터가 없어진 경우다.
	 * 넘길 포인터가 남아 있지 않아 인자가 없다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|Police")
	FGunDayResponderLost OnResponderLost;

private:
	UFUNCTION()
	void HandleWantedLevelChanged(int32 OldLevel, int32 NewLevel);

	/** 목록에 넣고 목격자 수를 맞춘다. 투입 경로는 전부 이걸 거친다. */
	void AddToRoster(AActor* Responder);

	/** 목록에서 빼고 목격자 수를 맞춘다. 빠진 액터를 돌려준다(이미 사라졌으면 널). */
	AActor* RemoveFromRosterAt(int32 Index);

	/** 투입 인원 수와 등록해 둔 목격자 수를 맞춘다. */
	void SyncWitnessCount();

	/** 죽었거나 사라진 인원을 목록에서 뺀다. 너무 멀어진 인원도 정리한다. */
	void PruneResponders(const FGunDayResponseTier& Tier);

	/** 한 명을 투입한다. 자리를 못 찾으면 false. */
	bool TrySpawnResponder(const FGunDayResponseTier& Tier);

	/** 플레이어 주변 내비메시 위에서 시야 밖 지점을 고른다. */
	bool FindSpawnLocation(const FGunDayResponseTier& Tier, const AActor& Player, FVector& OutLocation) const;

	const class UGunDayCoreSettings* GetSettings() const;

	class UGunDayWantedSubsystem* GetWantedSubsystem() const;

	/** 지금 투입되어 있는 인원. 약참조라 액터가 사라지면 자동으로 비워진다. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Responders;

	/** 마지막 투입 이후 흐른 시간(초). */
	float TimeSinceLastSpawn = 0.0f;

	/** 마지막으로 반영한 수배 레벨. */
	int32 CachedWantedLevel = 0;

	bool bResponseEnabled = true;

	bool bBoundToWanted = false;

	/** 수배 시스템에 올려 둔 목격자 수. 투입 인원 수와 같게 유지한다. */
	int32 RegisteredWitnesses = 0;
};
