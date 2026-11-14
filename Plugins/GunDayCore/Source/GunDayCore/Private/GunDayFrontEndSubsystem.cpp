// Copyright K-GTA. All Rights Reserved.

#include "GunDayFrontEndSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FileMediaSource.h"
#include "Components/AudioComponent.h"
#include "Containers/Ticker.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "GameFramework/PlayerController.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "GunDayEncounterSubsystem.h"
#include "GunDayWantedSubsystem.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** 다시 하기로 맵을 다시 열었을 때 시작 화면을 건너뛰려고 기억한다. 프로세스 동안만 산다. */
	bool GGunDaySkipTitleOnce = false;

	DECLARE_DELEGATE_RetVal_OneParam(FReply, FGunDayScreenKey, const FKey&);

	/** 화면 전체를 덮고 키 입력을 받는 판. */
	class SGunDayScreen : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SGunDayScreen) {}
			SLATE_DEFAULT_SLOT(FArguments, Content)
			SLATE_EVENT(FGunDayScreenKey, OnKey)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			OnKey = InArgs._OnKey;
			ChildSlot[InArgs._Content.Widget];
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

		virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override
		{
			return OnKey.IsBound() ? OnKey.Execute(KeyEvent.GetKey()) : FReply::Handled();
		}

		virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override
		{
			return OnKey.IsBound() ? OnKey.Execute(MouseEvent.GetEffectingButton()) : FReply::Handled();
		}

	private:
		FGunDayScreenKey OnKey;
	};

	/** 포커스와 상관없이 키를 먼저 받아 화면에 넘긴다. */
	class FGunDayScreenInput : public IInputProcessor
	{
	public:
		explicit FGunDayScreenInput(TFunction<bool(const FKey&, bool)> InOnKey) : OnKey(MoveTemp(InOnKey)) {}

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override
		{
			return OnKey(KeyEvent.GetKey(), KeyEvent.IsRepeat());
		}

		virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
		{
			return OnKey(MouseEvent.GetEffectingButton(), false);
		}

		virtual const TCHAR* GetDebugName() const override { return TEXT("GunDayScreenInput"); }

	private:
		TFunction<bool(const FKey&, bool)> OnKey;
	};

	FSlateFontInfo Font(const TCHAR* Typeface, int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	}

	FString PickLine(const TArray<FString>& Lines)
	{
		return Lines.Num() > 0 ? Lines[FMath::RandRange(0, Lines.Num() - 1)] : FString();
	}

	FString Stars(int32 Count, int32 Max)
	{
		FString Out;
		for (int32 Index = 0; Index < FMath::Max(Max, Count); ++Index)
		{
			Out += Index < Count ? TEXT("★") : TEXT("☆");
		}
		return Out;
	}
}

bool UGunDayFrontEndSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}

	return false;
}

void UGunDayFrontEndSubsystem::Deinitialize()
{
	HideScreen();
	StopTitleMovie();
	StopNarration();
	StopMusic(0.0f);
	BackgroundTexture = nullptr;

	Super::Deinitialize();
}

void UGunDayFrontEndSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();
	if (!Settings || !Settings->bShowFrontEnd)
	{
		return;
	}

	if (UGunDayEncounterSubsystem* Encounter = UGunDayEncounterSubsystem::Get(this))
	{
		Encounter->OnEncounterFinished.AddUniqueDynamic(this, &ThisClass::HandleEncounterFinished);
	}

	// -GunDayAutoStart: 시작 화면 없이 바로 판을 시작한다. 키 입력 없이 결과 화면까지 재 보는 테스트용.
	if (GGunDaySkipTitleOnce || FParse::Param(FCommandLine::Get(), TEXT("GunDayAutoStart")))
	{
		GGunDaySkipTitleOnce = false;
		// 맵이 다 깨어난 다음 틱에 시작한다. 킷이 플레이어를 세우기 전에 판이 돌면 안 된다.
		InWorld.GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::StartRound));
		return;
	}

	ShowTitle();
}

