// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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
