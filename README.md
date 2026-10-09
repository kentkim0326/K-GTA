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
| `GunDay.AutoReport 0` | 피해 자동 신고를 끈다 |
| `GunDay.Crowd.Gunshot` | 주변 시민을 흩어지게 한다 |
| `GunDay.Crowd.Calm` | 놀란 시민을 진정시킨다 |
| `GunDay.Alley.Start` | 1분 총격전 측정을 시작한다 |
| `GunDay.Alley.Stop` | 측정을 중단한다 |
| `GunDay.Alley.Result` | 지난 판 결과를 다시 띄운다 |
| `GunDay.Dispute.Start` | 주변 시민 둘로 시비를 일으킨다 |
| `GunDay.Dispute.Clear` | 진행 중인 시비를 끝낸다 |
| `GunDay.Jeong 80` | 사회의 정을 80으로 맞춘다 |
| `GunDay.Profile` | 주변 사람들의 진영을 로그에 찍는다 |
| `GunDay.News.Next` | 뉴스 한 줄을 지금 내보낸다 |
| `GunDay.News.Day` | 하루를 넘겨 일일 집계를 본다 |

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
| Min / Max Spawn Distance | 플레이어로부터 이 범위 안의 내비메시 위에 생성한다 (기본 9~25m) |
| Drive Responders To Player | 멀리 있는 인원을 플레이어 쪽으로 보낸다 |
| Responder Engage Distance | 이 거리 안에 들어오면 접근을 멈추고 킷 AI 에 맡긴다 |
| Despawn Distance | 이보다 멀어진 인원은 정리한다 |

`Responders Count As Witnesses` 를 켜 두면 투입된 경찰이 목격자로 잡힌다.
경찰이 살아 있는 동안에는 열기가 줄지 않는다. 쓰러뜨리거나 Despawn Distance 밖으로
도망쳐야 수배가 풀린다. 이게 추격에서 벗어나는 방법이 된다.

투입된 경찰은 교전 거리까지 스스로 다가온다. 적을 찾아다닐 필요가 없다.
디버그 HUD 가 켜져 있으면 머리 위에 파란 구가 그려져 어디 있는지 바로 보인다.

생성 위치는 내비메시 위에서 고르고 플레이어 시야 밖을 우선한다. **레벨에 NavMeshBoundsVolume 이 없으면
아무도 생성되지 않는다.** 로그에 경고가 찍힌다.

#### 킷 스포너를 쓰고 싶다면

프로젝트 세팅에서 `Spawn Responders` 를 끄고 `On Response Tier Changed` 에 블루프린트를 붙인다.
수배 레벨과 필요 인원이 넘어오므로 킷의 AI 스포너를 호출한 뒤, 스폰 결과를 `Register Responder` 로
등록하면 인원 계산과 정리에 함께 들어간다.

### 범죄 자동 감지

`UGunDayCrimeWatcherSubsystem` — 킷 블루프린트를 고치지 않고 수배를 올리기 위한 장치다.
월드의 폰들에 주기적으로 피해 이벤트를 걸어 두고, **플레이어가 입힌 피해만** 골라 신고한다.
맞은 쪽이 경찰이면 경찰 부상, 아니면 시민 부상으로 친다. 때린 상대가 사라지면 사망으로 올린다.

연사로 수배가 치솟지 않도록 상대별 신고 간격이 걸려 있다. 값은
프로젝트 세팅 > Game > GunDay Core > **범죄 감지** 에서 조정한다.

경찰 판정은 두 가지로 한다. 투입 명단에 있는 상대는 자동으로 경찰이고,
그 외에 경찰로 칠 클래스가 있으면 `Police Classes` 목록에 넣는다.

#### 허공에 쏘는 것까지 잡으려면

엔진에 "발사" 라는 공통 이벤트가 없어서 피해가 없는 사격은 감지할 수 없다.
킷의 발사 이벤트에서 `Report Player Gunfire` 노드를 한 번 불러 주면 된다.
킷 블루프린트를 건드리기 싫다면, 플레이어 블루프린트 쪽에서 입력 이벤트에 붙여도 같은 효과다.

### 군중 반응

`UGunDayCrowdSubsystem` — 총성이 나면 반경 안의 시민이 소리 반대쪽으로 달아나고,
몇 초 뒤 신고한다. 신고가 들어가면 열기가 오르고 그 시민은 한동안 목격자로 잡힌다.
목격자가 있는 동안에는 수배가 줄지 않으므로, 사람이 많은 곳에서 쏘면 그만큼 오래 쫓긴다.

