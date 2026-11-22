# YJJWave 도구

엔진: `D:\source\repos\UnrealEngine` (UE 5.8.1 소스 빌드, 다른 YJJ 프로젝트와 같은 엔진). 에디터를 끈 상태에서 실행한다.
맵 에셋은 없다: 빈 엔진 맵(`/Engine/Maps/Entry`) 위에 게임모드·HUD가 전부 그린다. 그림·글꼴·표는 실행 중에 파일에서 읽는다 (임포트 없음).

| 스크립트 | 하는 일 |
|---|---|
| `Build.ps1` | 에디터 타깃 빌드 (`Saved/Logs/Build.log`) |
| `RunGame.ps1` | 게임을 띄워 오토파일럿이 플레이 → 로그 요약 + `Saved/Screenshots/Play_*.png` (끝에 특성 성좌 화면 `Play_Tree.png`). `-Time 초 -Speed 배속 -ShotInterval 초` |
| `RunGame.ps1 -Bench N` | 편마다 N마리를 깔고 맞붙여 성능 측정. 로그의 `[AutoPilot] DENSE` 줄 = 유닛이 최대치의 80% 이상 살아 있는 구간의 시뮬·프레임 시간 |
| `Art/generate_art.py` | Pillow로 그림 생성 → `Content/YJJWave/Art/*.png`. 처음 한 번: `python -m venv Tools/Art/.venv` 후 `pip install pillow` |
| `Data/gen_tables.py` | `Tuning.csv`·`UILayout.csv`와 그 구조체(`Source/YJJWave/Data/YJJTuning.h`)를 만든다. 이미 있는 값은 덮어쓰지 않는다 |
| `Data/gen_perks.py` | 특성 성좌 `Perks.csv` 생성 (8갈래, 큰 별 16개, 중간 별 효과 그림 `Icon`) |
| `Audio/generate_voice.py` | 대사 목소리 (AI 성우): `Cutscenes.csv`·`Dialogues.csv`의 `Voice=Line_*` 줄을 Windows 한국어 TTS로 읽히고 ffmpeg로 화자별 가공(음 낮추기·저음·울림) → `Generated/VO_Line_*.wav`, `Sounds.csv` 줄 추가, 대사 시간을 목소리 길이에 맞춘다. 그다음 `Audio/BuildAudio.ps1` |

## 고칠 곳

숫자와 글자는 코드에 두지 않는다. 표는 `Content/YJJWave/Data/*.csv` (UTF-8 BOM, 엑셀로 열어도 된다). 게임 중에는 콘솔 `yjj.ReloadData`.

| 표 | 내용 |
|---|---|
| `Tuning.csv` | 규칙 수치: 전장 크기, 유닛 최대 수, 이웃 찾기 격자, 밀어내기, 스펠, 특성 비용, 봇… |
| `UILayout.csv` | 화면 배치·연출 수치, 아틀라스 칸 번호 (1920x1080 가상 좌표) |
| `Units.csv` | 병사·몬스터: 체력·공격·사거리·시야·속도·반지름·질량·범위·밀쳐내기·투사체·보상·그림 줄·크기 |
| `Buildings.csv` | 병영: 유닛, 비용과 증가율, 생산 주기·수, 레벨당 증가, 아이콘, 봇 비중 |
| `Waves.csv` | 물결 레벨별 몬스터 (초당 마리 `Rate`, 레벨 시작에 한꺼번에 `Burst` = 보스) |
| `Perks.csv` | 특성 성좌 별 (좌표·효과 2개·이웃). `gen_perks.py`로 만든다 |
| `Spells.csv` | 주문 F1·F2·F3의 Lv별 위력·반경·대기·지속 + Lv 특수 효과(Fx, A, B). Lv은 성좌의 주문 별로 올린다 |
| `Cutscenes.csv` | 스테이지 시작 컷씬: 화자(유닛 이름, `Title`이면 장 제목 카드)·글자 키·시간·카메라·목소리(`Voice`) |
| `Dialogues.csv` | 게임 중 말풍선 대화. 같은 `Talk`끼리 `Order` 순서로, 조건(`Stage·Trigger·Wave·MinWave·Param·Chance`)은 첫 줄 |
| `Broth.csv` | 냄비 국물 단계: 필요한 뼈, 열면 체력·공격력 +, 회복, 색 |
| `Regions.csv` | 월드맵 지역 7곳: 자리(화면 비율)·색·열림. `generate_art.py`가 이 표로 월드맵 그림을 그린다 |
| `Stages.csv` | 스테이지: 지역·월드맵 자리·바닥 그림(`Ground`)·체력 배율 |
| `Localization.csv` | 화면 글자 전부. 열 `Key, ko, en, Note`, `{0}` 서식. 언어: `-yjjlang=en` > OS 언어 > ko |

- 코드 규칙: `.cpp`에는 namespace·변수·함수·구조체·템플릿을 선언하지 않는다 (헤더에만). 상수는 표나 이름 붙은 변수로
- 유닛 시뮬: `Sim/YJJHorde` (SoA 배열 + 편별 균일 격자 + ParallelFor), 판 규칙: `Sim/YJJBattle`, 화면: `UI/YJJWaveHUD*`
- 색: `UI/YJJUIStyle.h`

## 조작

- 흐름: 타이틀(국거리 디펜스) → 월드맵 → 스테이지. 옵션(ESC)의 포기 = 월드맵으로
- 왼쪽 클릭: 빈 칸 → 고른 병영 짓기, 지은 병영 → 레벨업. 왼쪽·오른쪽 끌기: 화면 이동
- 유성·대지 가르기는 키·버튼을 누른 뒤 전장을 클릭한 자리에 (오른쪽 클릭·ESC 취소)
- 1~4 병영 고르기, F1 유성(커서 위치), F2 돌격 함성, F3 대지 가르기(커서 x), Q·냄비 클릭 뚜껑 열기, T 특성 성좌, F 배속, P/스페이스 일시정지, R 다시 시작(패배 후), Home 전선으로, F4 성능 표시
- 휠 확대·축소, 오른쪽 끌기·WASD·화면 가장자리 시점 이동

## 콘솔 변수

`yjj.AutoPlay 1`, `yjj.AutoPlayTime 초`, `yjj.AutoPlaySpeed N`, `yjj.ShotInterval 초`, `yjj.Bench N`, `yjj.ReloadData`, `yjj.Lang ko|en`
