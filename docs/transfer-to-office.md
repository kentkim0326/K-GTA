# 노트북 → E 드라이브 → 사무실 PC 이사 절차

노트북을 반납하기 전에 읽는다. 2026-10-19 작성.

---

## 1. 무엇이 어디에 있나

| 구분 | 어디 | 사무실에서 |
|---|---|---|
| 코드, 설정, 문서 | **GitHub 저장소** | `git clone` 하면 끝 |
| 자체 맵 `Content/KGTA/` | **GitHub (Git LFS)** | 같이 따라온다 |
| 구매 에셋 (킷, Tokyo_Street 등) | **노트북에만** | E 로 옮기거나 Fab 에서 다시 받는다 |
| `DerivedDataCache`, `Intermediate`, `Binaries`, `Saved` | 생성물 | 옮기지 않는다. 다시 만들어진다 |

**옮겨야 하는 것은 구매 에셋뿐이다.** 나머지는 저장소에 있다.

Fab 라이브러리에서 다시 받아도 되지만 16 GB 를 다시 내려받는 것보다 복사가 빠르다.

---

## 2. 노트북에서 (반납 전)

### 2-1. 푸시부터 한다

가장 중요하다. 이걸 빠뜨리면 작업이 사라진다.

```powershell
cd C:\Dev\GunsKorea
git status --short
```

뭔가 나오면 커밋하고 푸시한다.

```powershell
git add -A
git commit -m "노트북 반납 전 작업 저장"
git push origin main
```

확인한다. 두 줄의 커밋 해시가 같아야 한다.

```powershell
git log --oneline -1
git log --oneline origin/main -1
```

**골목 맵을 만들었다면** `Content/KGTA/Maps/` 아래 파일이 커밋됐는지 꼭 본다. LFS 로 올라간다.

```powershell
git lfs ls-files
```

### 2-2. E 드라이브로 복사한다

생성물을 빼고 복사한다. 16 GB 쯤이고 10~20분 걸린다.

```powershell
robocopy C:\Dev\GunsKorea E:\GunsKorea /E /XD DerivedDataCache Intermediate Binaries Saved .git /XF *.sln *.slnx
```

`.git` 을 뺀 것은 저장소에 다 있기 때문이다. 불안하면 `/XD` 목록에서 `.git` 을 빼고 통째로 복사해도 된다.

복사된 것을 확인한다. `Content`, `Config`, `Source`, `Plugins`, `docs` 가 있어야 한다.

```powershell
Get-ChildItem E:\GunsKorea | Select-Object Name
```

### 2-3. 추가로 챙길 것

- **Epic Games 계정 정보.** Fab 라이브러리가 거기 붙어 있다. 킷과 Tokyo_Street 를 다시 받으려면 필요하다.
- **GitHub 로그인.** 사무실 PC 에서 다시 인증해야 한다.

---

## 3. 사무실 PC에서

### 3-1. 설치 (순서대로)

1. **Epic Games Launcher** → 언리얼 엔진 **5.8** 설치. 50 GB 쯤.
2. **Visual Studio 2022** → "Game development with C++" 워크로드. 20~40 GB.
3. **Git for Windows** — https://git-scm.com/download/win. Git LFS 가 같이 들어 있다.
4. **Claude Code** — PowerShell 에서 `irm https://claude.ai/install.ps1 | iex`

설치 후 새 PowerShell 창에서 확인한다.

```powershell
git --version
git lfs version
claude --version
```

`claude` 가 안 잡히면 PATH 문제다. 새 창을 열거나 아래를 한 번 실행한다.

```powershell
[Environment]::SetEnvironmentVariable("Path", [Environment]::GetEnvironmentVariable("Path","User") + ";$env:USERPROFILE\.local\bin", "User")
```

### 3-2. 프로젝트 복원

**경로에 한글과 띄어쓰기를 쓰지 않는다.** `C:\Dev\GunsKorea` 로 간다.
SSD 가 다른 드라이브면 거기가 낫다.

E 드라이브에서 복사해 온다.

```powershell
robocopy E:\GunsKorea C:\Dev\GunsKorea /E
```

Git LFS 를 먼저 켠다. 순서를 지킨다.

```powershell
git lfs install
```

저장소를 붙인다.

```powershell
cd C:\Dev\GunsKorea
git init
git remote add origin https://github.com/kentkim0326/K-GTA.git
git fetch origin main
git checkout -b main origin/main
```

`untracked working tree files would be overwritten` 오류가 나면 멈추고 메시지를 확인한다.
E 에서 복사해 온 파일과 저장소 파일이 겹친 것이다. 저장소 쪽이 최신이므로 이렇게 넘긴다.

```powershell
git checkout -f -b main origin/main
```

### 3-3. 빌드

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="C:\Dev\GunsKorea\GunsKorea.uproject" -game -rocket -progress
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" GunsKoreaEditor Win64 Development -project="C:\Dev\GunsKorea\GunsKorea.uproject" -waitmutex
```

둘 다 `Result: Succeeded` 가 나와야 한다.

### 3-4. 열고 확인

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Dev\GunsKorea\GunsKorea.uproject"
```

첫 실행에서 셰이더 컴파일이 돈다. 수십 분에서 몇 시간이다. 한 번뿐이다.
`DerivedDataCache` 를 안 옮겼기 때문이고, 옮기면 생략되지만 용량이 크다.

플레이한 뒤 콘솔에서 확인한다.

```
GunDay.ShowDebug 1
GunDay.Alley.Start
```

경찰이 나오면 전부 정상이다.

### 3-5. Claude Code 띄우기

```powershell
cd C:\Dev\GunsKorea
claude
```

첫 마디.

```
docs/handover-2026-10-09.md 와 docs/transfer-to-office.md 를 읽고 상황 파악해줘.
```

---

## 4. E 드라이브를 못 쓰게 됐다면

구매 에셋은 전부 Fab 라이브러리에 남아 있다. 계정만 있으면 다시 받는다.

| 팩 | 크기 | 받는 법 |
|---|---|---|
| Third Person Shooter Kit v2.2 | 11.8 GB | Fab 라이브러리 → **Create Project** (UE 5.8) |
| Tokyo_Street | 2.3 GB | Fab 라이브러리 → **프로젝트에 추가** |
| VehicleVarietyPack | 1.4 GB | 같은 방법 |
| FighterGirl | 1.2 GB | 같은 방법 |
| BloodPack | 1.1 GB | 같은 방법 |

순서가 중요하다. **킷으로 프로젝트를 먼저 만들고**, 그 폴더에 저장소를 붙인 뒤,
나머지 팩을 그 프로젝트에 추가한다.

다운로드에 시간이 걸릴 뿐 잃는 것은 없다.
