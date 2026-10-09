// Copyright K-GTA. All Rights Reserved.

#include "GunDayDisputeSpot.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"

AGunDayDisputeSpot::AGunDayDisputeSpot()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	Billboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	if (Billboard)
	{
		Billboard->SetupAttachment(RootComponent);
	}
#endif
}
