// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GunDayTypes.h"
#include "GunDayDisputeSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGunDayDisputeStageChanged, APawn*, First, APawn*, Second, EGunDayDisputeStage, Stage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGunDayDisputeLine, APawn*, Speaker, const FString&, Line, EGunDayDisputeStage, Stage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGunDayDisputeShot, APawn*, Shooter, APawn*, Victim);

/** 진행 중인 시비 하나. */
USTRUCT(BlueprintType)
struct FGunDayActiveDispute
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "시비")
	TWeakObjectPtr<APawn> First;

	UPROPERTY(BlueprintReadOnly, Category = "시비")
	TWeakObjectPtr<APawn> Second;

	/** 쓰는 상황의 번호. */
	UPROPERTY(BlueprintReadOnly, Category = "시비")
	int32 ScenarioIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "시비")
	EGunDayDisputeStage Stage = EGunDayDisputeStage::Verbal;

	/** 이 시각이 지나면 다음 단계를 판정한다. */
	float NextStageAtSeconds = 0.0f;

	/** 이번 단계에서 말할 차례. 둘이 번갈아 말한다. */
	bool bFirstSpeaks = true;
};

/**
 * 시비 시스템.
 *
 * 《총기허용의 날》의 핵심이다. 플레이어가 특별해서 총을 쏘는 세계가 아니라
 * 누구나 총을 가진 세계다. 주차, 담배, 노인석, 라면. 사소한 일로 시민끼리 붙고
 * 말다툼에서 몸싸움으로, 총을 꺼내는 데까지 간다.
 *
 * 플레이어가 없어도 일어난다. 플레이어는 지나가다 마주친다.
 * 지나칠지, 말릴지, 끼어들어 쏠지는 플레이어가 정한다.
 *
 * 자리는 AGunDayDisputeSpot 을 레벨에 놓아 정한다. 골목마다 어울리는 상황을 붙인다.
 * 하나도 없으면 플레이어 주변 시민 중에서 아무나 골라 시작한다.
 */
UCLASS(BlueprintType)
class GUNDAYCORE_API UGunDayDisputeSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GunDay|Dispute", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Dispute Subsystem"))
	static UGunDayDisputeSubsystem* Get(const UObject* WorldContextObject);

	// ~ USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	// ~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return false; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;

	/** 플레이어 주변에서 시비를 하나 억지로 일으킨다. 시험용. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Dispute")
	bool StartDisputeNearPlayer(int32 ScenarioIndex = -1);

	/** 진행 중인 시비를 전부 끝낸다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Dispute")
	void ClearDisputes();

	UFUNCTION(BlueprintPure, Category = "GunDay|Dispute")
	int32 GetActiveDisputeCount() const { return Active.Num(); }

	/** 이 폰이 지금 시비에 끼어 있는가. */
	UFUNCTION(BlueprintPure, Category = "GunDay|Dispute")
	bool IsInDispute(const APawn* Pawn) const;

	UFUNCTION(BlueprintCallable, Category = "GunDay|Dispute")
	void SetDisputesEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "GunDay|Dispute")
	bool AreDisputesEnabled() const { return bDisputesEnabled; }

	/** 단계가 바뀌었다. 애니메이션과 연출을 여기에 붙인다. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|Dispute")
	FGunDayDisputeStageChanged OnDisputeStageChanged;

	/** 누가 한마디 했다. 자막과 음성을 여기에 붙인다. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|Dispute")
	FGunDayDisputeLine OnDisputeLine;

	/** 쐈다. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|Dispute")
	FGunDayDisputeShot OnDisputeShot;

private:
	/** 레벨에 놓인 시비 지점 중 쓸 만한 곳을 찾아 시비를 시작한다. */
	bool TryStartDisputeAtSpot();

	/** 두 사람으로 시비를 시작한다. */
	bool BeginDispute(APawn& First, APawn& Second, int32 ScenarioIndex);

	/** 한 단계 올린다. 확률을 못 넘으면 가라앉는다. */
	void AdvanceDispute(FGunDayActiveDispute& Dispute);

	/** 이번 단계의 대사를 한 줄 내보낸다. */
	void SpeakLine(FGunDayActiveDispute& Dispute);

	/** 총을 쏜다. 피해는 킷의 체력 시스템이 받는다. */
	void FireShot(FGunDayActiveDispute& Dispute);

	/** 반경 안의 시민을 모은다. 플레이어와 경찰과 이미 시비 중인 사람은 뺀다. */
	void GatherCandidates(const FVector& Center, float Radius, TArray<APawn*>& OutPawns) const;

	const FGunDayDisputeScenario* GetScenario(int32 Index) const;

	const class UGunDayCoreSettings* GetSettings() const;

	UPROPERTY()
	TArray<FGunDayActiveDispute> Active;

	float ElapsedSeconds = 0.0f;

	float TimeSinceLastAttempt = 0.0f;

	bool bDisputesEnabled = true;
};
