#pragma once

#include "CoreMinimal.h"
#include "Game/YJJAutoPilot.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/IConsoleManager.h"
#include "Sim/YJJBattle.h"
#include "Tasks/Task.h"
#include "YJJWaveGameMode.generated.h"

/** 지금 화면: 타이틀 → 월드맵 → 전투 */
enum class EYJJScreen : uint8
{
	Title,
	WorldMap,
	Battle,
};

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 판 상태를 바꾸기 전에 부른다 (작업 스레드의 시뮬이 끝날 때까지 기다림) */
	FYJJBattle& GetBattle() { WaitForSim(); return Battle; }
	/**
	 * HUD 전용, 기다리지 않는다. 작업 스레드가 쓰는 것은 Horde 뿐이므로 Horde 밖의 판 필드(금화·병영·연출·죽음 목록)만 읽고 쓴다.
	 * 유닛은 GetView() 사본을 읽는다.
	 */
	FYJJBattle& BattleNoWait() { return Battle; }
	/** 이번 프레임 그리기용 유닛 사본 */
	const FYJJHordeView& GetView() const { return View; }
	void WaitForSim();
	/** 지난 프레임 시뮬을 기다린 시간 (겹치기가 잘 되면 0에 가깝다) */
	float LastSimWaitMs() const { return SimWaitMs; }

	/** 시뮬이 흐르는 중인가 (일시정지·특성 창·패배가 아니면) */
	bool IsRunning() const;
	bool IsPaused() const { return bUserPaused; }
	float GetSpeed() const;
	/** 플레이어 배속 토글만 (x1, x2). 버튼 글자용: 테스트 배속은 빼고 보여 준다 */
	float SpeedToggle() const;
	void CycleSpeed();
	void TogglePause() { bUserPaused = !bUserPaused; }
	void Restart();
	/** 스테이지 시작 (지금 영혼·특성을 저장하고 새 판) */
	void StartStage(int32 Stage);
	bool HasNextStage() const;
	void NextStage();
	/** 영혼·특성·깬 스테이지 저장 (특성을 찍은 뒤에도 부른다) */
	void SaveProgress();
	/** 출전할 영웅 (월드맵 영웅 고르기) */
	void SetHero(const FName& Hero) { Progress.Hero = Hero; }

	// ---- 화면 흐름 ----
	EYJJScreen GetScreen() const { return Screen; }
	void GoToTitle();
	void GoToWorldMap();
	/** 옵션의 포기: 지금 판을 접고 월드맵으로 (얻은 영혼은 남는다) */
	void GiveUp();
	const FYJJProgress& GetProgress() const { return Progress; }
	/** 깬 스테이지 다음까지 열린다 */
	bool IsStageOpen(int32 Stage) const { return Stage <= Progress.Cleared; }
	int32 StageStars(int32 Stage) const { return Progress.Stars.IsValidIndex(Stage) ? Progress.Stars[Stage] : 0; }

	// ---- 컷씬 ----
	/** 지금 보여 주는 줄 (없으면 nullptr) */
	const FYJJCutLine* CutsceneLine() const;
	float CutsceneTime() const { return CutTime; }
	void NextCutsceneLine();
	void SkipCutscene();
	void TakeShot(const FString& Name) const;

private:
	void SpawnBenchmark(int32 PerTeam);

	FYJJBattle Battle;
	FYJJHordeView View;
	FYJJProgress Progress;
	EYJJScreen Screen = EYJJScreen::Title;
	bool bResultSaved = false;
	/** 스피드핵 감지 (Meta/YJJGuard.h) */
	FYJJSpeedWatch SpeedWatch;
	int32 CutIndex = INDEX_NONE;
	float CutTime = 0.f;
	void StartCutscene();
	UE::Tasks::FTask SimTask;
	bool bSimRunning = false;
	float PendingPostDt = 0.f;
	float SimWaitMs = 0.f;
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
	static TAutoConsoleVariable<int32> CVarAsyncSim;
	static constexpr double MsPerSecond = 1000.0;
	static constexpr int32 BenchSeedSalt = 7919;
	inline static const FName MusicBattle = TEXT("Music_Battle");
	inline static const FName MusicBoss = TEXT("Music_Boss");
	inline static const FName MusicMenu = TEXT("Music_Battle");
};
