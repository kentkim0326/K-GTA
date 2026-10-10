// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GunDayDisputeSpot.generated.h"

/**
 * 시비가 일어나는 자리.
 *
 * 골목마다 하나씩 놓는다. 편의점 앞, 주차장, 버스정류장, 담배 피우는 구석.
 * 자리마다 어울리는 상황을 골라 두면 그 장소에서만 그 시비가 난다.
 *
 * 배치만 하면 된다. 나머지는 UGunDayDisputeSubsystem 이 알아서 한다.
 */
UCLASS(Blueprintable)
class GUNDAYCORE_API AGunDayDisputeSpot : public AActor
{
	GENERATED_BODY()

public:
	AGunDayDisputeSpot();

	/**
	 * 이 자리에서 일어날 수 있는 상황의 번호.
	 * 프로젝트 세팅 > Game > GunDay Core > 시비 의 Dispute Scenarios 목록 순서를 쓴다.
	 * 비워 두면 전체 목록에서 아무거나 고른다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "시비 지점")
	TArray<int32> ScenarioIndices;

	/** 이 반경 안의 시민이 시비에 끌려 들어간다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "시비 지점", meta = (ClampMin = "100.0"))
	float Radius = 600.0f;

	/** 한 번 일어난 뒤 다시 일어나기까지의 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "시비 지점", meta = (ClampMin = "0.0", Units = "s"))
	float CooldownSeconds = 120.0f;

	/** 꺼 두면 이 자리에서는 시비가 나지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "시비 지점")
	bool bEnabled = true;

	/**
	 * 플레이어가 다가오면 이 자리의 상황에 맞는 배역 둘을 시야 밖에서 세운다.
	 * 끄면 반경 안에 이미 있는 시민을 끌어들인다. 사람이 많은 광장이면 끈다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "시비 지점")
	bool bBringOwnCast = true;

	/** 마지막으로 시비가 난 시각. 서브시스템이 관리한다. */
	float LastUsedSeconds = -10000.0f;

	/** 이 자리가 세운 배역. 서브시스템이 관리한다. */
	TWeakObjectPtr<APawn> CastFirst;
	TWeakObjectPtr<APawn> CastSecond;

	/** 배역을 세울 때 고른 상황. 배역이 있는 동안은 이 상황만 난다. */
	int32 CastScenarioIndex = INDEX_NONE;

	/** 총이 오간 뒤에는 같은 배역으로 다시 시작하지 않는다. 플레이어가 떠나면 풀린다. */
	bool bCastSpent = false;

	bool HasCast() const { return CastFirst.IsValid() && CastSecond.IsValid(); }

#if WITH_EDITORONLY_DATA
	/** 에디터에서 자리를 보기 위한 표식. */
	UPROPERTY()
	TObjectPtr<class UBillboardComponent> Billboard;
#endif
};
