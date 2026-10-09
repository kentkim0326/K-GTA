# K-GTA

김형섭 소설 《총기허용의 날》을 원작으로 한 3인칭 오픈월드 액션 게임.
`K-GTA` 는 **작업용 이름**이다. GTA 는 Take-Two 의 상표이므로 공개 배포 시 다른 이름을 쓴다.

| 항목 | 값 |
|---|---|
| 엔진 | Unreal Engine 5.8 |
| 베이스 | Third Person Shooter Kit v2.2 (Marcin Matuszczyk, 구매 완료) |
| 자체 코드 | `Plugins/GunDayCore` |
| 플랫폼 | Windows |
| 프로젝트 | `GunsKorea.uproject` |

인계 노트 전문은 [`docs/handover-2026-09-10.md`](docs/handover-2026-09-10.md) 에 있다.

---

## 처음 한 번 (PC에서)

언리얼 프로젝트 이름은 **GunsKorea** (UE 5.8). 저장소 이름 `K-GTA` 는 작업용 식별자일 뿐이고
프로젝트 폴더 이름과 같을 필요는 없다.

### 0. 프로젝트 폴더를 옮긴다

현재 위치: `C:\Users\LEMON AG\Documents\Unreal Projects\GunsKorea`

여기 그대로 두면 두 가지가 걸린다.

1. **`Documents` 는 OneDrive 동기화 대상인 경우가 많다.** 언리얼 프로젝트는 수십 기가이고
   `Intermediate`, `DerivedDataCache` 가 빌드할 때마다 수만 개 파일을 쏟아낸다.
   OneDrive 가 이걸 계속 업로드하려 들면 빌드가 느려지고 파일 잠금 오류가 난다.
2. **경로에 띄어쓰기가 두 군데 있다** (`LEMON AG`, `Unreal Projects`).
   엔진은 대개 견디지만 일부 빌드·패키징 툴과 셰이더 컴파일러가 걸린다.

에디터를 닫고 폴더째 옮긴다.

```bat
mkdir C:\Dev
move "C:\Users\LEMON AG\Documents\Unreal Projects\GunsKorea" C:\Dev\GunsKorea
```

옮긴 뒤 `C:\Dev\GunsKorea\GunsKorea.uproject` 를 더블클릭하면 그대로 열린다.
언리얼은 절대 경로를 저장하지 않는다. SSD 가 D 드라이브라면 `D:\Dev\GunsKorea` 가 더 낫다.

### 1. Git LFS 먼저

`.uasset` 을 LFS 없이 한 번이라도 커밋하면 이력을 다시 써야 한다. **순서를 지킨다.**

```bat
git lfs install
```

### 2. 저장소를 프로젝트 폴더에 붙인다

```bat
cd /d C:\Dev\GunsKorea
git init
git remote add origin https://github.com/kentkim0326/K-GTA.git
git fetch origin
git checkout -b main origin/main
```

`.gitignore`, `.gitattributes`, `Plugins/GunDayCore` 가 프로젝트 폴더로 내려온다.

### 3. 첫 커밋 전에 확인

```bat
git lfs track
git add -A
git status
```

`Binaries`, `Intermediate`, `Saved`, `DerivedDataCache` 가 목록에 없어야 한다.
`.uasset` 은 `git check-attr filter -- Content/어떤파일.uasset` 이 `lfs` 를 찍어야 한다.

### 4. 플러그인 활성화

프로젝트를 열면 `GunDayCore` 가 Plugins 목록에 잡힌다.
C++ 모듈이라 처음 열 때 빌드를 묻는다. 예를 누른다.
Visual Studio 2022 와 Windows SDK 가 필요하다.

---

## GunDayCore

킷을 직접 고치지 않기 위한 분리 플러그인. 킷이 버전을 올려도 덮어써지지 않는다.

### 수배 레벨 시스템

`UGunDayWantedSubsystem` — 게임 인스턴스 서브시스템. 블루프린트에서 `Get Wanted Subsystem` 으로 잡는다.

열기(heat)라는 숫자 하나가 쌓이고 줄어들며, 수배 레벨은 거기서 파생된다.

| 호출 | 언제 |
|---|---|
| `Report Crime` | 발포, 시민 사살, 경찰 사살 등. 가중치는 프로젝트 세팅에서 조정 |
| `Add Witness` / `Remove Witness` | AI Perception 이 플레이어를 보고 놓칠 때. 짝을 맞춰 부른다 |
| `Clear Wanted` | 체포, 사망, 세이프하우스 진입 |
| `On Wanted Level Changed` | 경찰 스포너와 인카운터가 여기에 붙는다 |
| `Get Heat Fraction To Next Level` | HUD 게이지 |

보는 눈이 하나라도 있으면 열기는 줄지 않는다. 목격이 끊기고 회피 대기 시간이 지나야
초당 일정량씩 빠진다. 대기 시간은 수배 레벨이 높을수록 길어진다.

값은 전부 **프로젝트 세팅 > Game > GunDay Core** 에서 만진다. 코드 수정 없이 조율한다.

### 다음

1. 경찰 대응 배선 — 수배 레벨별로 어떤 AI 스포너와 인카운터를 호출할지. 인지는 AI Perception, 판단은 StateTree.
2. 군중 반응 — 총성에 시민이 흩어지고 신고한다. Mass Entity 는 나중에.

첫 목표는 **골목 하나에서 경찰 둘과 1분간 총격전**이다. 도시도 차도 미션도 그 뒤다.