값은 프로젝트 세팅 > Game > GunDay Core > **군중 반응** 에서 조정한다.
반응 반경, 신고까지의 시간, 신고 한 건의 열기, 목격 지속 시간, 달아나는 거리를 만진다.

`Civilian Classes` 에 킷의 `BP_AICharacterCivilian` 을 넣어 두는 편이 좋다.
비워 두면 플레이어와 경찰을 뺀 모든 폰을 시민으로 보므로 적까지 달아난다.

킷의 행동 트리가 이동 명령과 다투면 `Drive Civilian Flee` 를 끄고
`On Civilian Alerted` 에 블루프린트를 붙여 킷 쪽 반응을 부르면 된다.

한 번에 반응하는 시민 수는 `Max Alerted Civilians` 로 묶여 있다. 기본 24명이다.
도시 규모로 수천 명이 필요해지면 그때 City Sample 의 Mass Entity 로 갈아탄다.

### 뉴스 티커

`UGunDayNewsSubsystem` — 이 게임에서 가장 무서운 것은 총이 아니라
**아무도 놀라지 않는다는 사실**이다. 그 무심함을 담는 자리다.

세 종류를 섞어 내보낸다.

- **속보** — 방금 벌어진 사건. 매번 나오지는 않는다. 매번 나오면 무심함이 사라진다
- **통계** — 하루가 끝날 때의 집계. "오늘 총기 사망 {오늘}명, 어제보다 {차이}명 {증감}"
- **잡담** — 사건과 무관한 멘트. 광고, 논평, 날씨, 금리. 여기가 풍자의 자리다

정이 바닥으로 떨어지면 전용 문구가 섞인다. 사회가 어떤 상태인지는 뉴스가 먼저 안다.

문구는 프로젝트 세팅 > Game > GunDay Core > **뉴스** 에서 고친다.
자리표를 쓰면 내보낼 때 숫자로 바뀐다.

| 자리표 | 바뀌는 값 |
|---|---|
| `{오늘}` `{어제}` | 오늘과 어제의 총기 사망자 수 |
| `{차이}` `{증감}` | 어제와의 차이, 증가 또는 감소 |
| `{총}` | 누적 사망자 수 |
| `{시비}` | 오늘의 시비 건수 |
| `{정}` | 지금의 정 수치 |
| `{날}` | 며칠째인가 |

`On Headline` 에 화면 아래 티커 UI 를, `On Day Rollover` 에 일일 집계 화면을 붙인다.
UI 가 붙기 전까지는 디버그 HUD 에 한 줄로 뜬다.

### 정(情) 과 진영

`UGunDaySocietySubsystem` — 작품의 주제를 숫자로 옮긴 것이다.

**정은 0에서 100까지의 값 하나다.** 사회 전체에 하나뿐이다.
높으면 시비가 중간에 가라앉고 누가 말리러 나선다. 낮으면 전부 끝까지 간다.
사람이 죽을 때마다 내려가고, 누가 말려서 싸움이 멎으면 올라간다.
아무 일도 없으면 기준값으로 천천히 돌아간다. 사람은 잊는다.

**진영은 사람마다 붙는 보이지 않는 꼬리표다.** 정치, 부동산, 세대, 지역, 성별 다섯 축.
축마다 이쪽, 저쪽, 어느 쪽도 아님 중 하나다. 폰 이름에서 결정적으로 만들어 내므로
같은 사람은 다시 만나도 같은 진영을 갖는다.

두 사람의 진영이 다른 축이 많을수록 **마찰**이 커진다.
마찰이 큰 둘이 시비에 끌려 들어가고, 붙으면 끝까지 갈 확률이 올라간다.
층간소음으로 시작한 말다툼이 "그러니까 당신 같은 사람들이" 로 번지는 그 비약이
여기서 나온다.

**말리는 사람.** 몸싸움 단계에서 정이 높으면 누군가 나선다.
나서면 가라앉을 기회가 한 번 더 생긴다. 다만 총이 오가면 말리던 사람이 대신 맞기도 한다.
선의가 처벌받는 그 장면이 이 작품의 톤이다.

값은 프로젝트 세팅 > Game > GunDay Core > **정과 진영** 에서 조정한다.

