// Copyright K-GTA. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UGunDayWantedSubsystem;

/**
 * 수배 레벨 시스템을 눈으로 확인하기 위한 디버그 도구.
 * 블루프린트 에셋 없이 콘솔(물결표 키)만으로 쓴다.
 *
 *   GunDay.ShowDebug 1      화면 좌상단에 수배 상태를 띄운다
 *   GunDay.Fire             공공장소 발포 한 번
 *   GunDay.ReportCrime PoliceKilled
 *   GunDay.AddHeat 50
 *   GunDay.SetWanted 3
 *   GunDay.Witness 1        목격자 수를 1 늘린다 (-1 이면 줄인다)
 *   GunDay.Clear            수배 해제
 *   GunDay.Police.Dismiss   투입된 경찰 전부 치우기
 *   GunDay.Police.Enabled 0 경찰 투입 끄기
 *   GunDay.AutoReport 0     피해 자동 신고 끄기
 *   GunDay.Crowd.Gunshot    주변 시민 흩어지게 하기
 *   GunDay.Crowd.Calm       놀란 시민 진정시키기
 *   GunDay.Alley.Start      1분 총격전 측정 시작
 *   GunDay.Alley.Stop       측정 중단
 *   GunDay.Alley.Result     지난 판 결과 다시 보기
 *   GunDay.Dispute.Start    주변 시민 둘로 시비 일으키기
 *   GunDay.Dispute.Clear    진행 중인 시비 끝내기
 *   GunDay.Jeong 80         사회의 정을 80 으로
 *   GunDay.Profile          주변 사람들의 진영 보기
 *   GunDay.News.Next        뉴스 한 줄 내보내기
 *   GunDay.News.Day         하루 넘겨 일일 집계 보기
 */
namespace GunDayDebug
{
	/** GunDay.ShowDebug 가 켜져 있는가. */
	bool IsHUDEnabled();

	/** 화면에 수배 상태를 한 프레임 그린다. */
	void DrawHUD(const UGunDayWantedSubsystem& Subsystem);
}
