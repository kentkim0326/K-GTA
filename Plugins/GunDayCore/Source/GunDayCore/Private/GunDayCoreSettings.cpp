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
	, bDriveRespondersToPlayer(true)
	, ResponderEngageDistance(1200.0f)
	, ResponderRepathIntervalSeconds(2.0f)
	, bRespondersCountAsWitnesses(true)
	, bAutoReportCrimes(true)
	, PawnRescanIntervalSeconds(2.0f)
	, InjuryReportCooldownSeconds(1.5f)
	, GunfireReportCooldownSeconds(1.0f)
	, bEnableCrowdReaction(true)
	, GunshotAlertRadius(3000.0f)
	, CrowdAlertCooldownSeconds(0.75f)
	, MaxAlertedCivilians(24)
	, CivilianReportDelaySeconds(6.0f)
	, CivilianReportHeat(15.0f)
	, CivilianWitnessSeconds(20.0f)
	, bDriveCivilianFlee(true)
	, CivilianFleeDistance(2500.0f)
	, bEnableDisputes(true)
	, DisputeIntervalSeconds(25.0f)
	, DisputeSearchRadius(4000.0f)
	, MaxActiveDisputes(2)
	, bStartDisputesWithoutSpots(true)
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

	// 시비 상황 기본값. 대사는 자리표다. 작품에 맞게 바꿔 쓴다.
	{
		FGunDayDisputeScenario Parking;
		Parking.Name = TEXT("주차 시비");
		Parking.VerbalLines = {
			TEXT("여기다 대면 어떡해요. 차 빼요."),
			TEXT("잠깐 세운 거예요. 뭘 그렇게 따져."),
			TEXT("잠깐이 삼십 분째야."),
		};
		Parking.ShovingLines = {
			TEXT("손대지 마세요. 진짜."),
			TEXT("뭐 어쩔 건데. 쳐 봐."),
		};
		Parking.DrawnLines = {
			TEXT("…내려놔. 내려놓으라고."),
			TEXT("차 뺄 거야 말 거야."),
		};
		DisputeScenarios.Add(Parking);

		FGunDayDisputeScenario Smoking;
		Smoking.Name = TEXT("담배 훈계");
		Smoking.VerbalLines = {
			TEXT("학생, 여기서 담배 피우면 안 돼."),
			TEXT("아저씨가 뭔 상관인데요."),
			TEXT("부모가 안 가르쳤니."),
		};
		Smoking.ShovingLines = {
			TEXT("놔요. 놓으라고."),
			TEXT("어디서 어른한테."),
		};
		Smoking.DrawnLines = {
			TEXT("…야, 그거 진짜야?"),
			TEXT("한 발만 더 와 봐요."),
		};
		Smoking.EscalateChance = 0.6f;
		DisputeScenarios.Add(Smoking);

		FGunDayDisputeScenario BusSeat;
		BusSeat.Name = TEXT("노인석");
		BusSeat.VerbalLines = {
			TEXT("거기 노약자석이야. 일어나."),
			TEXT("빈자리길래 앉았는데요."),
			TEXT("눈이 없어? 글씨가 안 보여?"),
		};
		BusSeat.ShovingLines = {
			TEXT("왜 밀어요 지금."),
			TEXT("요즘 것들은 말로 하면 안 들어."),
		};
		BusSeat.DrawnLines = {
			TEXT("할머니, 그거 치우세요."),
			TEXT("내가 못 쏠 것 같아?"),
		};
		BusSeat.StageSeconds = 5.0f;
		DisputeScenarios.Add(BusSeat);

		FGunDayDisputeScenario Store;
		Store.Name = TEXT("편의점");
		Store.VerbalLines = {
			TEXT("여기서 그렇게 소리 내고 드시면 어떡해요."),
			TEXT("라면 먹는 데 소리도 못 내나."),
			TEXT("한 번만 말할게요. 조용히 하세요."),
		};
		Store.ShovingLines = {
			TEXT("국물 쏟았잖아 지금."),
			TEXT("닦아. 네가 닦아."),
		};
		Store.DrawnLines = {
			TEXT("…형, 진정하고."),
			TEXT("라면값 내고 가."),
		};
		Store.FireChance = 0.5f;
		DisputeScenarios.Add(Store);
	}

	// 투입 인원 기본값. ResponderClass 는 비워 둔다.
	// 킷의 적 블루프린트를 프로젝트 세팅에서 직접 골라야 실제로 스폰된다.
	for (int32 Level = 1; Level <= WantedLevelThresholds.Num(); ++Level)
	{
		FGunDayResponseTier Tier;
		Tier.WantedLevel = Level;
		Tier.DesiredCount = Level * 2;
		Tier.SpawnIntervalSeconds = FMath::Max(1.0f, 4.0f - Level * 0.5f);
		Tier.MinSpawnDistance = 900.0f;
		Tier.MaxSpawnDistance = 2500.0f;
		ResponseTiers.Add(Tier);
	}
}
