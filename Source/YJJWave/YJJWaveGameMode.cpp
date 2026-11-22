#include "YJJWaveGameMode.h"
#include "Data/YJJGameData.h"
#include "Game/YJJAutoPilot.h"
#include "Meta/YJJWaveSave.h"
#include "Audio/YJJAudio.h"
#include "Meta/YJJOptions.h"
#include "Misc/Paths.h"
#include "Player/YJJWavePlayerController.h"
#include "UI/YJJWaveHUD.h"
#include "UnrealClient.h"
#include "YJJWave.h"

TAutoConsoleVariable<int32> AYJJWaveGameMode::CVarAutoPlay(TEXT("yjj.AutoPlay"), 0, TEXT("1이면 봇이 플레이한다"));
TAutoConsoleVariable<float> AYJJWaveGameMode::CVarAutoPlayTime(TEXT("yjj.AutoPlayTime"), 0.f, TEXT("오토파일럿: 게임 시간(초)이 지나면 요약을 남기고 종료. 0이면 계속"));
TAutoConsoleVariable<float> AYJJWaveGameMode::CVarAutoPlaySpeed(TEXT("yjj.AutoPlaySpeed"), 1.f, TEXT("오토파일럿 배속"));
TAutoConsoleVariable<float> AYJJWaveGameMode::CVarShotInterval(TEXT("yjj.ShotInterval"), 0.f, TEXT("오토파일럿: 실제 시간 몇 초마다 스크린샷 (0이면 끝에만)"));
TAutoConsoleVariable<int32> AYJJWaveGameMode::CVarAsyncSim(TEXT("yjj.AsyncSim"), 1, TEXT("1이면 유닛 시뮬을 작업 스레드에서 그리기와 겹쳐 돌린다"));
TAutoConsoleVariable<int32> AYJJWaveGameMode::CVarBench(TEXT("yjj.Bench"), 0, TEXT("시작하자마자 편마다 N마리를 깔아 맞붙인다"));

AYJJWaveGameMode::AYJJWaveGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	HUDClass = AYJJWaveHUD::StaticClass();
	PlayerControllerClass = AYJJWavePlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

AYJJWaveGameMode::~AYJJWaveGameMode() = default;

void AYJJWaveGameMode::BeginPlay()
{
	Super::BeginPlay();
	const FYJJGameDatabase& Database = FYJJGameDatabase::Get();
	for (const FString& Problem : Database.Problems)
	{
		UE_LOG(LogYJJWave, Error, TEXT("[Data] %s"), *Problem);
	}
	FYJJOptions::Get().Apply();
	Progress = UYJJWaveSave::Load();
	// 아직 안 깬 가장 앞 스테이지부터
	const int32 First = FMath::Clamp(Progress.Cleared, 0, FMath::Max(YJJData::Stages().Num() - 1, 0));
	Battle.Reset(First, Progress);
	UE_LOG(LogYJJWave, Display, TEXT("[Run] 시작: 유닛 %d종, 병영 %d종, 특성 %d개, 스테이지 %d개 (%d번부터)"),
		YJJData::Units().Num(), YJJData::Buildings().Num(), YJJData::Perks().Num(), YJJData::Stages().Num(), First + 1);
}

bool AYJJWaveGameMode::IsRunning() const
{
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	const AYJJWaveHUD* HUD = Player ? Cast<AYJJWaveHUD>(Player->GetHUD()) : nullptr;
	return !bUserPaused && !Battle.bGameOver && !Battle.bCardPending && !(HUD && HUD->IsTreeOpen());
}

float AYJJWaveGameMode::GetSpeed() const
{
	if (AutoPilot)
	{
		return FMath::Max(CVarAutoPlaySpeed.GetValueOnGameThread(), 0.f);
	}
	return FMath::Pow(YJJData::Tune().SpeedFactor, static_cast<float>(SpeedIndex));
}

void AYJJWaveGameMode::CycleSpeed()
{
	SpeedIndex = (SpeedIndex + 1) % FMath::Max(YJJData::Tune().SpeedSteps, 1);
	bUserPaused = false;
}

void AYJJWaveGameMode::WaitForSim()
{
	if (!bSimRunning)
	{
		return;
	}
	const double Start = FPlatformTime::Seconds();
	SimTask.Wait();
	SimWaitMs = static_cast<float>((FPlatformTime::Seconds() - Start) * MsPerSecond);
	bSimRunning = false;
}

void AYJJWaveGameMode::Restart()
{
	StartStage(Battle.Stage);
}

bool AYJJWaveGameMode::HasNextStage() const
{
	return Battle.bVictory && YJJData::Stages().IsValidIndex(Battle.Stage + 1);
}

void AYJJWaveGameMode::NextStage()
{
	if (HasNextStage())
	{
		StartStage(Battle.Stage + 1);
	}
}

void AYJJWaveGameMode::SaveProgress()
{
	WaitForSim();
	Battle.WriteProgress(Progress);
	UYJJWaveSave::Store(Progress);
}

void AYJJWaveGameMode::StartStage(int32 Stage)
{
	WaitForSim();
	SaveProgress();
	bResultSaved = false;
	PendingPostDt = 0.f;
	Battle.Reset(Stage, Progress);
	Accumulator = 0.0;
	bBenchSpawned = false;
	if (AYJJWaveHUD* HUD = GetWorld()->GetFirstPlayerController() ? Cast<AYJJWaveHUD>(GetWorld()->GetFirstPlayerController()->GetHUD()) : nullptr)
	{
		HUD->ResetBattleView();
	}
}

void AYJJWaveGameMode::TakeShot(const FString& Name) const
{
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Screenshots") / (Name + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(Path, true, false);
	UE_LOG(LogYJJWave, Display, TEXT("[Run] 스크린샷 %s"), *Path);
}

