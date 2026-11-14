// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Input/Reply.h"
#include "Styling/SlateBrush.h"
#include "Subsystems/WorldSubsystem.h"
#include "GunDayFrontEndSubsystem.generated.h"

class SWidget;
class UTexture2D;
struct FKey;

/**
 * 시작 화면과 결과 화면.
 *
 * 맵이 열리면 포스터를 띄우고 게임을 멈춘다. 아무 키나 누르면 1분짜리 판이
 * 시작된다(UGunDayEncounterSubsystem). 판이 끝나면 버텼는지 쓰러졌는지를
 * 띄우고, R 은 다시 하기, Esc 는 끝내기다. 다시 할 때는 시작 화면을 건너뛴다.
 *
 * 에셋 없이 Slate 로 그린다. 문구는 프로젝트 세팅 > GunDay Core > 시작·결과 화면.
 */
UCLASS()
class GUNDAYCORE_API UGunDayFrontEndSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ~ USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	// ~ UWorldSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** 시작 화면을 닫고 판을 시작한다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|FrontEnd")
	void StartRound();

private:
	void ShowTitle();
	void ShowResult();
	void ShowScreen(TSharedRef<SWidget> Content);
	void HideScreen();
	void PauseWhileScreenShown();

	FReply HandleTitleKey(const FKey& Key);
	FReply HandleResultKey(const FKey& Key);

	UFUNCTION()
	void HandleEncounterFinished(bool bSurvived, int32 Kills, float Seconds);

	void SampleWantedLevel();

	TSharedPtr<SWidget> Screen;

	/** 어떤 화면이 떠 있는가. 0 없음, 1 시작, 2 결과. */
	uint8 ScreenKind = 0;
	double ScreenShownAtSeconds = 0.0;

	/**
	 * 화면이 떠 있는 동안 모든 키를 먼저 받는다. 창을 클릭하면 Slate 포커스가 게임 뷰포트로
	 * 넘어가 위젯이 키를 못 받는 일이 있어서, 포커스와 상관없이 받는다.
	 */
	TSharedPtr<class IInputProcessor> InputCatcher;

	bool HandleScreenKey(const FKey& Key, bool bIsRepeat);

	/** 키를 받아 다음 프레임 처리를 기다리는 중. 한 번만 처리한다. */
	bool bKeyPending = false;

	/** 바탕 그림. 화면이 떠 있는 동안 GC 가 치우지 않게 붙잡아 둔다. */
	UPROPERTY()
	TObjectPtr<UTexture2D> BackgroundTexture;

	UPROPERTY()
	TObjectPtr<class UMediaPlayer> MoviePlayer;

	UPROPERTY()
	TObjectPtr<class UMediaTexture> MovieTexture;

	UPROPERTY()
	TObjectPtr<class UFileMediaSource> MovieSource;

	FSlateBrush BackgroundBrush;
	FSlateBrush MovieBrush;

	/** 동영상을 열어 반복 재생을 건다. 파일이 없으면 바탕 그림만 남는다. */
	void StartTitleMovie();
	void StopTitleMovie();

	bool bRoundRunning = false;
	bool bLastSurvived = false;
	int32 LastKills = 0;
	float LastSeconds = 0.0f;
	int32 PeakWantedLevel = 0;

	FTimerHandle ResultTimer;
	FTimerHandle WantedSampleTimer;
};
