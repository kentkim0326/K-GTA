// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GunDayTypes.h"
#include "GunDayCrimeWatcherSubsystem.generated.h"

/**
 * 범죄 자동 감지.
 *
 * 킷 블루프린트를 고치지 않고 수배를 올리기 위한 장치다.
 * 월드의 폰들에 주기적으로 피해 이벤트를 걸어 두고, 플레이어가 입힌 피해만 골라
 * 수배 시스템에 신고한다. 맞은 쪽이 경찰이면 경찰 부상, 아니면 시민 부상으로 친다.
 *
 * 총을 허공에 쏘는 것까지는 잡지 못한다. 엔진에 "발사" 라는 공통 이벤트가 없기 때문이다.
 * 그건 킷의 발사 이벤트에서 ReportPlayerGunfire 를 한 번 불러 주면 된다.
 */
UCLASS(BlueprintType)
class GUNDAYCORE_API UGunDayCrimeWatcherSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GunDay|Crime", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Crime Watcher Subsystem"))
	static UGunDayCrimeWatcherSubsystem* Get(const UObject* WorldContextObject);

	// ~ USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	// ~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return false; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;

	/**
	 * 플레이어가 총을 쐈다고 알린다. 킷의 발사 이벤트에 이 노드 하나만 붙이면
	 * 허공에 쏜 것까지 수배에 반영된다. 연사로 열기가 폭주하지 않도록 쿨다운이 걸려 있다.
	 */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Crime")
	void ReportPlayerGunfire();

	/** 자동 감지를 켜고 끈다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Crime")
	void SetWatchEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "GunDay|Crime")
	bool IsWatchEnabled() const { return bWatchEnabled; }

	/** 지금 피해 이벤트를 걸어 둔 폰 수. 디버그용. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Crime")
	int32 GetWatchedPawnCount() const { return WatchedPawns.Num(); }

private:
	UFUNCTION()
	void HandlePawnDamaged(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void HandlePawnDestroyed(AActor* DestroyedActor);

	/** 새로 생긴 폰에 피해 이벤트를 건다. */
	void RefreshWatchedPawns();

	/** 맞은 쪽이 경찰인가. 투입 명단과 설정의 경찰 클래스 목록으로 판단한다. */
	bool IsPoliceActor(const AActor& Actor) const;

	/** 피해를 입힌 쪽이 플레이어인가. */
	bool IsPlayerInstigator(const AController* InstigatedBy, const AActor* DamageCauser) const;

	const class UGunDayCoreSettings* GetSettings() const;

	class UGunDayWantedSubsystem* GetWantedSubsystem() const;

	/** 이미 이벤트를 걸어 둔 폰. 중복으로 걸지 않기 위한 목록. */
	TSet<TWeakObjectPtr<AActor>> WatchedPawns;

	/** 플레이어에게 맞은 적이 있는 폰. 사라질 때 사망으로 칠지 판단한다. */
	TSet<TWeakObjectPtr<AActor>> WoundedByPlayer;

	/** 폰별 마지막 부상 신고 시각. 한 발 한 발을 전부 세지 않기 위한 쿨다운. */
	TMap<TWeakObjectPtr<AActor>, float> LastInjuryReportTime;

	float TimeSinceRescan = 0.0f;

	float TimeSinceGunfireReport = 0.0f;

	/** 월드가 시작하고 흐른 시간. 쿨다운 계산용. */
	float ElapsedSeconds = 0.0f;

	bool bWatchEnabled = true;
};
