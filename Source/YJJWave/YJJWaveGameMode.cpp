#include "YJJWaveGameMode.h"
#include "Data/YJJGameData.h"
#include "Game/YJJAutoPilot.h"
#include "Misc/Paths.h"
#include "Player/YJJWavePlayerController.h"
#include "UI/YJJWaveHUD.h"
#include "UnrealClient.h"
#include "YJJWave.h"

TAutoConsoleVariable<int32> AYJJWaveGameMode::CVarAutoPlay(TEXT("yjj.AutoPlay"), 0, TEXT("1이면 봇이 플레이한다"));
TAutoConsoleVariable<float> AYJJWaveGameMode::CVarAutoPlayTime(TEXT("yjj.AutoPlayTime"), 0.f, TEXT("오토파일럿: 게임 시간(초)이 지나면 요약을 남기고 종료. 0이면 계속"));
TAutoConsoleVariable<float> AYJJWaveGameMode::CVarAutoPlaySpeed(TEXT("yjj.AutoPlaySpeed"), 1.f, TEXT("오토파일럿 배속"));
TAutoConsoleVariable<float> AYJJWaveGameMode::CVarShotInterval(TEXT("yjj.ShotInterval"), 0.f, TEXT("오토파일럿: 실제 시간 몇 초마다 스크린샷 (0이면 끝에만)"));
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
	Battle.Reset();
	UE_LOG(LogYJJWave, Display, TEXT("[Run] 시작: 유닛 %d종, 병영 %d종, 특성 %d개, 물결 %d레벨"),
		YJJData::Units().Num(), YJJData::Buildings().Num(), YJJData::Perks().Num(), YJJData::Waves().Num());
}

bool AYJJWaveGameMode::IsRunning() const
{
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	const AYJJWaveHUD* HUD = Player ? Cast<AYJJWaveHUD>(Player->GetHUD()) : nullptr;
	return !bUserPaused && !Battle.bGameOver && !(HUD && HUD->IsTreeOpen());
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

void AYJJWaveGameMode::Restart()
{
	Battle.Reset();
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
	FRandomStream Random(PerTeam);
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
		const float Y = Random.FRandRange(T.FieldMargin, Battle.FieldHeight - T.FieldMargin);
		const float Depth = Random.FRand() * Battle.FieldWidth * T.BenchDepth;
		Battle.Horde.Spawn(PlayerUnits[Index % PlayerUnits.Num()], EYJJTeam::Player, FVector2f(Battle.WallX + Depth, Y), Stats);
		Battle.Horde.Spawn(EnemyUnits[Index % EnemyUnits.Num()], EYJJTeam::Enemy, FVector2f(Battle.FieldWidth - T.EnemySpawnInset - Depth, Y), Stats);
	}
	UE_LOG(LogYJJWave, Display, TEXT("[Bench] 편마다 %d마리 배치 (아군 %d, 적 %d)"), PerTeam, Battle.Horde.Num(EYJJTeam::Player), Battle.Horde.Num(EYJJTeam::Enemy));
}

void AYJJWaveGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

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

	if (IsRunning())
	{
		const FYJJTuning& T = YJJData::Tune();
		Accumulator += DeltaSeconds * GetSpeed();
		int32 Steps = 0;
		while (Accumulator >= T.SimStep && Steps < T.MaxStepsPerFrame)
		{
			Battle.Tick(T.SimStep);
			Accumulator -= T.SimStep;
			++Steps;
		}
		// 너무 밀리면 버린다 (느린 기계에서 눈덩이처럼 쌓이지 않게)
		Accumulator = FMath::Min(Accumulator, static_cast<double>(T.SimStep));
	}

	if (AutoPilot)
	{
		AutoPilot->Tick(DeltaSeconds);
	}
}
