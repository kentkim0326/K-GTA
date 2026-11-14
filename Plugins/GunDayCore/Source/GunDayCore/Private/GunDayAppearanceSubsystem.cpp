// Copyright K-GTA. All Rights Reserved.

#include "GunDayAppearanceSubsystem.h"

#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GunDayCore.h"
#include "GunDayCoreSettings.h"
#include "TimerManager.h"

namespace
{
	const UGunDayCoreSettings* GetAppearanceSettings()
	{
		return GetDefault<UGunDayCoreSettings>();
	}

	/** 참조 포즈에서 뼈 하나의 컴포넌트 공간 변환. */
	FTransform GetRefPoseComponentSpace(const USkeletalMesh& Mesh, FName BoneName)
	{
		const FReferenceSkeleton& RefSkeleton = Mesh.GetRefSkeleton();
		const TArray<FTransform>& LocalPose = RefSkeleton.GetRefBonePose();

		FTransform ComponentSpace = FTransform::Identity;
		for (int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName); BoneIndex != INDEX_NONE; BoneIndex = RefSkeleton.GetParentIndex(BoneIndex))
		{
			ComponentSpace = ComponentSpace * LocalPose[BoneIndex];
		}
		return ComponentSpace;
	}
}

bool UGunDayAppearanceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}

	return false;
}

void UGunDayAppearanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 맵이 열리며 생기는 캐릭터부터 잡으려고 BeginPlay 전에 건다.
	if (UWorld* World = GetWorld())
	{
		ActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleActorSpawned));
	}
}

void UGunDayAppearanceSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}
	ActorSpawnedHandle.Reset();
	Dressed.Reset();
	LoadedAssets.Reset();

	Super::Deinitialize();
}

void UGunDayAppearanceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const UGunDayCoreSettings* Settings = GetAppearanceSettings();
	if (!Settings || !Settings->bApplyAppearances)
	{
		return;
	}

	PreloadAssets();

	// 맵에 처음부터 놓여 있었거나 BeginPlay 전에 생긴 캐릭터.
	for (TActorIterator<ACharacter> It(&InWorld); It; ++It)
	{
		ApplyAppearance(*It);
	}
}

void UGunDayAppearanceSubsystem::HandleActorSpawned(AActor* Actor)
{
	ACharacter* Character = Cast<ACharacter>(Actor);
	UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return;
	}

	const UGunDayCoreSettings* Settings = GetAppearanceSettings();
	if (!Settings || !Settings->bApplyAppearances)
	{
		return;
	}

	// 생성 직후에는 킷 블루프린트가 메시를 아직 고르고 있을 수 있다. 한 틱 뒤에 입힌다.
	TWeakObjectPtr<ThisClass> WeakThis(this);
	TWeakObjectPtr<ACharacter> WeakCharacter(Character);
	World->GetTimerManager().SetTimerForNextTick([WeakThis, WeakCharacter]()
	{
		if (WeakThis.IsValid() && WeakCharacter.IsValid())
		{
			WeakThis->ApplyAppearance(WeakCharacter.Get());
		}
	});
}

void UGunDayAppearanceSubsystem::PreloadAssets()
{
	const UGunDayCoreSettings* Settings = GetAppearanceSettings();
	if (!Settings)
	{
		return;
	}

	auto Keep = [this](UObject* Object)
	{
		if (Object)
		{
			LoadedAssets.AddUnique(Object);
		}
	};

	Keep(Settings->UE4MannequinSkeleton.LoadSynchronous());
	Keep(Settings->MannyMesh.LoadSynchronous());
	Keep(Settings->UE4ToUE5RetargetAnimClass.LoadSynchronous());
	Keep(Settings->UE5ToMetaHumanRetargetAnimClass.LoadSynchronous());
	Keep(Settings->FaceAnimClass.LoadSynchronous());

	for (const FGunDayAppearanceRule& Rule : Settings->AppearanceRules)
	{
		Keep(Rule.PawnClass.LoadSynchronous());
		for (const FGunDayAppearance& Appearance : Rule.Appearances)
		{
			Keep(Appearance.Body.LoadSynchronous());
			Keep(Appearance.Face.LoadSynchronous());
			for (const TSoftObjectPtr<USkeletalMesh>& Cloth : Appearance.Clothes)
			{
				Keep(Cloth.LoadSynchronous());
			}
			for (const TSoftObjectPtr<UStaticMesh>& Hair : Appearance.Hair)
			{
				Keep(Hair.LoadSynchronous());
			}
		}
	}

	UE_LOG(LogGunDay, Log, TEXT("외형: 자산 %d 개를 불러 뒀다."), LoadedAssets.Num());
}

const FGunDayAppearance* UGunDayAppearanceSubsystem::PickAppearance(const ACharacter& Character) const
{
	const UGunDayCoreSettings* Settings = GetAppearanceSettings();
	if (!Settings)
	{
		return nullptr;
	}

	for (const FGunDayAppearanceRule& Rule : Settings->AppearanceRules)
	{
		const UClass* PawnClass = Rule.PawnClass.Get();
		if (PawnClass && Character.IsA(PawnClass) && Rule.Appearances.Num() > 0)
		{
			// 같은 사람은 늘 같은 옷. 고유 번호로 고른다.
			const int32 Index = static_cast<int32>(Character.GetUniqueID() % static_cast<uint32>(Rule.Appearances.Num()));
			return &Rule.Appearances[Index];
		}
	}

	return nullptr;
}