void UGunDayFrontEndSubsystem::ShowScreen(TSharedRef<SWidget> Content)
{
	HideScreen();

	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}

	Screen = Content;
	ScreenShownAtSeconds = FPlatformTime::Seconds();
	Viewport->AddViewportWidgetContent(Content, 1000);

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocus(Content, EFocusCause::SetDirectly);

		TWeakObjectPtr<ThisClass> WeakThis(this);
		InputCatcher = MakeShared<FGunDayScreenInput>([WeakThis](const FKey& Key, bool bIsRepeat)
		{
			return WeakThis.IsValid() && WeakThis->HandleScreenKey(Key, bIsRepeat);
		});
		FSlateApplication::Get().RegisterInputPreProcessor(InputCatcher);
	}
}

bool UGunDayFrontEndSubsystem::HandleScreenKey(const FKey& Key, bool bIsRepeat)
{
	if (!Screen.IsValid())
	{
		return false;
	}

	// 화면이 막 뜬 순간의 입력(쏘던 손, 누르고 있던 키)은 흘려보낸다.
	if (bIsRepeat || FPlatformTime::Seconds() - ScreenShownAtSeconds < 0.5)
	{
		return true;
	}

	// 전처리기 안에서 화면을 닫으면(전처리기 해제) 순회 중인 목록을 건드리게 된다.
	// 처리는 다음 프레임으로 미룬다. 게임이 멈춰 있어 월드 타이머는 못 쓰고 코어 티커를 쓴다.
	if (bKeyPending)
	{
		return true;
	}
	bKeyPending = true;

	const uint8 Kind = ScreenKind;
	TWeakObjectPtr<ThisClass> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Kind, Key](float)
	{
		if (ThisClass* Self = WeakThis.Get())
		{
			Self->bKeyPending = false;
			if (Kind == 1)
			{
				Self->HandleTitleKey(Key);
			}
			else if (Kind == 2)
			{
				Self->HandleResultKey(Key);
			}
		}
		return false;
	}));
	return true;
}

void UGunDayFrontEndSubsystem::HideScreen()
{
	if (InputCatcher.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputCatcher);
	}
	InputCatcher.Reset();
	ScreenKind = 0;

	if (!Screen.IsValid())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
		}
	}
	Screen.Reset();

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

