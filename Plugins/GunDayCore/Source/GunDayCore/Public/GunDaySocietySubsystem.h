// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GunDayTypes.h"
#include "GunDaySocietySubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGunDayJeongChanged, float, NewJeong, float, Delta);

/**
 * 사회의 온도 — 정(情) 과 진영.
 *
 * 작품의 주제를 숫자 하나로 옮긴 것이다.
 * 돈은 벌었지만 정이 사라진 사회. 정이 높으면 시비가 중간에 가라앉고 누가 말린다.
 * 낮으면 전부 끝까지 간다. 플레이어의 행동이 이 숫자를 움직인다.
 *
 * 진영은 사람마다 붙는 보이지 않는 꼬리표다. 정치, 부동산, 세대, 지역, 성별.
 * 다른 진영끼리 만날수록 시비가 붙을 확률과 끝까지 갈 확률이 올라간다.
 * 폰 이름에서 결정적으로 만들어 내므로 같은 사람은 늘 같은 진영을 갖는다.
 */
UCLASS(BlueprintType)
class GUNDAYCORE_API UGunDaySocietySubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GunDay|Society", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Society Subsystem"))
	static UGunDaySocietySubsystem* Get(const UObject* WorldContextObject);

	// ~ USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return false; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;

	/** 지금의 정. 0 이면 서로 남이고 100 이면 서로 이웃이다. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Society")
	float GetJeong() const { return Jeong; }

	/** 정을 더하거나 뺀다. 말리면 오르고 쏘면 내린다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Society")
	void AddJeong(float Delta);

	/** 정을 직접 맞춘다. 시험용. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Society")
	void SetJeong(float NewJeong);

	/** 0~1 로 환산한 정. 공식에 쓰기 좋게. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Society")
	float GetJeongFraction() const;

	/** 이 사람의 진영. 없으면 만들어 둔다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Society")
	FGunDayPersonProfile GetProfile(const APawn* Pawn);

	/**
	 * 두 사람 사이의 마찰. 0 이면 부딪힐 일이 없고 1 이면 모든 축에서 반대편이다.
	 * 시비가 붙을 확률과 끝까지 갈 확률이 여기서 갈린다.
	 */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Society")
	float GetFriction(const APawn* First, const APawn* Second);

	/** 두 사람이 가장 세게 부딪히는 축. 대사를 고를 때 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Society")
	bool GetSharpestFaultLine(const APawn* First, const APawn* Second, EGunDayFaultLine& OutFaultLine);

	/** 사람 한 명의 진영을 읽기 좋은 한 줄로. 디버그용. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Society")
	FString DescribeProfile(const APawn* Pawn);

	/** 정이 바뀌었다. HUD 와 연출을 여기에 붙인다. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|Society")
	FGunDayJeongChanged OnJeongChanged;

private:
	/** 폰 이름에서 결정적으로 진영을 만든다. 같은 사람은 늘 같은 진영이다. */
	FGunDayPersonProfile MakeProfile(const APawn& Pawn) const;

	const class UGunDayCoreSettings* GetSettings() const;

	/** 0~100. 사회 전체에 하나뿐이다. */
	float Jeong = 50.0f;

	UPROPERTY()
	TMap<TWeakObjectPtr<APawn>, FGunDayPersonProfile> Profiles;

	/** 기준값으로 되돌아가는 속도를 재기 위한 누적 시간. */
	float TimeSincePrune = 0.0f;
};
