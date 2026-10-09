// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GunDayTypes.h"
#include "GunDayCoreSettings.generated.h"

/**
 * 프로젝트 세팅 > Game > GunDay Core 에서 수정한다.
 * 값은 Config/DefaultGame.ini 에 저장되므로 팀 전체가 공유한다.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "GunDay Core"))
class GUNDAYCORE_API UGunDayCoreSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGunDayCoreSettings();

	virtual FName GetCategoryName() const override { return FName("Game"); }

	/** 범죄 한 건이 더하는 열기. 키가 없는 범죄는 0 으로 친다. */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨")
	TMap<EGunDayCrime, float> CrimeHeat;

	/**
	 * 각 수배 레벨에 도달하는 열기 문턱값. 오름차순으로 둔다.
	 * 원소 개수가 곧 최대 수배 레벨이다(기본 5).
	 */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨")
	TArray<float> WantedLevelThresholds;

	/** 마지막 목격이 끊긴 뒤 열기가 줄기 시작할 때까지의 시간(초). */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨", meta = (ClampMin = "0.0", Units = "s"))
	float EvasionDelaySeconds;

	/** 수배 레벨 1당 회피 대기 시간에 더해지는 시간(초). 높은 수배일수록 오래 쫓긴다. */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨", meta = (ClampMin = "0.0", Units = "s"))
	float EvasionDelayPerLevel;

	/** 회피 상태에서 초당 줄어드는 열기. */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨", meta = (ClampMin = "0.0"))
	float HeatDecayPerSecond;

	/** 목격자가 있는 동안에도 열기가 줄어들지 여부. 끄면 시야 안에서는 절대 줄지 않는다. */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨")
	bool bDecayOnlyWhenUnseen;

	/** 열기 상한. 문턱값 최대치를 넘겨 둬야 최고 수배가 잠깐이라도 유지된다. */
	UPROPERTY(config, EditAnywhere, Category = "수배 레벨", meta = (ClampMin = "0.0"))
	float MaxHeat;

	/**
	 * 수배 레벨별 경찰 투입 규칙. 레벨당 한 줄씩 채운다.
	 * 비어 있으면 아무도 투입되지 않는다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "경찰 대응")
	TArray<FGunDayResponseTier> ResponseTiers;

	/**
	 * 플러그인이 직접 액터를 스폰할지 여부.
	 * 끄면 OnResponseTierChanged 만 울린다. 킷의 스포너를 쓰고 싶을 때 끈다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "경찰 대응")
	bool bSpawnResponders;

	/** 수배가 풀리면 투입했던 인원을 정리한다. */
	UPROPERTY(config, EditAnywhere, Category = "경찰 대응")
	bool bDismissRespondersOnClear;

	/**
	 * 투입된 경찰이 플레이어 쪽으로 다가오게 한다.
	 * 끄면 그 자리에 선 채 킷 AI 가 플레이어를 인지할 때까지 기다린다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "경찰 대응")
	bool bDriveRespondersToPlayer;

	/** 이 거리보다 멀면 다가온다. 이 안에 들어오면 킷 AI 에 맡긴다. */
	UPROPERTY(config, EditAnywhere, Category = "경찰 대응", meta = (ClampMin = "0.0"))
	float ResponderEngageDistance;

	/** 다가오는 명령을 다시 내리는 간격(초). */
	UPROPERTY(config, EditAnywhere, Category = "경찰 대응", meta = (ClampMin = "0.1", Units = "s"))
	float ResponderRepathIntervalSeconds;

	/**
	 * 투입된 경찰을 목격자로 친다.
	 * 켜 두면 경찰이 살아 있는 동안 열기가 줄지 않는다. 떼어내거나 쓰러뜨려야 수배가 풀린다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "경찰 대응")
	bool bRespondersCountAsWitnesses;

	/**
	 * 플레이어가 입힌 피해를 자동으로 범죄로 신고한다.
	 * 끄면 콘솔이나 블루프린트에서 직접 ReportCrime 을 불러야 한다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "범죄 감지")
	bool bAutoReportCrimes;

	/** 새로 생긴 폰에 피해 이벤트를 거는 주기(초). 짧을수록 반응이 빠르고 비용이 든다. */
	UPROPERTY(config, EditAnywhere, Category = "범죄 감지", meta = (ClampMin = "0.1", Units = "s"))
	float PawnRescanIntervalSeconds;

	/** 같은 상대에 대한 부상 신고 간격(초). 연사 한 번에 수배가 치솟는 것을 막는다. */
	UPROPERTY(config, EditAnywhere, Category = "범죄 감지", meta = (ClampMin = "0.0", Units = "s"))
	float InjuryReportCooldownSeconds;

	/** 발포 신고 간격(초). ReportPlayerGunfire 에 걸린다. */
	UPROPERTY(config, EditAnywhere, Category = "범죄 감지", meta = (ClampMin = "0.0", Units = "s"))
	float GunfireReportCooldownSeconds;

	/**
	 * 경찰로 칠 액터 클래스. 여기 든 상대를 쏘면 시민이 아니라 경찰로 신고된다.
	 * 투입 명단에 있는 상대는 이 목록이 비어 있어도 경찰로 친다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "범죄 감지")
	TArray<TSoftClassPtr<AActor>> PoliceClasses;

	/** 총성에 시민이 반응하게 한다. */
	UPROPERTY(config, EditAnywhere, Category = "군중 반응")
	bool bEnableCrowdReaction;

	/** 총성이 들리는 반경. */
	UPROPERTY(config, EditAnywhere, Category = "군중 반응", meta = (ClampMin = "0.0"))
	float GunshotAlertRadius;

	/** 총성 처리 간격(초). 연사 한 번에 같은 시민을 수십 번 놀라게 하지 않는다. */
	UPROPERTY(config, EditAnywhere, Category = "군중 반응", meta = (ClampMin = "0.0", Units = "s"))
	float CrowdAlertCooldownSeconds;

	/** 한 번에 반응시킬 시민 수의 상한. 프레임을 지키기 위한 안전장치다. */
	UPROPERTY(config, EditAnywhere, Category = "군중 반응", meta = (ClampMin = "0"))
	int32 MaxAlertedCivilians;

	/** 놀란 시민이 신고하기까지 걸리는 시간(초). */
	UPROPERTY(config, EditAnywhere, Category = "군중 반응", meta = (ClampMin = "0.0", Units = "s"))
	float CivilianReportDelaySeconds;

	/** 신고 한 건이 올리는 열기. */
	UPROPERTY(config, EditAnywhere, Category = "군중 반응", meta = (ClampMin = "0.0"))
	float CivilianReportHeat;

	/** 신고한 시민을 목격자로 잡아 두는 시간(초). 이 동안에는 열기가 줄지 않는다. */
	UPROPERTY(config, EditAnywhere, Category = "군중 반응", meta = (ClampMin = "0.0", Units = "s"))
	float CivilianWitnessSeconds;

	/** 시민을 소리 반대쪽으로 달아나게 한다. 킷의 행동 트리와 다투면 끈다. */
	UPROPERTY(config, EditAnywhere, Category = "군중 반응")
	bool bDriveCivilianFlee;

	/** 달아나는 거리. */
	UPROPERTY(config, EditAnywhere, Category = "군중 반응", meta = (ClampMin = "0.0"))
	float CivilianFleeDistance;

	/**
	 * 시민으로 칠 폰 클래스. 비워 두면 플레이어와 경찰을 뺀 모든 폰을 시민으로 본다.
	 * 킷의 BP_AICharacterCivilian 을 넣어 두면 적까지 달아나는 일이 없다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "군중 반응")
	TArray<TSoftClassPtr<APawn>> CivilianClasses;

	/** 시민끼리의 시비를 일으킨다. */
	UPROPERTY(config, EditAnywhere, Category = "시비")
	bool bEnableDisputes;

	/** 새 시비를 일으킬지 판정하는 간격(초). */
	UPROPERTY(config, EditAnywhere, Category = "시비", meta = (ClampMin = "1.0", Units = "s"))
	float DisputeIntervalSeconds;

	/** 플레이어로부터 이 반경 안의 시비 지점과 시민만 본다. */
	UPROPERTY(config, EditAnywhere, Category = "시비", meta = (ClampMin = "0.0"))
	float DisputeSearchRadius;

	/** 동시에 진행할 수 있는 시비 수. */
	UPROPERTY(config, EditAnywhere, Category = "시비", meta = (ClampMin = "0"))
	int32 MaxActiveDisputes;

	/**
	 * 레벨에 시비 지점(AGunDayDisputeSpot)이 하나도 없을 때
	 * 플레이어 주변 시민 중 아무나 골라 시비를 일으킨다.
	 * 골목마다 자리를 놓고 나면 꺼도 된다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "시비")
	bool bStartDisputesWithoutSpots;

	/**
	 * 시비 상황 목록. 주차, 담배, 노인석, 편의점.
	 * 대사는 작품에 맞게 직접 고친다. 기본값은 구조를 보여 주기 위한 자리표다.
	 */
	UPROPERTY(config, EditAnywhere, Category = "시비")
	TArray<FGunDayDisputeScenario> DisputeScenarios;

	/** 게임을 시작할 때의 정. */
	UPROPERTY(config, EditAnywhere, Category = "정과 진영", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float StartingJeong;

	/** 아무 일도 없으면 돌아가는 기준값. 사람은 잊는다. */
	UPROPERTY(config, EditAnywhere, Category = "정과 진영", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float BaselineJeong;

	/** 기준값으로 돌아가는 속도(초당). */
	UPROPERTY(config, EditAnywhere, Category = "정과 진영", meta = (ClampMin = "0.0"))
	float JeongRecoveryPerSecond;

	/** 시비 끝에 총이 오갔을 때 깎이는 정. */
	UPROPERTY(config, EditAnywhere, Category = "정과 진영")
	float JeongOnDisputeShot;

	/** 누가 말려서 시비가 가라앉았을 때 오르는 정. */
	UPROPERTY(config, EditAnywhere, Category = "정과 진영")
	float JeongOnMediationSuccess;

	/** 플레이어가 사람을 죽였을 때 깎이는 정. */
	UPROPERTY(config, EditAnywhere, Category = "정과 진영")
	float JeongOnPlayerKill;

	/** 어느 축에서도 어느 편도 아닐 확률. 높을수록 무색한 사람이 많아진다. */
	UPROPERTY(config, EditAnywhere, Category = "정과 진영", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NeutralPositionChance;

	/** 축별 무게. 순서는 정치, 부동산, 세대, 지역, 성별. */
	UPROPERTY(config, EditAnywhere, Category = "정과 진영")
	TArray<float> FaultLineWeights;

	/** 진영이 다를수록 시비가 끝까지 갈 확률이 얼마나 오르는가. */
	UPROPERTY(config, EditAnywhere, Category = "정과 진영", meta = (ClampMin = "0.0"))
	float FrictionEscalationWeight;

	/** 정이 가득할 때 시비가 올라갈 확률에 곱하는 값. 작을수록 잘 가라앉는다. */
	UPROPERTY(config, EditAnywhere, Category = "정과 진영", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float JeongCalmFactor;

	/** 정이 가득할 때 누가 말리러 들어올 확률. */
	UPROPERTY(config, EditAnywhere, Category = "정과 진영", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MediationChanceAtFullJeong;

	/** 말리러 들어온 사람이 총에 맞을 확률. 선의는 가끔 처벌받는다. */
	UPROPERTY(config, EditAnywhere, Category = "정과 진영", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MediatorShotChance;

	/** 말리는 사람의 대사. */
	UPROPERTY(config, EditAnywhere, Category = "정과 진영")
	TArray<FString> MediationLines;
};
