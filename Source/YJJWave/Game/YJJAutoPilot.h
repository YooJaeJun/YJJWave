#pragma once

#include "CoreMinimal.h"
#include "Sim/YJJHorde.h"

class AYJJWaveGameMode;
class AYJJWaveHUD;
class FYJJBattle;

/**
 * 자동 플레이 봇 (검증·영상 소재용).
 * 병영을 Buildings.csv BotWeight 비율대로 짓고, 칸이 다 차면 가장 싼 강화를 사고,
 * 영혼이 모이면 이어진 별 중 큰 별을 먼저 찍고, 스펠은 준비되는 대로 전선에 쓴다.
 * 카메라는 전선을 따라가며 스크린샷을 남기고, 정해진 게임 시간이 지나면 요약을 로그에 남기고 끈다.
 */
class FYJJAutoPilot
{
public:
	FYJJAutoPilot(AYJJWaveGameMode* InMode, float InDuration, float InShotInterval, bool bInBench);

	void Tick(float RealDeltaSeconds);

private:
	void Think(FYJJBattle& Battle);
	void SpendGold(FYJJBattle& Battle);
	void SpendSouls(FYJJBattle& Battle);
	void CastSpells(FYJJBattle& Battle);
	void FollowFront(const FYJJBattle& Battle, AYJJWaveHUD* HUD, float RealDeltaSeconds);
	void Finish(const FYJJBattle& Battle);
	int32 PickBuilding(const FYJJBattle& Battle) const;

	AYJJWaveGameMode* Mode = nullptr;
	float Duration = 0.f;
	float ShotInterval = 0.f;
	float ShotTimer = 0.f;
	float ThinkTimer = 0.f;
	int32 ShotCount = 0;
	int32 LastLevel = 0;
	bool bFinished = false;
	/** 벤치마크: 전장 전체가 보이게 멀리서 (그리기 최악 조건) */
	bool bBench = false;
	float ExitTimer = 0.f;
	/** 여러 스테이지에 걸친 게임 시간 합 */
	float TotalTime = 0.f;
	bool bTreeShot = false;

	// 성능 기록
	double StepMsSum = 0.0;
	double FrameMsSum = 0.0;
	int32 Samples = 0;
	float WorstStepMs = 0.f;
	int32 PeakUnits = 0;
	// 밀집 구간: 유닛이 최대치의 DenseShare 이상 살아 있을 때만
	double DenseStepMsSum = 0.0;
	double DenseFrameMsSum = 0.0;
	int32 DenseSamples = 0;
	float DenseWorstFrameMs = 0.f;
	double DensePhaseMsSum[static_cast<int32>(EYJJHordePhase::Count)] = {};
	double DenseUnitDrawMsSum = 0.0;
	int64 DenseVisibleSum = 0;
	double DenseWaitMsSum = 0.0;
	double DenseReorderMsSum = 0.0;
	FRandomStream Random;

	static constexpr int32 RandomSeed = 1234;
	static constexpr float MsPerSecond = 1000.f;
	static constexpr float DenseShare = 0.8f;
	/** 끝난 뒤 종료 대기의 이 비율이 지나면 특성 성좌 화면을 한 장 찍는다 */
	static constexpr float TreeShotAt = 0.5f;
	static constexpr const TCHAR* ShotPrefix = TEXT("Play_");
};
