// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "GunDayTypes.h"
#include "GunDayWantedSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGunDayWantedLevelChanged, int32, OldLevel, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGunDaySpottedChanged, bool, bSpotted);

/**
 * 수배 레벨 시스템.
 *
 * 킷의 블루프린트에서 부르는 것을 전제로 만들었다.
 *   - 총을 쏘거나 누구를 때리면 ReportCrime 을 부른다.
 *   - 경찰·시민 AI Perception 이 플레이어를 보면 AddWitness, 놓치면 RemoveWitness.
 *   - 경찰 스포너와 인카운터는 OnWantedLevelChanged 를 듣고 반응한다.
 *
 * 게임 인스턴스 서브시스템이라 레벨이 바뀌어도 살아 있다. 레벨 이동 후에도
 * 수배를 유지하고 싶지 않다면 새 레벨 진입 시 ClearWanted 를 부른다.
 */
UCLASS(BlueprintType)
class GUNDAYCORE_API UGunDayWantedSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** 어디서든 서브시스템을 잡는다. 월드가 없으면 널을 돌려준다. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Wanted", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Wanted Subsystem"))
	static UGunDayWantedSubsystem* Get(const UObject* WorldContextObject);

	// ~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return false; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;

	/** 범죄 한 건을 신고한다. 설정된 가중치만큼 열기가 오른다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Wanted")
	void ReportCrime(EGunDayCrime Crime);

	/** 열기를 직접 더한다. 음수를 넣으면 깎인다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Wanted")
	void AddHeat(float Amount);

	/** 목격자가 한 명 늘었다. 보는 눈이 하나라도 있으면 열기가 줄지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Wanted")
	void AddWitness();

	/** 목격자가 플레이어를 놓쳤다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Wanted")
	void RemoveWitness();

	/** 목격자 수를 0 으로 되돌린다. 레벨 전환이나 AI 전멸 후 정리용. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Wanted")
	void ClearWitnesses();

	/** 수배를 완전히 지운다. 체포, 사망, 세이프하우스 진입에 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Wanted")
	void ClearWanted();

	/** 수배 레벨을 강제로 맞춘다. 미션 스크립트와 디버그용. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Wanted")
	void SetWantedLevel(int32 NewLevel);

	/** 현재 수배 레벨. 0 이면 수배 없음. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Wanted")
	int32 GetWantedLevel() const { return WantedLevel; }

	/** 설정된 문턱값 개수에서 나오는 최대 수배 레벨. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Wanted")
	int32 GetMaxWantedLevel() const;

	/** 현재 열기. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Wanted")
	float GetHeat() const { return Heat; }

	/** 현재 레벨 구간 안에서의 진행도 0~1. UI 게이지용. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Wanted")
	float GetHeatFractionToNextLevel() const;

	/** 지금 누군가가 플레이어를 보고 있는가. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Wanted")
	bool IsSpotted() const { return WitnessCount > 0; }

	/** 열기가 줄기 시작하기까지 남은 시간(초). 이미 줄고 있으면 0. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Wanted")
	float GetTimeUntilDecay() const;

	/** 시스템 자체를 켜고 끈다. 컷신과 튜토리얼에서 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Wanted")
	void SetWantedSystemEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "GunDay|Wanted")
	bool IsWantedSystemEnabled() const { return bEnabled; }

	/** 수배 레벨이 오르거나 내릴 때. 경찰 스포너는 여기에 붙는다. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|Wanted")
	FGunDayWantedLevelChanged OnWantedLevelChanged;

	/** 목격 상태가 바뀔 때. 0 명 ↔ 1 명 이상 경계에서만 울린다. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|Wanted")
	FGunDaySpottedChanged OnSpottedChanged;

private:
	/** 열기를 넣고 레벨을 다시 계산한다. */
	void SetHeat(float NewHeat);

	/** 열기에 해당하는 수배 레벨을 문턱값 표에서 찾는다. */
	int32 HeatToWantedLevel(float InHeat) const;

	/** 레벨이 실제로 바뀌었을 때만 델리게이트를 울린다. */
	void UpdateWantedLevel();

	const class UGunDayCoreSettings* GetSettings() const;

	/** 누적 열기. 수배 레벨은 여기서 파생된다. */
	float Heat = 0.0f;

	/** 마지막으로 계산된 수배 레벨. */
	int32 WantedLevel = 0;

	/** 지금 플레이어를 보고 있는 목격자 수. */
	int32 WitnessCount = 0;

	/** 목격이 끊긴 뒤 흐른 시간(초). 회피 대기 시간을 여기서 잰다. */
	float TimeSinceLastSeenSeconds = 0.0f;

	/** 시스템 on/off. */
	bool bEnabled = true;
};