### 시비 시스템

`UGunDayDisputeSubsystem` — 《총기허용의 날》의 핵심이다.
플레이어가 특별해서 총을 쏘는 세계가 아니라 누구나 총을 가진 세계다.
주차, 담배, 노인석, 라면. 사소한 일로 시민끼리 붙고 말다툼에서 총까지 간다.

**모양은 둘이다.** 우발과 대기.

우발은 주차나 라면처럼 그 자리에서 붙는 것이다. 진영이 다르면 더 크게 붙고, 누가 말리면 멎는다.

대기는 다르다. 병원 대기실에 총을 들고 와 앉아 있는 사람은 몇 달 전에 이미 결심했다.
진영은 상관없고 말린다고 멎지 않는다. **멎는 길은 상대가 잘못을 인정하는 것뿐이다.**
발뺌하면 그 길이 닫힌다. 시나리오의 `Apology Chance` 가 그 문이다.

**단계는 넷이다.** 말다툼 → 몸싸움 → 총 꺼냄 → 발포.
각 단계마다 확률 판정을 하고, 못 넘으면 그 자리에서 가라앉는다.
총을 꺼냈다가 집어넣는 경우도 있다. 그쪽이 더 무서울 때가 있다.

플레이어가 없어도 일어난다. 플레이어는 지나가다 마주친다.
지나칠지, 말릴지, 끼어들어 쏠지는 플레이어가 정한다.
끼어들어 쏘면 그건 플레이어의 범죄이므로 수배가 오른다.

#### 골목마다 다른 시비

`AGunDayDisputeSpot` 액터를 레벨에 놓는다. 편의점 앞, 주차장, 버스정류장, 담배 피우는 구석.
자리마다 `Scenario Indices` 로 어울리는 상황만 지정하면 그 장소에서는 그 시비만 난다.
반경 안의 시민 둘이 끌려 들어간다. 쿨다운이 있어 같은 자리에서 연달아 나지 않는다.

자리를 하나도 안 놓았으면 플레이어 주변 시민 중 아무나로 시작한다.
골목마다 자리를 다 놓고 나면 `Start Disputes Without Spots` 를 꺼 둔다.

#### 대사

상황은 프로젝트 세팅 > Game > GunDay Core > **시비** 의 Dispute Scenarios 에 있다.
단계별 대사 목록과 단계 시간, 상승 확률, 발포 확률을 거기서 만진다.

**기본 대사는 자리표다.** 구조를 보여 주려고 넣은 것이니 작품의 말로 바꿔 쓴다.
현실성은 총이 아니라 그 앞의 말에서 나온다.

대기형에서는 **발뺌의 언어**가 핵심이다. "경과는 개인차가 있습니다",
"그건 제 담당이 아니라서요", "법적으로 가시면 됩니다".
웃기고 서늘한 자리는 총이 아니라 그 문장들이다.

`On Dispute Line` 에 자막 UI 를, `On Dispute Stage Changed` 에 애니메이션과 연출을 붙인다.
자막이 붙기 전까지는 디버그 HUD 가 켜져 있으면 머리 위에 대사가 뜬다.

### 첫 마일스톤 측정

`UGunDayEncounterSubsystem` — 「골목 하나에서 경찰 둘과 1분간 총격전」을 같은 조건으로
반복해서 재기 위한 도구다. 매번 손으로 세팅하지 않는다.

```
GunDay.Alley.Start        60초, 수배 레벨 1
GunDay.Alley.Start 90 2   90초, 수배 레벨 2
```

수배와 투입 인원과 놀란 시민을 전부 지우고 시작한다. 화면에 남은 시간과 처치 수가 뜨고,
끝나면 생존 여부와 버틴 시간과 처치 수가 나온다. 플레이어가 쓰러지면 그 자리에서 끝난다.

결과는 30초간 남고 `LogGunDay` 에도 찍힌다. 놓쳤으면 `GunDay.Alley.Result` 로 다시 띄운다.

재미있는지 보는 것이 목적이다. 숫자가 아니라 손맛을 본다.
지루하면 투입 간격과 인원을, 너무 어려우면 수배 레벨과 거리를 프로젝트 세팅에서 조정한다.

### 다음

첫 목표는 **골목 하나에서 경찰 둘과 1분간 총격전**이다. 도시도 차도 미션도 그 뒤다.