void UGunDayFrontEndSubsystem::ShowTitle()
{
	const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();

	BackgroundTexture = Settings->TitleBackground.LoadSynchronous();
	BackgroundBrush = FSlateBrush();
	if (BackgroundTexture)
	{
		BackgroundBrush.SetResourceObject(BackgroundTexture);
		BackgroundBrush.ImageSize = FVector2D(BackgroundTexture->GetSizeX(), BackgroundTexture->GetSizeY());
	}

	StartTitleMovie();
	StartNarration();
	PlayMusic(Settings->TitleMusic, Settings->TitleMusicVolume, true);

	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	auto Shade = [White](float Alpha) -> TSharedRef<SWidget>
	{
		return SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, Alpha));
	};

	TSharedRef<SWidget> Content =
		SNew(SGunDayScreen)
		.OnKey(FGunDayScreenKey::CreateUObject(this, &ThisClass::HandleTitleKey))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				Shade(1.0f)
			]
			// 바탕 그림, 그 위에 동영상. 동영상은 첫 프레임 전까지 투명이라 그림이 비친다.
			+ SOverlay::Slot()
			[
				SNew(SScaleBox).Stretch(EStretch::ScaleToFill)
				[
					SNew(SImage).Image(&BackgroundBrush)
				]
			]
			+ SOverlay::Slot()
			[
				SNew(SScaleBox).Stretch(EStretch::ScaleToFill)
				[
					SNew(SImage).Image(&MovieBrush)
				]
			]
			// 글자가 읽히게 아래쪽을 단계적으로 어둡게 한다.
			+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(0.52f)[ Shade(0.0f) ]
				+ SVerticalBox::Slot().FillHeight(0.10f)[ Shade(0.2f) ]
				+ SVerticalBox::Slot().FillHeight(0.10f)[ Shade(0.4f) ]
				+ SVerticalBox::Slot().FillHeight(0.28f)[ Shade(0.62f) ]
			]
			// 내레이션 자막. 제목 위, 화면 가운데.
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 440.0f))
			[
				// 폭을 고정하고 그 폭에서 줄바꿈한다. 자동 줄바꿈은 가운데 정렬 슬롯에서 폭을 좁게 잡아
				// 한 단어씩 세로로 쪼개졌다.
				SNew(SBox)
				.WidthOverride(1300.0f)
				[
					SNew(STextBlock)
					.Text_UObject(this, &ThisClass::GetCurrentCaption)
					.Font(Font(TEXT("Italic"), 32))
					.ColorAndOpacity(FLinearColor(1.0f, 0.96f, 0.86f))
					.ShadowOffset(FVector2D(2.0f, 2.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
					.Justification(ETextJustify::Center)
					.WrapTextAt(1300.0f)
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(90.0f, 0.0f, 0.0f, 80.0f))
			[
				SNew(SVerticalBox)
				// 제목 위의 작은 딱지. 잼 테마 "DIRECTOR'S CUT". 비우면 안 그린다.
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(4.0f, 0.0f, 0.0f, 10.0f)
				[
					SNew(SBorder)
					.Visibility(Settings->TitleTag.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
					.BorderImage(White)
					.BorderBackgroundColor(FLinearColor(0.72f, 0.06f, 0.06f, 0.92f))
					.Padding(FMargin(14.0f, 5.0f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(Settings->TitleTag))
						.Font([]()
						{
							FSlateFontInfo TagFont = Font(TEXT("Bold"), 22);
							TagFont.LetterSpacing = 320;
							return TagFont;
						}())
						.ColorAndOpacity(FLinearColor::White)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Settings->TitleText))
					.Font(Font(TEXT("Bold"), 84))
					.ColorAndOpacity(FLinearColor::White)
					.ShadowOffset(FVector2D(3.0f, 3.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
				]
				// 세로 상자는 가로를 꽉 채우므로 폭 제한은 글자에 직접 건다. 오른쪽 PRESS ANY KEY 와 겹치지 않게.
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
				[
					SNew(SBox)
					.WidthOverride(760.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Settings->TitleBlurb))
						.Font(Font(TEXT("Regular"), 24))
						.ColorAndOpacity(FLinearColor(0.88f, 0.88f, 0.88f))
						.ShadowOffset(FVector2D(2.0f, 2.0f))
						.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
						.WrapTextAt(760.0f)
					]
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(0.0f, 0.0f, 90.0f, 88.0f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Settings->TitlePrompt))
				.Font(Font(TEXT("Bold"), 30))
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(2.0f, 2.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
			]
		];

	ShowScreen(Content);
	ScreenKind = 1;
	PauseWhileScreenShown();
}

void UGunDayFrontEndSubsystem::StartTitleMovie()
{
	const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();
	MovieBrush = FSlateBrush();
	MovieBrush.DrawAs = ESlateBrushDrawType::NoDrawType;

	const FString MoviePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / Settings->TitleMovie);
	if (Settings->TitleMovie.IsEmpty() || !FPaths::FileExists(MoviePath))
	{
		UE_LOG(LogGunDay, Log, TEXT("시작 화면: 동영상이 없어 그림만 쓴다 (%s)"), *MoviePath);
		return;
	}

	MoviePlayer = NewObject<UMediaPlayer>(this);
	MoviePlayer->SetLooping(true);
	MoviePlayer->PlayOnOpen = true;

	MovieTexture = NewObject<UMediaTexture>(this);
	MovieTexture->AutoClear = true;
	MovieTexture->ClearColor = FLinearColor::Transparent;
	MovieTexture->SetMediaPlayer(MoviePlayer);
	MovieTexture->UpdateResource();

	MovieSource = NewObject<UFileMediaSource>(this);
	MovieSource->SetFilePath(MoviePath);

	MovieBrush = FSlateBrush();
	MovieBrush.SetResourceObject(MovieTexture);
	MovieBrush.ImageSize = FVector2D(1920.0f, 1080.0f);

	const bool bOpened = MoviePlayer->OpenSource(MovieSource);
	UE_LOG(LogGunDay, Log, TEXT("시작 화면: 동영상 %s (%s)"), bOpened ? TEXT("재생") : TEXT("열기 실패"), *MoviePath);
}

void UGunDayFrontEndSubsystem::StopTitleMovie()
{
	if (MoviePlayer)
	{
		MoviePlayer->Close();
	}
	MovieBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	MoviePlayer = nullptr;
	MovieTexture = nullptr;
	MovieSource = nullptr;
}

void UGunDayFrontEndSubsystem::PauseWhileScreenShown()
{
	// 맵이 막 열린 순간에는 플레이어 컨트롤러가 없어 멈추기가 실패한다. 될 때까지 다음 틱에 다시 한다.
	if (!Screen.IsValid() || UGameplayStatics::SetGamePaused(this, true))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::PauseWhileScreenShown));
	}
}

