// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GunDayAppearanceSubsystem.generated.h"

class ACharacter;
class USkeletalMeshComponent;
struct FGunDayAppearance;

/**
 * 겉모습.
 *
 * 킷 캐릭터는 마네킹이다. 그 마네킹은 그대로 두고 숨긴 채 애니메이션만 돌리고,
 * 그 위에 사람 몸·얼굴·옷·머리카락을 덧씌운다. 킷 블루프린트는 고치지 않는다.
 *
 *   UE4 마네킹(숨김) → UE5 Manny(숨김) → MetaHuman 몸 → 옷(몸을 따라감)
 *                                                    → 얼굴 → 머리카락
 *
 * 두 번의 리타깃은 킷에 들어 있는 템플릿 애님 블루프린트가 한다.
 * 플레이어처럼 처음부터 UE5 Manny 인 캐릭터는 첫 단계를 건너뛴다.
 *
 * 누구에게 무엇을 입힐지는 프로젝트 세팅 > GunDay Core > 외형 에 있다.
 * 맞거나 죽는 판정, 무기 소켓은 전부 숨긴 마네킹이 그대로 맡는다.
 */
UCLASS()
class GUNDAYCORE_API UGunDayAppearanceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ~ USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ~ UWorldSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** 이 캐릭터에 규칙에 맞는 겉모습을 입힌다. 이미 입었으면 아무것도 안 한다. */
	UFUNCTION(BlueprintCallable, Category = "GunDay|Appearance")
	bool ApplyAppearance(ACharacter* Character);

private:
	void HandleActorSpawned(AActor* Actor);

	/** 규칙에 쓰인 메시와 애님 클래스를 미리 불러 둔다. 처음 나타날 때 멈칫하지 않게. */
	void PreloadAssets();

	const FGunDayAppearance* PickAppearance(const ACharacter& Character) const;

	USkeletalMeshComponent* AddSkeletalPart(ACharacter& Character, USceneComponent& Parent, USkeletalMesh* Mesh, UClass* AnimClass, FName Name);

	FDelegateHandle ActorSpawnedHandle;

	/** 미리 불러 둔 자산. GC 가 치우지 않게 붙잡아 둔다. */
	UPROPERTY()
	TArray<TObjectPtr<UObject>> LoadedAssets;

	/** 이미 입힌 캐릭터. */
	TSet<TWeakObjectPtr<ACharacter>> Dressed;
};
