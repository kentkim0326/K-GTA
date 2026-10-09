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

### 4. 킷 콘텐츠는 저장소에 없다

`Content` 의 킷 원본 약 12 GB 는 `.gitignore` 로 제외했다. GitHub 무료 LFS 한도가 1 GB 다.
저장소에는 코드와 설정, 그리고 `Content/KGTA/` 아래 자체 에셋만 들어간다.

PC 를 바꾸거나 처음부터 다시 깔 때의 순서는 이렇다.

1. Epic Games Launcher 에서 Third Person Shooter Kit 으로 프로젝트를 새로 만든다 (UE 5.8).
2. 그 폴더에서 위 2번 절차대로 이 저장소를 붙인다.

자체 에셋은 전부 `Content/KGTA/` 아래에 만든다. 킷 에셋을 고쳐야 하면 거기로 복제해서 쓴다.

### 5. 플러그인 활성화

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

### 디버그 HUD 와 콘솔 명령

블루프린트 배선 없이 수배 레벨 시스템을 바로 확인할 수 있다.
플레이 중 물결표(`~`) 키로 콘솔을 열고 입력한다.

| 명령 | 하는 일 |
|---|---|
| `GunDay.ShowDebug 1` | 화면에 수배 레벨, 열기, 감소 상태를 띄운다 |
| `GunDay.Fire` | 공공장소 발포 한 번 |
| `GunDay.ReportCrime PoliceKilled` | 범죄 한 건. 이름 대신 번호도 된다 |
| `GunDay.AddHeat 50` | 열기를 직접 더한다. 음수면 깎인다 |
| `GunDay.SetWanted 3` | 수배 레벨을 강제로 맞춘다 |
| `GunDay.Witness 1` | 목격자를 늘린다. `-1` 이면 줄인다 |
| `GunDay.Clear` | 수배와 목격자를 전부 지운다 |
| `GunDay.Police.Dismiss` | 투입된 경찰을 전부 치운다 |
| `GunDay.Police.Enabled 0` | 경찰 투입을 끈다 |

확인해 볼 흐름은 이렇다. `GunDay.ShowDebug 1` 로 HUD 를 켜고 `GunDay.Fire` 를 몇 번 치면
열기가 쌓이고 별이 늘어난다. 그대로 두면 회피 대기 시간이 지난 뒤 열기가 줄고 별이 빠진다.
`GunDay.Witness 1` 로 목격자를 붙여 두면 줄지 않는다. `GunDay.Witness -1` 로 떼면 다시 준다.

### 경찰 대응 배선

`UGunDayPoliceResponseSubsystem` — 월드 서브시스템. 수배 레벨이 바뀌면 그 레벨의 규칙을 찾아
살아 있어야 할 인원을 맞춘다. 모자라면 간격을 두고 한 명씩 투입하고, 죽거나 멀어진 인원은 정리한다.

규칙은 **프로젝트 세팅 > Game > GunDay Core > 경찰 대응** 의 Response Tiers 표에서 정한다.
레벨당 한 줄이고, 기본값으로 레벨 1~5 가 2·4·6·8·10 명으로 들어가 있다.

| 항목 | 뜻 |
|---|---|
| Responder Class | 투입할 액터. **킷의 적 블루프린트를 여기서 고른다.** 비우면 스폰하지 않는다 |
| Desired Count | 그 레벨에서 유지할 인원 |
| Spawn Interval | 한 명과 다음 한 명 사이의 간격(초) |
| Min / Max Spawn Distance | 플레이어로부터 이 범위 안의 내비메시 위에 생성한다 |
| Despawn Distance | 이보다 멀어진 인원은 정리한다 |

생성 위치는 내비메시 위에서 고르고 플레이어 시야 밖을 우선한다. **레벨에 NavMeshBoundsVolume 이 없으면
아무도 생성되지 않는다.** 로그에 경고가 찍힌다.

#### 킷 스포너를 쓰고 싶다면

프로젝트 세팅에서 `Spawn Responders` 를 끄고 `On Response Tier Changed` 에 블루프린트를 붙인다.
수배 레벨과 필요 인원이 넘어오므로 킷의 AI 스포너를 호출한 뒤, 스폰 결과를 `Register Responder` 로
등록하면 인원 계산과 정리에 함께 들어간다.

### 다음

1. 군중 반응 — 총성에 시민이 흩어지고 신고한다. Mass Entity 는 나중에.

첫 목표는 **골목 하나에서 경찰 둘과 1분간 총격전**이다. 도시도 차도 미션도 그 뒤다.
