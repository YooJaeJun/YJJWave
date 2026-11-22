#pragma once

#include "CoreMinimal.h"
#include "Game/YJJAutoPilot.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/IConsoleManager.h"
#include "Sim/YJJBattle.h"
#include "YJJWaveGameMode.generated.h"

/**
 * 한 판을 들고 고정 틱으로 돌린다. 배속·일시정지·다시 시작, 오토파일럿·벤치마크 콘솔 변수.
 *
 *   yjj.AutoPlay 1          봇이 짓고·강화하고·특성 찍고·스펠 쓴다
 *   yjj.AutoPlayTime 초     게임 시간 이만큼 지나면 요약 로그 남기고 종료 (0 = 끝없이)
 *   yjj.AutoPlaySpeed N     오토파일럿 배속
 *   yjj.ShotInterval 초     실제 시간 간격으로 스크린샷 (Saved/Screenshots/Play_*.png)
 *   yjj.Bench N             시작하자마자 편마다 N마리를 깔고 맞붙인다 (대용량 성능 확인)
 */
UCLASS()
class YJJWAVE_API AYJJWaveGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AYJJWaveGameMode();
	virtual ~AYJJWaveGameMode() override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	FYJJBattle& GetBattle() { return Battle; }
	const FYJJBattle& GetBattle() const { return Battle; }

	/** 시뮬이 흐르는 중인가 (일시정지·특성 창·패배가 아니면) */
	bool IsRunning() const;
	bool IsPaused() const { return bUserPaused; }
	float GetSpeed() const;
	void CycleSpeed();
	void TogglePause() { bUserPaused = !bUserPaused; }
	void Restart();
	void TakeShot(const FString& Name) const;

private:
	void SpawnBenchmark(int32 PerTeam);

	FYJJBattle Battle;
	TUniquePtr<FYJJAutoPilot> AutoPilot;
	double Accumulator = 0.0;
	int32 SpeedIndex = 0;
	bool bUserPaused = false;
	bool bBenchSpawned = false;

	static TAutoConsoleVariable<int32> CVarAutoPlay;
	static TAutoConsoleVariable<float> CVarAutoPlayTime;
	static TAutoConsoleVariable<float> CVarAutoPlaySpeed;
	static TAutoConsoleVariable<float> CVarShotInterval;
	static TAutoConsoleVariable<int32> CVarBench;
};
