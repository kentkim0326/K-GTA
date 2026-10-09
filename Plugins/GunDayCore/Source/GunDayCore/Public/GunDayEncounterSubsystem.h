// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GunDayEncounterSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGunDayEncounterStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGunDayEncounterFinished, bool, bSurvived, int32, Kills, float, Seconds);

/**
 * 첫 마일스톤 측정 도구 — 「골목 하나에서 경찰 둘과 1분간 총격전」.
 *
 * 매번 손으로 세팅하지 않고 같은 조건으로 반복하기 위한 것이다.
 * 수배를 초기화하고 지정한 수배 레벨로 올린 뒤 제한 시간을 잰다.
 * 끝나면 살아남았는지, 몇 명을 쓰러뜨렸는지 화면에 띄운다.
 *
 *   GunDay.Alley.Start      기본값으로 시작 (60초, 수배 레벨 1)
 *   GunDay.Alley.Start 90 2 90초, 수배 레벨 2
 *   GunDay.Alley.Stop       중단
 *
 * 이 1분이 재미있으면 게임이 된다. 재미없으면 도시를 아무리 지어도 소용없다.
 */
UCLASS(BlueprintType)
class GUNDAYCORE_API UGunDayEncounterSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GunDay|Encounter", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Encounter Subsystem"))
	static UGunDayEncounterSubsystem* Get(const UObject* WorldContextObject);

	// ~ USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	// ~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bRunning; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;

	/** 측정을 시작한다. 수배와 투입 인원을 초기화한 뒤 지정 레벨로 올린다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Encounter")
	void StartEncounter(float DurationSeconds = 60.0f, int32 WantedLevel = 1);

	/** 측정을 중단한다. 결과는 남기지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Encounter")
	void StopEncounter();

	UFUNCTION(BlueprintPure, Category = "GunDay|Encounter")
	bool IsRunning() const { return bRunning; }

	/** 남은 시간(초). */
	UFUNCTION(BlueprintPure, Category = "GunDay|Encounter")
	float GetRemainingSeconds() const;

	/** 이번 판에서 쓰러뜨린 경찰 수. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Encounter")
	int32 GetKills() const { return Kills; }

	UPROPERTY(BlueprintAssignable, Category = "GunDay|Encounter")
	FGunDayEncounterStarted OnEncounterStarted;

	/** 끝났다. 살아남았는지, 몇 명을 쓰러뜨렸는지, 몇 초가 걸렸는지. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|Encounter")
	FGunDayEncounterFinished OnEncounterFinished;

private:
	UFUNCTION()
	void HandleResponderLost();

	/** 플레이어가 쓰러졌는가. 폰이 사라졌거나 컨트롤러를 잃었으면 그렇게 본다. */
	bool IsPlayerDown() const;

	void FinishEncounter(bool bSurvived);

	class UGunDayWantedSubsystem* GetWantedSubsystem() const;

	class UGunDayPoliceResponseSubsystem* GetPoliceSubsystem() const;

	bool bRunning = false;

	float ElapsedSeconds = 0.0f;

	float DurationSeconds = 60.0f;

	int32 Kills = 0;

	bool bBoundToPolice = false;
};