FReply UGunDayFrontEndSubsystem::HandleTitleKey(const FKey& Key)
{
	if (Key == EKeys::Escape)
	{
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
		return FReply::Handled();
	}

	StartRound();
	return FReply::Handled();
}

void UGunDayFrontEndSubsystem::StartNarration()
{
	StopNarration();

	const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();
	USoundBase* Sound = Settings->TitleNarration.LoadSynchronous();
	if (!Sound)
	{
		return;
	}

	Narration = UGameplayStatics::CreateSound2D(this, Sound, 1.0f, 1.0f, 0.0f, nullptr, false, false);
	if (Narration)
	{
		// 시작 화면은 게임을 멈춘다. UI 소리여야 멈춘 동안에도 들린다.
		Narration->bIsUISound = true;
		Narration->Play();
		NarrationStartSeconds = FPlatformTime::Seconds();
		UE_LOG(LogGunDay, Log, TEXT("시작 화면: 내레이션 재생 (%s, 자막 %d줄)"), *Sound->GetName(), Settings->TitleCaptions.Num());
	}
}

void UGunDayFrontEndSubsystem::PlayMusic(const TSoftObjectPtr<USoundBase>& Track, float Volume, bool bUISound)
{
	StopMusic(0.6f);

	USoundBase* Sound = Track.LoadSynchronous();
	if (!Sound)
	{
		return;
	}

	Music = UGameplayStatics::CreateSound2D(this, Sound, Volume, 1.0f, 0.0f, nullptr, false, false);
	if (Music)
	{
		// 시작·결과 화면은 게임을 멈춘다. 그 화면의 음악은 UI 소리여야 들린다.
		Music->bIsUISound = bUISound;
		Music->FadeIn(1.0f, Volume);
	}
}

void UGunDayFrontEndSubsystem::StopMusic(float FadeSeconds)
{
	if (Music)
	{
		if (FadeSeconds > 0.0f)
		{
			Music->bAutoDestroy = true;
			Music->FadeOut(FadeSeconds, 0.0f);
		}
		else
		{
			Music->Stop();
			Music->DestroyComponent();
		}
	}
	Music = nullptr;
}

void UGunDayFrontEndSubsystem::StopNarration()
{
	if (Narration)
	{
		Narration->Stop();
		Narration->DestroyComponent();
	}
	Narration = nullptr;
}

FText UGunDayFrontEndSubsystem::GetCurrentCaption() const
{
	if (!Narration)
	{
		return FText::GetEmpty();
	}

	const float Elapsed = static_cast<float>(FPlatformTime::Seconds() - NarrationStartSeconds);
	for (const FGunDayCaption& Caption : GetDefault<UGunDayCoreSettings>()->TitleCaptions)
	{
		if (Elapsed >= Caption.Start && Elapsed < Caption.End)
		{
			return FText::FromString(Caption.Text);
		}
	}
	return FText::GetEmpty();
}

void UGunDayFrontEndSubsystem::StartRound()
{
	HideScreen();
	StopTitleMovie();
	StopNarration();
	UGameplayStatics::SetGamePaused(this, false);
	{
		const UGunDayCoreSettings* MusicSettings = GetDefault<UGunDayCoreSettings>();
		PlayMusic(MusicSettings->RoundMusic, MusicSettings->RoundMusicVolume, false);
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->SetShowMouseCursor(false);
	}

	const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();
	UGunDayEncounterSubsystem* Encounter = UGunDayEncounterSubsystem::Get(this);
	if (!Encounter)
	{
		return;
	}

	bRoundRunning = true;
	PeakWantedLevel = 0;
	Encounter->StartEncounter(Settings->RoundSeconds, Settings->RoundStartWantedLevel);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(WantedSampleTimer, FTimerDelegate::CreateUObject(this, &ThisClass::SampleWantedLevel), 0.5f, true);
	}

	UE_LOG(LogGunDay, Log, TEXT("시작 화면: 판 시작 (%.0f초, 수배 %d)"), Settings->RoundSeconds, Settings->RoundStartWantedLevel);
}

