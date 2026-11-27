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
	/** 판에 들어가기 전: 타이틀 한 장 → 월드맵 한 장. 다 찍으면 true (게임 모드가 판을 시작한다) */
	bool TickMenus(float RealDeltaSeconds);
	/** 스테이지마다 영웅을 돌아가며 (마지막 영웅부터, 새 영웅을 먼저 써 보게) */
	void PickHeroFor(int32 Stage);

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
	bool bCardShot = false;
	bool bOptionsShot = false;
	bool bCutShot = false;
	bool bBuyShot = false;
	bool bBurnShot = false;
	bool bPotShot = false;
	bool bTalkShot = false;
	bool bPromoteShot = false;
	bool bSmackShot = false;
	bool bHeroSkillShot = false;
	bool bBossSkillShot = false;
	bool bSkillTextShot = false;
	/** 줌 확인: 0 = 가장 멀리 한 장, 1 = 가장 가까이 한 장, 2 = 끝 */
	int32 ZoomShotStep = 0;
	/** 그 줌을 유지하는 남은 시간 (실제 초, 스크린샷이 저장될 때까지) */
	float ZoomShotHold = 0.f;
	// 배속 토글 확인: 두 번째 웨이브에서 토글을 눌러 게임 시간이 몇 배로 흐르는지 재고 되돌린다 (0 아직, 1 재는 중, 2 끝)
	int32 SpeedCheck = 0;
	float SpeedCheckReal = 0.f;
	float SpeedCheckStart = 0.f;
	static constexpr int32 SpeedCheckWave = 2;
	static constexpr float SpeedCheckSeconds = 4.f;
	int32 MenuStep = 0;
	bool bEndingShot = false;
	bool bResultShot = false;
	float ResultWait = 0.f;
	float MenuTimer = 0.f;
	float CardShotWait = 0.f;

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
	static constexpr float BuyShotAt = 0.85f;
	static constexpr int32 BuyShotBuilding = 1;
	static constexpr float TreeShotAt = 0.6f;
	/** 주문 연출이 이만큼 지났을 때 한 장 (갈라진 땅·불바다 확인용) */
	static constexpr float SpellShotAt = 0.15f;
	/** 말풍선이 이만큼 (초) 나온 뒤 한 장 */
	static constexpr float TalkShotAt = 1.2f;
	static constexpr float Half = 0.5f;
	/** 진급 스크린샷: 빛이 이만큼 진행됐을 때, 이 확대로 */
	static constexpr float PromoteShotAt = 0.3f;
	static constexpr float PromoteShotZoom = 1.6f;
	/** 국자 스크린샷: 내려오는 중 이만큼일 때 */
	static constexpr float SmackShotAt = 0.7f;
	/** 줌 확인 장면: 이 웨이브부터, 아주 작은 값·큰 값을 주면 HUD 가 최소·최대로 맞춘다 */
	static constexpr int32 ZoomShotWave = 2;
	static constexpr float ZoomOutShot = 0.01f;
	static constexpr float ZoomInShot = 100.f;
	static constexpr float ZoomShotHoldTime = 0.6f;
	/** 큰 스킬: 쾅 고리가 이만큼 퍼졌을 때, 보스 예고 원이 이만큼 찼을 때, 싹쓸이 글자가 이만큼 떴을 때 */
	static constexpr float SkillBlastShotAt = 0.25f;
	static constexpr float BossWarnShotAt = 0.6f;
	static constexpr float SkillTextShotAt = 0.15f;
	/** 메뉴 화면이 그려질 때까지 기다리는 실제 시간 */
	static constexpr float MenuShotWait = 0.6f;
	/** 결과 창 별 연출이 다 나올 때까지 (실제 시간) */
	static constexpr float ResultShotDelay = 4.0f;
	/** 그다음 옵션 화면 */
	static constexpr float OptionsShotAt = 0.25f;
	/** 컷씬 첫 줄이 이만큼 지나면 찍는다 */
	static constexpr float CutShotAt = 0.9f;
	static constexpr const TCHAR* ShotPrefix = TEXT("Play_");
};
