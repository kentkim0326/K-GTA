// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GunDayNewsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGunDayHeadline, const FString&, Headline);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGunDayDayRollover, int32, DayNumber, int32, YesterdayDeaths);

/**
 * 뉴스 티커.
 *
 * 이 게임에서 가장 무서운 것은 총이 아니라 아무도 놀라지 않는다는 사실이다.
 * 사람이 죽으면 화면 아래로 한 줄이 지나간다. 숫자로만 처리된다.
 * "오늘 총기 사망 37명, 어제보다 4명 감소."
 *
 * 세 종류를 섞어 내보낸다.
 *   속보 — 방금 벌어진 사건
 *   통계 — 하루가 끝날 때의 집계
 *   잡담 — 사건과 무관한 멘트. 광고, 논평, 날씨. 무심함은 여기서 나온다.
 *
 * 문구 안의 자리표는 내보낼 때 숫자로 바뀐다.
 *   {오늘} {어제} {차이} {증감} {총} {시비} {정} {날}
 */
UCLASS(BlueprintType)
class GUNDAYCORE_API UGunDayNewsSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GunDay|News", meta = (WorldContext = "WorldContextObject", DisplayName = "Get News Subsystem"))
	static UGunDayNewsSubsystem* Get(const UObject* WorldContextObject);

	// ~ USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	// ~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return false; }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;

	/** 총격 한 건을 집계에 넣는다. 사망이면 사망자 수도 올라간다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|News")
	void ReportShooting(bool bFatal, bool bByPlayer);

	/** 시비 한 건을 집계에 넣는다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|News")
	void ReportDispute();

	/** 지금 바로 한 줄 내보낸다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|News")
	void PushHeadline(const FString& RawText);

	/** 잡담 한 줄을 뽑아 내보낸다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|News")
	void PushFiller();

	/** 마지막으로 내보낸 한 줄. UI 가 붙기 전까지 읽어 갈 수 있다. */
	UFUNCTION(BlueprintPure, Category = "GunDay|News")
	FString GetLastHeadline() const { return LastHeadline; }

	UFUNCTION(BlueprintPure, Category = "GunDay|News")
	int32 GetTodayDeaths() const { return TodayDeaths; }

	UFUNCTION(BlueprintPure, Category = "GunDay|News")
	int32 GetYesterdayDeaths() const { return YesterdayDeaths; }

	UFUNCTION(BlueprintPure, Category = "GunDay|News")
	int32 GetTotalDeaths() const { return TotalDeaths; }

	UFUNCTION(BlueprintPure, Category = "GunDay|News")
	int32 GetDayNumber() const { return DayNumber; }

	/** 하루를 강제로 넘긴다. 시험용. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|News")
	void AdvanceDay();

	/** 한 줄이 나왔다. 화면 아래 티커 UI 를 여기에 붙인다. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|News")
	FGunDayHeadline OnHeadline;

	/** 하루가 지났다. 일일 집계 화면을 여기에 붙인다. */
	UPROPERTY(BlueprintAssignable, Category = "GunDay|News")
	FGunDayDayRollover OnDayRollover;

private:
	/** 문구 안의 자리표를 지금 숫자로 바꾼다. */
	FString Format(const FString& Template) const;

	/** 목록에서 한 줄 뽑아 내보낸다. */
	void PushFrom(const TArray<FString>& Templates);

	const class UGunDayCoreSettings* GetSettings() const;

	class UGunDaySocietySubsystem* GetSocietySubsystem() const;

	FString LastHeadline;

	int32 TodayDeaths = 0;

	int32 YesterdayDeaths = 0;

	int32 TotalDeaths = 0;

	int32 TodayShootings = 0;

	int32 TodayDisputes = 0;

	int32 DayNumber = 1;

	float TimeSinceHeadline = 0.0f;

	float TimeSinceDayStart = 0.0f;
};