void UGunDayFrontEndSubsystem::SampleWantedLevel()
{
	if (const UGunDayWantedSubsystem* Wanted = UGunDayWantedSubsystem::Get(this))
	{
		PeakWantedLevel = FMath::Max(PeakWantedLevel, Wanted->GetWantedLevel());
	}
}

void UGunDayFrontEndSubsystem::HandleEncounterFinished(bool bSurvived, int32 Kills, float Seconds)
{
	if (!bRoundRunning)
	{
		return;
	}

	SampleWantedLevel();
	bRoundRunning = false;
	bLastSurvived = bSurvived;
	LastKills = Kills;
	LastSeconds = Seconds;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WantedSampleTimer);
		const float Delay = GetDefault<UGunDayCoreSettings>()->ResultDelaySeconds;
		if (Delay > 0.0f)
		{
			World->GetTimerManager().SetTimer(ResultTimer, FTimerDelegate::CreateUObject(this, &ThisClass::ShowResult), Delay, false);
			return;
		}
	}

	ShowResult();
}

void UGunDayFrontEndSubsystem::ShowResult()
{
	const UGunDayCoreSettings* Settings = GetDefault<UGunDayCoreSettings>();

	const FString Title = bLastSurvived ? Settings->SurvivedTitle : Settings->DiedTitle;
	const FString Line = PickLine(bLastSurvived ? Settings->SurvivedLines : Settings->DiedLines);
	const FLinearColor TitleColor = bLastSurvived ? FLinearColor(0.95f, 0.95f, 0.95f) : FLinearColor(0.85f, 0.12f, 0.1f);

	int32 MaxWanted = 5;
	if (const UGunDayWantedSubsystem* Wanted = UGunDayWantedSubsystem::Get(this))
	{
		MaxWanted = Wanted->GetMaxWantedLevel();
	}

	const FString Stats = FString::Printf(TEXT("TIME  %d:%02d        KILLS  %d        WANTED  %s"),
		FMath::FloorToInt(LastSeconds) / 60, FMath::FloorToInt(LastSeconds) % 60, LastKills, *Stars(PeakWantedLevel, MaxWanted));

	TSharedRef<SWidget> Content =
		SNew(SGunDayScreen)
		.OnKey(FGunDayScreenKey::CreateUObject(this, &ThisClass::HandleResultKey))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.78f))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 24.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Title))
					.Font(Font(TEXT("Bold"), 72))
					.ColorAndOpacity(TitleColor)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 48.0f)
				[
					SNew(SBox)
					.MaxDesiredWidth(1100.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Line))
						.Font(Font(TEXT("Italic"), 28))
						.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f))
						.Justification(ETextJustify::Center)
						.AutoWrapText(true)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 64.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Stats))
					.Font(Font(TEXT("Regular"), 26))
					.ColorAndOpacity(FLinearColor(0.95f, 0.8f, 0.3f))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Settings->ResultPrompt))
					.Font(Font(TEXT("Bold"), 26))
					.ColorAndOpacity(FLinearColor::White)
				]
			]
		];

	ShowScreen(Content);
	ScreenKind = 2;
	PauseWhileScreenShown();

	// 총격전이 끝나면 다시 잔잔한 곡. 방금 일어난 일과 어울리지 않는 그 대비가 이 게임의 말투다.
	PlayMusic(Settings->TitleMusic, Settings->TitleMusicVolume, true);

	UE_LOG(LogGunDay, Log, TEXT("결과 화면: %s, %.1f초, 처치 %d, 최고 수배 %d"),
		bLastSurvived ? TEXT("생존") : TEXT("쓰러짐"), LastSeconds, LastKills, PeakWantedLevel);
}

FReply UGunDayFrontEndSubsystem::HandleResultKey(const FKey& Key)
{
	if (Key == EKeys::R || Key == EKeys::Enter || Key == EKeys::SpaceBar)
	{
		GGunDaySkipTitleOnce = true;
		UGameplayStatics::SetGamePaused(this, false);
		HideScreen();
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
		return FReply::Handled();
	}

	if (Key == EKeys::Escape)
	{
		UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
	}

	return FReply::Handled();
}
