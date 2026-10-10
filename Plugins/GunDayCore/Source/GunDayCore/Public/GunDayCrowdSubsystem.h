// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GunDayCrowdSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGunDayCivilianAlerted, APawn*, Civilian, FVector, NoiseLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGunDayCivilianReported, APawn*, Civilian);

/** 총성을 들은 시민 한 명의 상태. */
USTRUCT()
struct FGunDayAlertedCivilian
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<APawn> Civilian;

	/** 이 시각이 지나면 신고한다. */
	float ReportAtSeconds = 0.0f;

	/** 이 시각까지 목격자로 잡아 둔다. */
	float WitnessUntilSeconds = 0.0f;

	bool bReported = false;

	bool bWitnessRegistered = false;
};

/**
 * 군중 반응.
 *
 * 총성이 나면 반경 안의 시민에게 알린다. 시민은 소리의 반대쪽으로 달아나고,
 * 잠시 뒤 신고한다. 신고가 들어가면 열기가 오르고 그 시민은 한동안 목격자로 잡힌다.
 *
 * 킷의 civilian 프리셋을 그대로 쓴다. 새 AI 를 만들지 않는다.
 * 킷의 행동 트리와 다투지 않으려면 Drive Civilian Flee 를 끄고
 * OnCivilianAlerted 에 블루프린트를 붙여 킷 쪽 반응을 호출하면 된다.
 */
UCLASS(BlueprintType)
class GUNDAYCORE_API UGunDayCrowdSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GunDay|Crowd", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Crowd Subsystem"))
	static UGunDayCrowdSubsystem* Get(const UObject* WorldContextObject);

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
	 * 그 지점에서 총성이 났다고 알린다.
	 * 범죄 감지가 켜져 있으면 자동으로 불린다. 직접 부를 일은 많지 않다.
	 */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Crowd")
	void NotifyGunshot(FVector NoiseLocation);

	/** 지금 놀란 상태인 시민 수. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Crowd")
	int32 GetAlertedCivilianCount() const { return Alerted.Num(); }

	/** 이 시민이 지금 놀라 달아나는 중인가. 행인을 걷게 하는 쪽이 이 동안은 손대지 않는다. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Crowd")
	bool IsAlerted(const APawn* Civilian) const;

	/** 놀란 시민을 전부 진정시킨다. 목격자 등록도 푼다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Crowd")
	void CalmAll();

	/** 시민 한 명이 총성을 들었다. 킷 쪽 반응을 붙이려면 여기에 건다. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|Crowd")
	FGunDayCivilianAlerted OnCivilianAlerted;

	/** 시민 한 명이 신고했다. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|Crowd")
	FGunDayCivilianReported OnCivilianReported;

private:
	/** 소리의 반대쪽 내비메시 위로 달아나게 한다. */
	void DriveFlee(APawn& Civilian, const FVector& NoiseLocation) const;

	/** 이 폰을 시민으로 볼 것인가. 플레이어와 경찰은 뺀다. */
	bool IsCivilian(const APawn& Pawn) const;

	/** 목격자 등록을 하나 올리거나 내린다. */
	void SetWitnessRegistered(FGunDayAlertedCivilian& Entry, bool bRegistered);

	const class UGunDayCoreSettings* GetSettings() const;

	class UGunDayWantedSubsystem* GetWantedSubsystem() const;

	UPROPERTY()
	TArray<FGunDayAlertedCivilian> Alerted;

	/** 월드가 시작하고 흐른 시간. */
	float ElapsedSeconds = 0.0f;

	/** 같은 총성으로 반복해서 놀라지 않도록 하는 간격. */
	float TimeSinceLastGunshot = 1000.0f;
};
