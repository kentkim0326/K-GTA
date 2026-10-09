# K-GTA 작업 지침

Unreal Engine 5.8 프로젝트. 배경과 결정 사항은 `docs/handover-2026-09-10.md` 를 먼저 읽는다.

## 지켜야 할 것

- **킷 파일을 고치지 않는다.** Third Person Shooter Kit 은 구매한 물건이고 버전이 올라간다.
  자체 로직은 전부 `Plugins/GunDayCore` 에 C++ 로 넣고, 킷 블루프린트가 부를 수 있게 노출한다.
- **이미 킷에 있는 것을 새로 만들지 않는다.** 모션 매칭, 엄폐, 인카운터, AI 스포너,
  적 프리셋, 무기·피격 시스템, 메타휴먼은 이미 들어 있다. 미션 시스템은 킷 v2.3 을 기다린다.
- **`.uasset` 과 `.umap` 은 Git LFS 를 거친다.** `.gitattributes` 를 먼저 확인하고 커밋한다.
- **자체 에셋은 `Content/KGTA/` 아래에만 만든다.** `Content` 의 나머지는 킷 원본이고
  용량(약 12 GB) 때문에 버전 관리에서 제외했다. 킷 에셋을 고쳐야 하면 `Content/KGTA/` 로
  복제한 뒤 복제본을 쓴다.
- **경로에 한글과 띄어쓰기를 쓰지 않는다.** 언리얼이 원인 불명 오류를 낸다.
- **작가 사이트 저장소(`kentkim0326/kentkim`)와 섞지 않는다.**

## 코드 규칙

- 모듈 접두사 `GUNDAYCORE_API`, 로그 카테고리 `LogGunDay`.
- 블루프린트에서 부를 것은 `UFUNCTION(BlueprintCallable)` 또는 `BlueprintPure`,
  카테고리는 `GunDay|<영역>`.
- 튜닝 값은 코드에 상수로 박지 말고 `UGunDayCoreSettings` 에 `UPROPERTY(config, EditAnywhere)` 로 낸다.
- 주석과 커밋 메시지는 한국어로 쓴다.

## 우선순위

1. 수배 레벨 시스템 (완료 — `UGunDayWantedSubsystem`)
2. 경찰 대응 배선 (완료 — `UGunDayPoliceResponseSubsystem`)
3. 군중 반응
4. 골목 하나에서 경찰 둘과 1분간 총격전 — 이것이 첫 마일스톤이다
