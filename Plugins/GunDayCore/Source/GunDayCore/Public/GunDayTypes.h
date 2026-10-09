// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"
#include "GunDayTypes.generated.h"

/** 수배 열기(heat)를 올리는 행위. 값별 가중치는 GunDayCoreSettings 에서 조정한다. */
UENUM(BlueprintType)
enum class EGunDayCrime : uint8
{
	/** 총을 꺼내 들어 시민이 목격했다. */
	Brandishing			UMETA(DisplayName = "총기 노출"),
	/** 공공장소 발포. */
	PublicGunfire		UMETA(DisplayName = "공공장소 발포"),
	/** 시민 부상. */
	CivilianInjured		UMETA(DisplayName = "시민 부상"),
	/** 시민 사망. */
	CivilianKilled		UMETA(DisplayName = "시민 사망"),
	/** 경찰 부상. */
	PoliceInjured		UMETA(DisplayName = "경찰 부상"),
	/** 경찰 사망. */
	PoliceKilled		UMETA(DisplayName = "경찰 사망"),
	/** 차량 탈취. */
	VehicleTheft		UMETA(DisplayName = "차량 탈취"),
	/** 기물 파손. */
	PropertyDamage		UMETA(DisplayName = "기물 파손"),

	MAX					UMETA(Hidden)
};

/**
 * 수배 레벨 하나에 대응하는 경찰 투입 규칙.
 * 프로젝트 세팅 > Game > GunDay Core 에서 레벨별로 한 줄씩 채운다.
 */
USTRUCT(BlueprintType)
struct GUNDAYCORE_API FGunDayResponseTier
{
	GENERATED_BODY()

	/** 이 규칙이 적용되는 수배 레벨. 같은 레벨이 여러 줄이면 첫 줄만 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "경찰 대응", meta = (ClampMin = "1"))
	int32 WantedLevel = 1;

	/**
	 * 투입할 액터 클래스. 킷의 적 블루프린트를 그대로 지정한다.
	 * 비워 두면 이 레벨에서는 직접 스폰하지 않는다.
	 * (킷의 스포너를 쓰고 싶으면 비워 두고 OnResponseTierChanged 에 블루프린트를 붙인다.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "경찰 대응")
	TSoftClassPtr<AActor> ResponderClass;

	/** 이 레벨에서 살아 있어야 할 인원. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "경찰 대응", meta = (ClampMin = "0"))
	int32 DesiredCount = 2;

	/** 한 명을 투입하고 다음 한 명까지 기다리는 시간(초). 한꺼번에 쏟지 않기 위한 간격. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "경찰 대응", meta = (ClampMin = "0.0", Units = "s"))
	float SpawnIntervalSeconds = 3.0f;

	/** 플레이어로부터 이 거리보다 가까운 곳에는 생성하지 않는다. 눈앞에 튀어나오지 않게 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "경찰 대응", meta = (ClampMin = "0.0"))
	float MinSpawnDistance = 1500.0f;

	/** 생성 후보를 찾는 최대 반경. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "경찰 대응", meta = (ClampMin = "0.0"))
	float MaxSpawnDistance = 4000.0f;

	/** 이 거리보다 멀어진 투입 인원은 정리한다. 0 이면 정리하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "경찰 대응", meta = (ClampMin = "0.0"))
	float DespawnDistance = 12000.0f;
};
