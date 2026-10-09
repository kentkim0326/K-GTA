// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GunDayTypes.h"
#include "GunDayCoreSettings.generated.h"

/**
 * 프로젝트 세팅 > Game > GunDay Core 에서 수정한다.
 * 값은 Config/DefaultGame.ini 에 저장되므로 팀 전체가 공유한다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "GunDay Core"))
class GUNDAYCORE_API UGunDayCoreSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGunDayCoreSettings();

	virtual FName GetCategoryName() const override { return FName("Game"); }

	/** 범죄 한 건이 더하는 열기. 키가 없는 범죄는 0 으로 친다. */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨")
	TMap<EGunDayCrime, float> CrimeHeat;

	/**
	 * 각 수배 레벨에 도달하는 열기 문턱값. 오름차순으로 둔다.
	 * 원소 개수가 곧 최대 수배 레벨이다(기본 5).
	 */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨")
	TArray<float> WantedLevelThresholds;

	/** 마지막 목격이 끊긴 뒤 열기가 줄기 시작할 때까지의 시간(초). */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨", meta = (ClampMin = "0.0", Units = "s"))
	float EvasionDelaySeconds;

	/** 수배 레벨 1당 회피 대기 시간에 더해지는 시간(초). 높은 수배일수록 오래 쫓긴다. */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨", meta = (ClampMin = "0.0", Units = "s"))
	float EvasionDelayPerLevel;

	/** 회피 상태에서 초당 줄어드는 열기. */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨", meta = (ClampMin = "0.0"))
	float HeatDecayPerSecond;

	/** 목격자가 있는 동안에도 열기가 줄어들지 여부. 끄면 시야 안에서는 절대 줄지 않는다. */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨")
	bool bDecayOnlyWhenUnseen;

	/** 열기 상한. 문턱값 최대치를 넘겨 둬야 최고 수배가 잠깐이라도 유지된다. */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨", meta = (ClampMin = "0.0"))
	float MaxHeat;

	/**
	 * 수배 레벨별 경찰 투입 규칙. 레벨당 한 줄씩 채운다.
	 * 비어 있으면 아무도 투입되지 않는다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "경찰 대응")
	TArray<FGunDayResponseTier> ResponseTiers;

	/**
	 * 플러그인이 직접 액터를 스폰할지 여부.
	 * 끄면 OnResponseTierChanged 만 울린다. 킷의 스포너를 쓰고 싶을 때 끈다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "경찰 대응")
	bool bSpawnResponders;

	/** 수배가 풀리면 투입했던 인원을 정리한다. */
	UPROPERTY(config, EditAnywhere, Category = "경찰 대응")
	bool bDismissRespondersOnClear;
};
