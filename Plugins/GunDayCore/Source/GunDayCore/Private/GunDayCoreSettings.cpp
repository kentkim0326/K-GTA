// Copyright K-GTA. All Rights Reserved.

#include "GunDayCoreSettings.h"

UGunDayCoreSettings::UGunDayCoreSettings()
	: EvasionDelaySeconds(12.0f)
	, EvasionDelayPerLevel(6.0f)
	, HeatDecayPerSecond(8.0f)
	, bDecayOnlyWhenUnseen(true)
	, MaxHeat(600.0f)
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
}
