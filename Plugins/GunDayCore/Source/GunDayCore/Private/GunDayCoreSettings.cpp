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
	, ResponderStuckSeconds(8.0f)
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
	, bSpawnPedestrians(true)
	, PedestrianCount(12)
	, PedestrianMinSpawnDistance(2000.0f)
	, PedestrianMaxSpawnDistance(5000.0f)
	, PedestrianDespawnDistance(7000.0f)
	, PedestrianSpawnIntervalSeconds(0.4f)
	, bDrivePedestrianWander(true)
	, PedestrianWanderRadius(2500.0f)
	, PedestrianIdleMinSeconds(1.0f)
	, PedestrianIdleMaxSeconds(6.0f)
	, bEnableDisputes(true)
	, DisputeIntervalSeconds(25.0f)
	, DisputeSearchRadius(4000.0f)
	, DisputeLineIntervalSeconds(2.0f)
	, MaxActiveDisputes(2)
	, bStartDisputesWithoutSpots(true)
	, bAmbientSparkDisputes(true)
	, bSpawnDisputeCast(true)
	, DisputeCastSpawnDistance(5000.0f)
	, DisputeCastDespawnDistance(8000.0f)
	, DisputeCastSpacing(160.0f)
	, DisputeCastStartDistance(2000.0f)
	, StartingJeong(45.0f)
	, BaselineJeong(45.0f)
	, JeongRecoveryPerSecond(0.05f)
	, JeongOnDisputeShot(-3.0f)
	, JeongOnMediationSuccess(2.0f)
	, JeongOnPlayerKill(-5.0f)
	, NeutralPositionChance(0.3f)
	, FrictionEscalationWeight(0.5f)
	, JeongCalmFactor(0.35f)
	, MediationChanceAtFullJeong(0.7f)
	, MediatorShotChance(0.25f)
	, bEnableNews(true)
	, NewsIntervalSeconds(35.0f)
	, DayLengthSeconds(600.0f)
	, BreakingNewsChance(0.35f)
	, LowJeongNewsThreshold(25.0f)
{
	CrimeHeat.Add(EGunDayCrime::Brandishing, 10.0f);
	CrimeHeat.Add(EGunDayCrime::PublicGunfire, 25.0f);
	CrimeHeat.Add(EGunDayCrime::CivilianInjured, 30.0f);
	CrimeHeat.Add(EGunDayCrime::CivilianKilled, 70.0f);
	CrimeHeat.Add(EGunDayCrime::PoliceInjured, 90.0f);
	CrimeHeat.Add(EGunDayCrime::PoliceKilled, 160.0f);
	CrimeHeat.Add(EGunDayCrime::VehicleTheft, 20.0f);
	CrimeHeat.Add(EGunDayCrime::PropertyDamage, 8.0f);

	// 킷 v2.2 의 이름. 실행 중 "킷 구성:" 로그로 BP_AICharacterBase 에서 확인했다(2026-10-10).
	KitHitEventNames = { FName(TEXT("AI Get Damage Dispatcher")) };
	KitDeathEventNames = { FName(TEXT("AI Died")) };
	KitHealthPropertyNames = { FName(TEXT("AI's health")) };
	KitDamageAttributionRadius = 6000.0f;

	// 레벨 1~5 문턱값.
	WantedLevelThresholds = { 20.0f, 80.0f, 180.0f, 320.0f, 500.0f };

	// 뉴스 문구. 전부 자리표다. 작품의 목소리로 바꿔 쓴다.
	BreakingTemplates = {
		TEXT("[속보] 시내 모처서 총기 사고. 경찰 조사 중."),
		TEXT("[속보] 또 총격. 오늘만 {오늘}번째."),
		TEXT("[속보] 주차 시비 끝 발포. 목격자 \"늘 있는 일\"."),
		TEXT("[속보] 총성 신고 접수. 출동 시간 평균 14분."),
	};

	PlayerBreakingTemplates = {
		TEXT("[속보] 시민 1명 총기 사용. 신원 확인 중."),
		TEXT("[속보] 목격자 \"그냥 지나가던 사람이었다\"."),
		TEXT("[속보] 경찰, 용의자 추적 중. 시민들은 일상 유지."),
	};

	DailyTemplates = {
		TEXT("{날}일차 집계 — 오늘 총기 사망 {오늘}명, 어제보다 {차이}명 {증감}."),
		TEXT("{날}일차 — 누적 사망 {총}명. 시비 신고 {시비}건."),
		TEXT("{날}일차 집계 — 사망 {오늘}명. 전문가 \"예년 수준\"."),
	};

	FillerTemplates = {
		TEXT("오늘의 날씨, 맑음. 외출 시 안전에 유의하시기 바랍니다."),
		TEXT("정부, 총기 안전 캠페인 예산 전년 대비 3% 증액."),
		TEXT("보험업계 \"총기 특약 가입 문의 급증\"."),
		TEXT("전문가 \"총이 문제가 아니라 사람이 문제\"."),
		TEXT("다음 주 금리 동결 전망. 부동산 시장은 관망세."),
		TEXT("방탄 조끼 할인전. 가정의 달 맞이 2+1."),
		TEXT("국회, 총기법 개정안 논의 92일째 공전."),
		TEXT("시민 설문 — \"이웃을 신뢰한다\" 19%, 역대 최저."),
		TEXT("프로야구 소식입니다. 어제 경기는 정상 진행됐습니다."),
	};

	LowJeongTemplates = {
		TEXT("시민 체감 안전도 조사, 응답률 저조로 중단."),
		TEXT("\"요즘은 눈도 안 마주친다\" — 거리 인터뷰."),
		TEXT("장례업계 호황. 관련주 사흘째 상승."),
		TEXT("정부 \"사회 통합\" 표어 공모. 상금 300만원."),
		TEXT("오늘도 평온한 하루였습니다. 내일 뵙겠습니다."),
	};

	// 축별 무게. 정치, 부동산, 세대, 지역, 성별.
	FaultLineWeights = { 1.0f, 1.0f, 1.0f, 0.7f, 0.8f };

	// 말리는 사람의 대사. 이것도 자리표다.
	MediationLines = {
		TEXT("아유 됐어 그만해. 그만하라고."),
		TEXT("여기서 이러지들 마요."),
		TEXT("경찰 불러요 그냥. 네?"),
		TEXT("두 분 다 진정하시고."),
	};

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

		// 대기형. 한쪽은 몇 달 전에 이미 결심하고 왔다.
		// 조롱의 대상은 환자가 아니라 발뺌의 언어다.
		FGunDayDisputeScenario Clinic;
		Clinic.Name = TEXT("병원 대기실");
		Clinic.Shape = EGunDayDisputeShape::Grievance;
		Clinic.VerbalLines = {
			TEXT("원장님 언제 나오세요. 세 시간째 기다리는데."),
			TEXT("예약 없이 오시면 상담이 어렵습니다."),
			TEXT("수술한 사람이 나한테 예약을 하래."),
			TEXT("그건 제 담당이 아니라서요."),
		};
		Clinic.ShovingLines = {
			TEXT("이거 보세요. 이게 사람 얼굴입니까."),
			TEXT("경과는 개인차가 있습니다. 체질 문제도 있고요."),
			TEXT("체질? 내 체질이 이렇게 만들었다고?"),
		};
		Clinic.DrawnLines = {
			TEXT("…선생님, 진정하시고 앉으세요."),
			TEXT("법적으로 가시면 됩니다. 저희도 변호사가 있고요."),
			TEXT("세 번째 병원이야. 다들 똑같은 소리를 해."),
		};
		Clinic.ApologyLines = {
			TEXT("…제가 집도했습니다. 죄송합니다."),
			TEXT("다시 봐 드리겠습니다. 비용은 받지 않겠습니다."),
		};
		// 개인 원한이라 진영은 거의 상관없고, 말린다고 멎지 않는다.
		Clinic.FrictionInfluence = 0.1f;
		Clinic.MediationEffectiveness = 0.2f;
		Clinic.ApologyChance = 0.15f;
		Clinic.EscalateChance = 0.85f;
		Clinic.FireChance = 0.7f;
		Clinic.StageSeconds = 6.0f;
		DisputeScenarios.Add(Clinic);

		// 대기형. 돈이 묶인 사람과 서류의 언어.
		FGunDayDisputeScenario Lease;
		Lease.Name = TEXT("전세 사기");
		Lease.Shape = EGunDayDisputeShape::Grievance;
		Lease.VerbalLines = {
			TEXT("보증금 언제 주실 겁니까. 이사 날짜가 지났는데."),
			TEXT("저희도 피해자예요. 집주인이 연락이 안 돼서."),
			TEXT("계약서에 중개사 도장 찍혀 있잖아요."),
			TEXT("그건 단순 중개였고요, 책임 범위가 다릅니다."),
		};
		Lease.ShovingLines = {
			TEXT("내 전 재산이에요. 전 재산."),
			TEXT("그래서 보증보험 드시라고 말씀드렸는데."),
			TEXT("들라고 한 적 없잖아. 괜찮다고 했잖아."),
		};
		Lease.DrawnLines = {
			TEXT("…사장님, 이러시면 더 복잡해집니다."),
			TEXT("소송하시면 됩니다. 삼 년이면 나와요."),
			TEXT("삼 년. 애가 학교를 어떻게 다니는데."),
		};
		Lease.ApologyLines = {
			TEXT("…제가 알고 있었습니다. 변제 계획을 쓰겠습니다."),
			TEXT("오늘 안에 집주인 데려오겠습니다. 약속합니다."),
		};
		Lease.FrictionInfluence = 0.15f;
		Lease.MediationEffectiveness = 0.25f;
		Lease.ApologyChance = 0.12f;
		Lease.EscalateChance = 0.85f;
		Lease.FireChance = 0.7f;
		Lease.StageSeconds = 6.0f;
		DisputeScenarios.Add(Lease);

		// 대기형. 당사자는 어른들이다. 조롱의 대상은 처리 절차의 언어다.
		FGunDayDisputeScenario School;
		School.Name = TEXT("학폭 처리");
		School.Shape = EGunDayDisputeShape::Grievance;
		School.VerbalLines = {
			TEXT("심의위원회가 세 번 연기됐습니다. 세 번이요."),
			TEXT("절차가 있어서요. 저희도 규정대로 해야 합니다."),
			TEXT("우리 애는 지금 학교를 못 가요."),
			TEXT("전학 권고도 방법입니다. 아이를 위해서."),
		};
		School.ShovingLines = {
			TEXT("왜 우리가 전학을 가요. 왜."),
			TEXT("쌍방으로 처리되면 양쪽 다 기록이 남습니다."),
			TEXT("쌍방? 맞은 쪽이랑 때린 쪽이 쌍방이에요?"),
		};
		School.DrawnLines = {
			TEXT("…어머님, 여기 학교입니다."),
			TEXT("서면 사과 권고로 종결하는 게 서로 좋습니다."),
			TEXT("서면. 종이 한 장."),
		};
		School.ApologyLines = {
			TEXT("…제가 미뤘습니다. 이번 주에 열겠습니다."),
			TEXT("기록 남기고 정식으로 처리하겠습니다. 죄송합니다."),
		};
		School.FrictionInfluence = 0.1f;
		School.MediationEffectiveness = 0.3f;
		School.ApologyChance = 0.18f;
		School.EscalateChance = 0.8f;
		School.FireChance = 0.6f;
		School.StageSeconds = 6.0f;
		DisputeScenarios.Add(School);

		// 대기형. 다친 사람과 서류상 소속의 언어.
		FGunDayDisputeScenario Industrial;
		Industrial.Name = TEXT("산재 은폐");
		Industrial.Shape = EGunDayDisputeShape::Grievance;
		Industrial.VerbalLines = {
			TEXT("산재 신청서에 확인 도장 언제 찍어 주십니까."),
			TEXT("공상으로 처리하자고 말씀드렸잖아요. 그게 빠릅니다."),
			TEXT("빠른 게 아니고 기록이 안 남는 거죠."),
			TEXT("선생님은 저희 소속이 아니라서요. 하청에 말씀하세요."),
		};
		Industrial.ShovingLines = {
			TEXT("손가락이에요. 세 개."),
			TEXT("안전교육 이수 서류에 서명하셨고요."),
			TEXT("그 서류 받은 날이 사고 다음 날이잖아."),
		};
		Industrial.DrawnLines = {
			TEXT("…여기 현장입니다. 사람들 봅니다."),
			TEXT("재해 경위상 본인 부주의 비율이 있습니다."),
			TEXT("부주의. 열두 시간째 서 있었는데."),
		};
		Industrial.ApologyLines = {
			TEXT("…제가 막았습니다. 오늘 도장 찍겠습니다."),
			TEXT("원청에 보고하겠습니다. 소속도 바로잡겠습니다."),
		};
		Industrial.FrictionInfluence = 0.1f;
		Industrial.MediationEffectiveness = 0.25f;
		Industrial.ApologyChance = 0.14f;
		Industrial.EscalateChance = 0.82f;
		Industrial.FireChance = 0.65f;
		Industrial.StageSeconds = 6.0f;
		DisputeScenarios.Add(Industrial);

		// 대기형. 약관의 언어. 숫자와 조항으로 사람을 돌려보낸다.
		FGunDayDisputeScenario Insurance;
		Insurance.Name = TEXT("보험금 거절");
		Insurance.Shape = EGunDayDisputeShape::Grievance;
		Insurance.VerbalLines = {
			TEXT("부지급 통보서 받았습니다. 설명 좀 해 주세요."),
			TEXT("심사 결과가 그렇게 나왔습니다. 저는 전달만 합니다."),
			TEXT("십이 년을 냈어요. 한 번도 안 밀리고."),
			TEXT("납입 이력과 지급 심사는 별개 사안입니다."),
		};
		Insurance.ShovingLines = {
			TEXT("기왕증이라니. 그런 말은 처음 듣는데."),
			TEXT("고지의무 위반 소지가 있어서요. 약관 제3조."),
			TEXT("그 약관 설명해 준 사람 아무도 없었어요."),
		};
		Insurance.DrawnLines = {
			TEXT("…고객님, CCTV 돌아갑니다."),
			TEXT("분쟁조정 신청하시면 됩니다. 평균 칠 개월이고요."),
			TEXT("칠 개월. 수술은 다음 주인데."),
		};
		Insurance.ApologyLines = {
			TEXT("…재심사 올리겠습니다. 제 이름으로 올립니다."),
			TEXT("약관 설명 누락이 맞습니다. 지급으로 정리하겠습니다."),
		};
		Insurance.FrictionInfluence = 0.1f;
		Insurance.MediationEffectiveness = 0.3f;
		Insurance.ApologyChance = 0.16f;
		Insurance.EscalateChance = 0.8f;
		Insurance.FireChance = 0.6f;
		Insurance.StageSeconds = 6.0f;
		DisputeScenarios.Add(Insurance);
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