USkeletalMeshComponent* UGunDayAppearanceSubsystem::AddSkeletalPart(ACharacter& Character, USceneComponent& Parent, USkeletalMesh* Mesh, UClass* AnimClass, FName Name)
{
	USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(&Character, MakeUniqueObjectName(&Character, USkeletalMeshComponent::StaticClass(), Name));
	Part->SetupAttachment(&Parent);
	Part->SetSkeletalMeshAsset(Mesh);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);

	if (AnimClass)
	{
		// 리타깃 노드는 붙은 부모 메시의 포즈를 읽는다. 부모가 먼저 돌아야 한다.
		Part->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Part->SetAnimInstanceClass(AnimClass);
	}

	Part->RegisterComponent();
	Character.AddInstanceComponent(Part);

	if (USkeletalMeshComponent* ParentMesh = Cast<USkeletalMeshComponent>(&Parent))
	{
		Part->AddTickPrerequisiteComponent(ParentMesh);
	}

	return Part;
}

bool UGunDayAppearanceSubsystem::ApplyAppearance(ACharacter* Character)
{
	const UGunDayCoreSettings* Settings = GetAppearanceSettings();
	if (!Character || !Settings || !Settings->bApplyAppearances || Dressed.Contains(Character))
	{
		return false;
	}

	USkeletalMeshComponent* Source = Character->GetMesh();
	if (!Source || !Source->GetSkeletalMeshAsset())
	{
		return false;
	}

	const FGunDayAppearance* Appearance = PickAppearance(*Character);
	if (!Appearance)
	{
		return false;
	}

	USkeletalMesh* BodyMesh = Appearance->Body.LoadSynchronous();
	UClass* BodyAnimClass = Settings->UE5ToMetaHumanRetargetAnimClass.LoadSynchronous();
	if (!BodyMesh || !BodyAnimClass)
	{
		UE_LOG(LogGunDay, Warning, TEXT("외형: '%s' 의 몸 메시나 리타깃 애님 클래스를 못 불러 %s 는 마네킹으로 둔다."),
			*Appearance->Name.ToString(), *Character->GetName());
		return false;
	}

	Dressed.Add(Character);

	// 1. UE4 마네킹이면 UE5 Manny 를 한 번 거친다. 화면에는 안 보이지만 포즈는 계속 계산한다.
	USkeletalMeshComponent* Driver = Source;
	const USkeleton* UE4Skeleton = Settings->UE4MannequinSkeleton.LoadSynchronous();
	if (UE4Skeleton && Source->GetSkeletalMeshAsset()->GetSkeleton() == UE4Skeleton)
	{
		USkeletalMesh* MannyMesh = Settings->MannyMesh.LoadSynchronous();
		UClass* MannyAnimClass = Settings->UE4ToUE5RetargetAnimClass.LoadSynchronous();
		if (!MannyMesh || !MannyAnimClass)
		{
			UE_LOG(LogGunDay, Warning, TEXT("외형: UE5 Manny 메시나 UE4→UE5 리타깃을 못 불러 %s 는 마네킹으로 둔다."), *Character->GetName());
			return false;
		}

		Driver = AddSkeletalPart(*Character, *Source, MannyMesh, MannyAnimClass, TEXT("GunDay_Manny"));
		Driver->SetVisibility(false);
		Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// 2. 몸. UE5 Manny 의 포즈를 MetaHuman 몸으로 옮긴다.
	USkeletalMeshComponent* Body = AddSkeletalPart(*Character, *Driver, BodyMesh, BodyAnimClass, TEXT("GunDay_Body"));

	// 3. 옷과 신발. 몸의 뼈를 그대로 따른다.
	for (const TSoftObjectPtr<USkeletalMesh>& ClothRef : Appearance->Clothes)
	{
		if (USkeletalMesh* ClothMesh = ClothRef.LoadSynchronous())
		{
			USkeletalMeshComponent* Cloth = AddSkeletalPart(*Character, *Body, ClothMesh, nullptr, TEXT("GunDay_Cloth"));
			Cloth->SetLeaderPoseComponent(Body);
		}
	}

	// 4. 얼굴과 머리카락.
	if (USkeletalMesh* FaceMesh = Appearance->Face.LoadSynchronous())
	{
		USkeletalMeshComponent* Face = AddSkeletalPart(*Character, *Body, FaceMesh, Settings->FaceAnimClass.LoadSynchronous(), TEXT("GunDay_Face"));

		// 머리카락 메시는 얼굴의 참조 포즈 공간에 놓여 있다. head 뼈에 붙이고 그만큼 되돌린다.
		const FTransform HeadRefPose = GetRefPoseComponentSpace(*FaceMesh, Settings->HairBoneName);
		for (const TSoftObjectPtr<UStaticMesh>& HairRef : Appearance->Hair)
		{
			if (UStaticMesh* HairMesh = HairRef.LoadSynchronous())
			{
				UStaticMeshComponent* Hair = NewObject<UStaticMeshComponent>(Character, MakeUniqueObjectName(Character, UStaticMeshComponent::StaticClass(), TEXT("GunDay_Hair")));
				Hair->SetupAttachment(Face, Settings->HairBoneName);
				Hair->SetStaticMesh(HairMesh);
				Hair->SetRelativeTransform(HeadRefPose.Inverse());
				Hair->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Hair->SetGenerateOverlapEvents(false);
				Hair->SetCanEverAffectNavigation(false);
				Hair->RegisterComponent();
				Character->AddInstanceComponent(Hair);
			}
		}
	}

	// 5. 원래 마네킹은 숨긴다. 맞는 판정·래그돌·무기 소켓은 계속 이 메시가 맡으니 포즈는 계속 계산한다.
	Source->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Source->SetVisibility(false, false);

	UE_LOG(LogGunDay, Log, TEXT("외형: %s 에 '%s' 를 입혔다 (%s)."),
		*Character->GetName(), *Appearance->Name.ToString(), Driver == Source ? TEXT("UE5 직접") : TEXT("UE4→UE5→MetaHuman"));
	return true;
}