void AYJJWaveGameMode::SpawnBenchmark(int32 PerTeam)
{
	// 양 끝에서 빽빽한 덩어리로 시작해 가운데서 부딪히게
	const FYJJTuning& T = YJJData::Tune();
	// 한 스트림에서 Y·깊이를 번갈아 뽑으면 LCG 격자 무늬(사선 줄)가 생겨서 둘로 나눈다
	FRandomStream RandomY(PerTeam);
	FRandomStream RandomDepth(PerTeam * BenchSeedSalt + 1);
	TArray<int32> PlayerUnits;
	TArray<int32> EnemyUnits;
	for (int32 Unit = 0; Unit < YJJData::Units().Num(); ++Unit)
	{
		if (!YJJData::Unit(Unit).bBoss)
		{
			(YJJData::Unit(Unit).Team == EYJJTeam::Player ? PlayerUnits : EnemyUnits).Add(Unit);
		}
	}
	if (PlayerUnits.Num() == 0 || EnemyUnits.Num() == 0)
	{
		return;
	}
	const FYJJSpawnStats Stats;
	for (int32 Index = 0; Index < PerTeam; ++Index)
	{
		const float Y = RandomY.FRandRange(T.FieldMargin, Battle.FieldHeight - T.FieldMargin);
		const float Depth = RandomDepth.FRand() * Battle.FieldWidth * T.BenchDepth;
		Battle.Horde.Spawn(PlayerUnits[Index % PlayerUnits.Num()], EYJJTeam::Player, FVector2f(Battle.CampEdgeX + Depth, Y), Stats);
		Battle.Horde.Spawn(EnemyUnits[Index % EnemyUnits.Num()], EYJJTeam::Enemy, FVector2f(Battle.FieldWidth - T.EnemySpawnInset - Depth, Y), Stats);
	}
	UE_LOG(LogYJJWave, Display, TEXT("[Bench] 편마다 %d마리 배치 (아군 %d, 적 %d)"), PerTeam, Battle.Horde.Num(EYJJTeam::Player), Battle.Horde.Num(EYJJTeam::Enemy));
}

void AYJJWaveGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// 지난 프레임에 띄운 시뮬이 끝나야 판을 만질 수 있다
	WaitForSim();

	// 콘솔 변수는 -ExecCmds 로 첫 프레임 뒤에 들어오므로 여기서 확인한다
	if (!AutoPilot && CVarAutoPlay.GetValueOnGameThread() > 0)
	{
		AutoPilot = MakeUnique<FYJJAutoPilot>(this, CVarAutoPlayTime.GetValueOnGameThread(), CVarShotInterval.GetValueOnGameThread(), CVarBench.GetValueOnGameThread() > 0);
		UE_LOG(LogYJJWave, Display, TEXT("[AutoPilot] 시작"));
	}
	if (!bBenchSpawned && CVarBench.GetValueOnGameThread() > 0)
	{
		bBenchSpawned = true;
		SpawnBenchmark(CVarBench.GetValueOnGameThread());
	}

	// 1) 지난 프레임에 띄운 시뮬을 마무리 (연출·보상·패배 판정)
	WaitForSim();
	if (PendingPostDt > 0.f)
	{
		Battle.PostStep(PendingPostDt);
		PendingPostDt = 0.f;
	}
	// 판이 끝나면 한 번 저장 (깬 스테이지·영혼)
	if (Battle.bGameOver && !bResultSaved)
	{
		bResultSaved = true;
		if (Battle.bVictory)
		{
			Progress.Cleared = FMath::Max(Progress.Cleared, Battle.Stage + 1);
		}
		SaveProgress();
	}

	if (AutoPilot)
	{
		AutoPilot->Tick(DeltaSeconds);
	}

	// 2) 이번 프레임 그리기용 사본
	Battle.Horde.CopyTo(View);

	// 소리: 쌓인 사건을 틀고, 보스가 있으면 보스곡
	for (const FName& Key : Battle.Sounds)
	{
		FYJJAudio::Play(this, Key);
	}
	Battle.Sounds.Reset();
	FYJJAudio::PlayMusic(this, Battle.bGameOver ? NAME_None : (View.Boss != INDEX_NONE ? MusicBoss : MusicBattle));

	// 3) 규칙·생산은 여기서, 유닛 시뮬은 작업 스레드로 (그리기와 겹친다)
	if (IsRunning())
	{
		const FYJJTuning& T = YJJData::Tune();
		Accumulator += DeltaSeconds * GetSpeed();
		int32 Steps = 0;
		while (Accumulator >= T.SimStep && Steps < T.MaxStepsPerFrame)
		{
			Accumulator -= T.SimStep;
			++Steps;
		}
		// 너무 밀리면 버린다 (느린 기계에서 눈덩이처럼 쌓이지 않게)
		Accumulator = FMath::Min(Accumulator, static_cast<double>(T.SimStep));
		if (Steps > 0)
		{
			Battle.PreStep(T.SimStep * Steps);
			PendingPostDt = T.SimStep * Steps;
			const float Step = T.SimStep;
			if (CVarAsyncSim.GetValueOnGameThread() > 0)
			{
				bSimRunning = true;
				SimTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, Steps, Step]()
				{
					for (int32 Index = 0; Index < Steps; ++Index)
					{
						Battle.Horde.Step(Step);
					}
				});
			}
			else
			{
				for (int32 Index = 0; Index < Steps; ++Index)
				{
					Battle.Horde.Step(Step);
				}
			}
		}
	}
}

void AYJJWaveGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	WaitForSim();
	SaveProgress();
	Super::EndPlay(EndPlayReason);
}
