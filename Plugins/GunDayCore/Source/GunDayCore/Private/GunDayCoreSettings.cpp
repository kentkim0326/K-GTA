// Copyright K-GTA. All Rights Reserved.

#include "GunDayCoreSettings.h"

UGunDayCoreSettings::UGunDayCoreSettings()
	: EvasionDelaySeconds(12.0f)
	, EvasionDelayPerLevel(6.0f)
	, HeatDecayPerSecond(8.0f)
	, bDecayOnlyWhenUnseen(true)
	, MaxHeat(600.0f)
	, bSpawnResponders(true)
	, bDismissRespondersOnClear(true)
	, bRespondersCountAsWitnesses(true)
	, bAutoReportCrimes(true)
	, PawnRescanIntervalSeconds(2.0f)
	, InjuryReportCooldownSeconds(1.5f)
	, GunfireReportCooldownSeconds(1.0f)
{
	CrimeHeat.Add(EGunDayCrime::Brandishing, 10.0f);
	CrimeHeat.Add(EGunDayCrime::PublicGunfire, 25.0f);
	CrimeHeat.Add(EGunDayCrime::CivilianInjured, 30.0f);
	CrimeHeat.Add(EGunDayCrime::CivilianKilled, 70.0f);
	CrimeHeat.Add(EGunDayCrime::PoliceInjured, 90.0f);
	CrimeHeat.Add(EGunDayCrime::PoliceKilled, 160.0f);
	CrimeHeat.Add(EGunDayCrime::VehicleTheft, 20.0f);
	CrimeHeat.Add(EGunDayCrime::PropertyDamage, 8.0f);

	// 레벨 1~5 문턱값.
	WantedLevelThresholds = { 20.0f, 80.0f, 180.0f, 320.0f, 500.0f };

	// 투입 인원 기본값. ResponderClass 는 비워 둔다.
	// 킷의 적 블루프린트를 프로젝트 세팅에서 직접 골라야 실제로 스폰된다.
	for (int32 Level = 1; Level <= WantedLevelThresholds.Num(); ++Level)
	{
		FGunDayResponseTier Tier;
		Tier.WantedLevel = Level;
		Tier.DesiredCount = Level * 2;
		Tier.SpawnIntervalSeconds = FMath::Max(1.0f, 4.0f - Level * 0.5f);
		ResponseTiers.Add(Tier);
	}
}
