#include "Game/YJJAutoPilot.h"
#include "Data/YJJGameData.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "Sim/YJJBattle.h"
#include "UI/YJJWaveHUD.h"
#include "YJJWave.h"
#include "YJJWaveGameMode.h"

FYJJAutoPilot::FYJJAutoPilot(AYJJWaveGameMode* InMode, float InDuration, float InShotInterval, bool bInBench)
	: Mode(InMode)
	, Duration(InDuration)
	, ShotInterval(InShotInterval)
	, bBench(bInBench)
{
	Random.Initialize(RandomSeed);
	ShotTimer = InShotInterval;
}

bool FYJJAutoPilot::TickMenus(float RealDeltaSeconds)
{
	MenuTimer += RealDeltaSeconds;
	if (MenuTimer < MenuShotWait)
	{
		return false;
	}
	MenuTimer = 0.f;
	switch (MenuStep++)
	{
	case 0: Mode->TakeShot(FString::Printf(TEXT("%sTitle"), ShotPrefix)); return false;
	case 1: Mode->GoToWorldMap(); return false;
	case 2: Mode->TakeShot(FString::Printf(TEXT("%sWorldMap"), ShotPrefix)); return false;
	default: return true;
	}
}

void FYJJAutoPilot::Tick(float RealDeltaSeconds)
{
	FYJJBattle& Battle = Mode->GetBattle();
	const FYJJTuning& T = YJJData::Tune();
	APlayerController* Player = Mode->GetWorld()->GetFirstPlayerController();
	AYJJWaveHUD* HUD = Player ? Cast<AYJJWaveHUD>(Player->GetHUD()) : nullptr;

	if (bFinished)
	{
		// 마지막 스크린샷이 저장될 시간을 준 뒤 종료
		ExitTimer -= RealDeltaSeconds;
		if (!bBuyShot && HUD && ExitTimer < T.BotExitDelay * BuyShotAt)
		{
			bBuyShot = true;
			HUD->SetBuyPopup(BuyShotBuilding);
			Mode->TakeShot(FString::Printf(TEXT("%sBuy"), ShotPrefix));
		}
		if (!bOptionsShot && HUD && ExitTimer < T.BotExitDelay * OptionsShotAt)
		{
			HUD->SetBuyPopup(INDEX_NONE);
			bOptionsShot = true;
			HUD->SetTreeOpen(false);
			HUD->SetOptionsOpen(true);
			Mode->TakeShot(FString::Printf(TEXT("%sOptions"), ShotPrefix));
		}
		if (!bTreeShot && HUD && ExitTimer < T.BotExitDelay * TreeShotAt)
		{
			bTreeShot = true;
			HUD->SetBuyPopup(INDEX_NONE);
			HUD->SetTreeOpen(true);
			// 주문 별 하나의 설명창을 함께 찍는다 (Lv·다음 Lv·특수 효과)
			for (int32 Perk = 0; Perk < YJJData::Perks().Num(); ++Perk)
			{
				if (YJJData::Perks()[Perk].Kind == EYJJPerkKind::Spell)
				{
					HUD->SetForcedHoverPerk(Perk);
					break;
				}
			}
			Mode->TakeShot(FString::Printf(TEXT("%sTree"), ShotPrefix));
		}
		if (ExitTimer <= 0.f)
		{
			FPlatformMisc::RequestExit(false, TEXT("YJJAutoPilot"));
		}
		return;
	}

	// 성능 기록
	StepMsSum += Battle.Horde.LastStepMs();
	FrameMsSum += RealDeltaSeconds * MsPerSecond;
	++Samples;
	WorstStepMs = FMath::Max(WorstStepMs, Battle.Horde.LastStepMs());
	const int32 Alive = Battle.Horde.Num(EYJJTeam::Player) + Battle.Horde.Num(EYJJTeam::Enemy);
	PeakUnits = FMath::Max(PeakUnits, Alive);
	if (Alive >= PeakUnits * DenseShare && Alive > 0)
	{
		DenseStepMsSum += Battle.Horde.LastStepMs();
		DenseFrameMsSum += RealDeltaSeconds * MsPerSecond;
		DenseWorstFrameMs = FMath::Max(DenseWorstFrameMs, RealDeltaSeconds * MsPerSecond);
		for (int32 Phase = 0; Phase < static_cast<int32>(EYJJHordePhase::Count); ++Phase)
		{
			DensePhaseMsSum[Phase] += Battle.Horde.LastPhaseMs(static_cast<EYJJHordePhase>(Phase));
		}
		DenseWaitMsSum += Mode->LastSimWaitMs();
		DenseReorderMsSum += Battle.Horde.LastReorderMs();
		if (HUD)
		{
			DenseUnitDrawMsSum += HUD->LastUnitDrawMs();
			DenseVisibleSum += HUD->LastVisibleUnits();
		}
		++DenseSamples;
	}

	if (HUD)
	{
		HUD->SetTreeOpen(false);
		FollowFront(Battle, HUD, RealDeltaSeconds);
		// 냄비: 봇이 열 단계가 되면 냄비를 비추고 한 장 (뚜껑은 그다음 판단에서 연다)
		if (!bPotShot && Battle.PotStage() >= YJJData::Tune().BotPotStage)
		{
			bPotShot = true;
			HUD->FocusWorld(Battle.PotPos() - FVector2f(0.f, YJJData::Layout().PotDrawSize * Half), YJJData::Tune().BotPotZoom);
			Mode->TakeShot(FString::Printf(TEXT("%sPot"), ShotPrefix));
		}
		// 말풍선 대화: 처음 것이 조금 나온 뒤 한 장
		if (!bTalkShot && HUD->TalkAge() >= TalkShotAt)
		{
			bTalkShot = true;
			Mode->TakeShot(FString::Printf(TEXT("%sTalk"), ShotPrefix));
		}
	}

	// 스킬 연출 확인용: 불바다·진급·국자가 처음 보이면 한 장씩
	for (const FYJJEffect& Effect : Battle.Effects)
	{
		const bool bShowing = Effect.Age >= Effect.Life * SpellShotAt;
		if (!bPromoteShot && HUD && Effect.Kind == EYJJEffectKind::Promote && Effect.Age >= Effect.Life * PromoteShotAt)
		{
			bPromoteShot = true;
			HUD->FocusWorld(Effect.Pos, PromoteShotZoom);
			Mode->TakeShot(FString::Printf(TEXT("%sPromote"), ShotPrefix));
		}
		if (!bSmackShot && HUD && Effect.Kind == EYJJEffectKind::SmackFall && Effect.Age >= Effect.Life * SmackShotAt)
		{
			bSmackShot = true;
			HUD->FocusWorld(Effect.Pos, PromoteShotZoom);
			Mode->TakeShot(FString::Printf(TEXT("%sSmack"), ShotPrefix));
		}
		if (!bBurnShot && bShowing && Effect.Kind == EYJJEffectKind::Burn)
		{
			bBurnShot = true;
			Mode->TakeShot(FString::Printf(TEXT("%sBurn"), ShotPrefix));
		}
	}

	// 컷씬: 첫 번째만 대사가 다 나온 뒤 한 장 찍고, 나머지는 건너뛴다
	if (const FYJJCutLine* Line = Mode->CutsceneLine())
	{
		if (!bCutShot)
		{
			if (Mode->CutsceneTime() >= Line->Duration * CutShotAt)
			{
				bCutShot = true;
				Mode->TakeShot(FString::Printf(TEXT("%sCutscene"), ShotPrefix));
			}
			return;
		}
		Mode->SkipCutscene();
	}

	// 카드: 아무거나 하나
	if (Battle.bCardPending)
	{
		// 처음 한 번은 카드 화면을 찍고 조금 뒤에 고른다 (스크린샷이 저장될 시간)
		if (!bCardShot)
		{
			bCardShot = true;
			CardShotWait = YJJData::Tune().BotExitDelay;
			Mode->TakeShot(FString::Printf(TEXT("%sCard"), ShotPrefix));
		}
		CardShotWait -= RealDeltaSeconds;
		if (CardShotWait > 0.f)
		{
			return;
		}
		Battle.PickCard(Random.RandHelper(Battle.CardOffer.Num()));
	}

	ThinkTimer -= RealDeltaSeconds * Mode->GetSpeed();
	if (ThinkTimer <= 0.f)
	{
		ThinkTimer = T.BotThinkInterval;
		Think(Battle);
	}

	if (Battle.Level != LastLevel)
	{
		LastLevel = Battle.Level;
		UE_LOG(LogYJJWave, Display, TEXT("[AutoPilot] 웨이브 %d: 아군 %d, 적 %d, 병영 %d, 특성 %d, 금화 %.0f, 시뮬 %.2fms"),
			Battle.Level, Battle.Horde.Num(EYJJTeam::Player), Battle.Horde.Num(EYJJTeam::Enemy),
			Battle.Plots.FilterByPredicate([](const FYJJPlot& Plot) { return Plot.Building != INDEX_NONE; }).Num(),
			Battle.NumAllocated(), Battle.Gold, Battle.Horde.LastStepMs());
	}

	if (ShotInterval > 0.f)
	{
		ShotTimer -= RealDeltaSeconds;
		if (ShotTimer <= 0.f)
		{
			ShotTimer = ShotInterval;
			Mode->TakeShot(FString::Printf(TEXT("%s%02d"), ShotPrefix, ShotCount++));
		}
	}

	// 이긴 마무리(보스 쓰러짐) 한 장, 결과 창은 별이 다 나온 뒤 한 장
	if (Battle.bEnding && !Battle.bGameOver && !bEndingShot)
	{
		bEndingShot = true;
		Mode->TakeShot(FString::Printf(TEXT("%sEnding%d"), ShotPrefix, Battle.Stage + 1));
	}
	ResultWait = Battle.bGameOver ? ResultWait + RealDeltaSeconds : 0.f;
	if (Battle.bGameOver && ResultWait < ResultShotDelay)
	{
		return;
	}
	// 결과 창을 찍고, 그 화면이 저장될 때까지 조금 더 기다린 뒤 다음 스테이지
	if (Battle.bGameOver && !bResultShot)
	{
		bResultShot = true;
		Mode->TakeShot(FString::Printf(TEXT("%sStage%d"), ShotPrefix, Battle.Stage + 1));
		return;
	}
	if (Battle.bGameOver && Mode->HasNextStage() && ResultWait >= ResultShotDelay + MenuShotWait)
	{
		bEndingShot = bResultShot = false;
		ResultWait = 0.f;
		Mode->NextStage();
		return;
	}
	if (Battle.bGameOver && Mode->HasNextStage())
	{
		return;
	}
	TotalTime += RealDeltaSeconds * Mode->GetSpeed();
	if (Battle.bGameOver || (Duration > 0.f && TotalTime >= Duration))
	{
		Finish(Battle);
	}
}

void FYJJAutoPilot::FollowFront(const FYJJBattle& Battle, AYJJWaveHUD* HUD, float RealDeltaSeconds)
{
	// 두 편 앞줄 사이(부딪히는 곳)를 따라간다
	const FYJJTuning& T = YJJData::Tune();
	const float PlayerFront = Battle.Horde.FrontX(EYJJTeam::Player);
	const float EnemyFront = Battle.Horde.FrontX(EYJJTeam::Enemy);
	float X = Battle.CampEdgeX;
	if (PlayerFront > 0.f && EnemyFront > 0.f)
	{
		X = (PlayerFront + EnemyFront) * 0.5f;
	}
	else if (EnemyFront > 0.f)
	{
		X = EnemyFront;
	}
	if (bBench)
	{
		X = Battle.FieldWidth * 0.5f;
	}
	const FVector2f Target(X, Battle.FieldHeight * 0.5f);
	const FVector2f Current = HUD->CameraCenter();
	const float Blend = FMath::Min(T.BotCameraFollow * RealDeltaSeconds, 1.f);
	HUD->FocusWorld(Current + (Target - Current) * Blend, bBench ? T.BenchCameraZoom : T.BotCameraZoom);
}

void FYJJAutoPilot::Think(FYJJBattle& Battle)
{
	SpendSouls(Battle);
	SpendGold(Battle);
	CastSpells(Battle);
	if (bPotShot && Battle.PotStage() >= YJJData::Tune().BotPotStage)
	{
		Battle.OpenPot();
	}
	// 국자: 적 앞줄을 내려친다 (사람이 클릭하듯)
	const float EnemyFront = Battle.Horde.FrontX(EYJJTeam::Enemy);
	if (EnemyFront > 0.f && Battle.CanSmack())
	{
		Battle.Cast(EYJJSpell::Ladle, FVector2f(EnemyFront + YJJData::Tune().BotMeteorLead * Half, Random.FRandRange(Battle.FieldHeight * Half - YJJData::Tune().BotSmackSpread, Battle.FieldHeight * Half + YJJData::Tune().BotSmackSpread)));
	}
}

int32 FYJJAutoPilot::PickBuilding(const FYJJBattle& Battle) const
{
	// 지은 수 / 비중이 가장 작은 병영 (비중 0은 안 짓는다)
	int32 Best = INDEX_NONE;
	float BestRatio = TNumericLimits<float>::Max();
	for (int32 Building = 0; Building < YJJData::Buildings().Num(); ++Building)
	{
		const float Weight = YJJData::Building(Building).BotWeight;
		if (Weight <= 0.f)
		{
			continue;
		}
		const float Ratio = Battle.BuildingCount(Building) / Weight;
		if (Ratio < BestRatio)
		{
			BestRatio = Ratio;
			Best = Building;
		}
	}
	return Best;
}

void FYJJAutoPilot::SpendGold(FYJJBattle& Battle)
{
	// 빈 칸이 있으면 짓는 데 돈을 모은다
	const int32 Building = PickBuilding(Battle);
	const int32 Empty = Battle.Plots.IndexOfByPredicate([](const FYJJPlot& Plot) { return Plot.Building == INDEX_NONE; });
	if (Empty != INDEX_NONE && Building != INDEX_NONE)
	{
		Battle.Build(Empty, Building);
		return;
	}
	// 다 찼으면 가장 싼 강화부터
	int32 Cheapest = INDEX_NONE;
	for (int32 Plot = 0; Plot < Battle.Plots.Num(); ++Plot)
	{
		const int32 Cost = Battle.UpgradeCost(Plot);
		if (Cost > 0 && (Cheapest == INDEX_NONE || Cost < Battle.UpgradeCost(Cheapest)))
		{
			Cheapest = Plot;
		}
	}
	if (Cheapest != INDEX_NONE)
	{
		Battle.Upgrade(Cheapest);
	}
	// 금화가 남으면 병사를 바로 산다 (사람이 x10을 누르듯)
	const FYJJTuning& T = YJJData::Tune();
	if (Building != INDEX_NONE && Battle.Gold >= T.BotBuyGold)
	{
		Battle.Buy(Building, FMath::RoundToInt(YJJData::Layout().BuyCountC));
	}
}

void FYJJAutoPilot::SpendSouls(FYJJBattle& Battle)
{
	while (Battle.Souls >= Battle.PerkCost())
	{
		TArray<int32> Choices;
		TArray<int32> Notables;
		for (int32 Perk = 0; Perk < YJJData::Perks().Num(); ++Perk)
		{
			if (Battle.IsReachable(Perk))
			{
				Choices.Add(Perk);
				// 봇은 중간 별·주문 별을 먼저
				if (YJJData::Perks()[Perk].Kind == EYJJPerkKind::Notable || YJJData::Perks()[Perk].Kind == EYJJPerkKind::Spell)
				{
					Notables.Add(Perk);
				}
			}
		}
		const TArray<int32>& From = Notables.Num() > 0 ? Notables : Choices;
		if (From.Num() == 0 || !Battle.Allocate(From[Random.RandHelper(From.Num())]))
		{
			return;
		}
	}
}

void FYJJAutoPilot::CastSpells(FYJJBattle& Battle)
{
	const FYJJTuning& T = YJJData::Tune();
	const float EnemyFront = Battle.Horde.FrontX(EYJJTeam::Enemy);
	if (EnemyFront > 0.f && Battle.CanCast(EYJJSpell::Meteor))
	{
		const float Radius = Battle.SpellNow(EYJJSpell::Meteor).Radius;
		Battle.Cast(EYJJSpell::Meteor, FVector2f(EnemyFront + T.BotMeteorLead, Random.FRandRange(Radius, Battle.FieldHeight - Radius)));
	}
	if (Battle.Horde.Num(EYJJTeam::Enemy) > Battle.Horde.Num(EYJJTeam::Player) * T.BotRallyRatio)
	{
		Battle.Cast(EYJJSpell::Rally, FVector2f::ZeroVector);
	}
}

void FYJJAutoPilot::Finish(const FYJJBattle& Battle)
{
	bFinished = true;
	ExitTimer = YJJData::Tune().BotExitDelay;
	Mode->TakeShot(FString::Printf(TEXT("%sEnd"), ShotPrefix));
	const double AvgStep = Samples > 0 ? StepMsSum / Samples : 0.0;
	const double AvgFrame = Samples > 0 ? FrameMsSum / Samples : 0.0;
	UE_LOG(LogYJJWave, Display, TEXT("[AutoPilot] DONE %s: 스테이지 %d 웨이브 %d, 시간 %.0f초, 처치 %d, 영혼 %.0f, 특성 %.0f개, 최대 유닛 %d, 시뮬 평균 %.2fms 최악 %.2fms, 프레임 평균 %.1fms (%.0f fps)"),
		Battle.bGameOver ? (Battle.bVictory ? TEXT("모든 스테이지 돌파") : TEXT("패배")) : TEXT("시간 끝"), Battle.Stage + 1, Battle.Level, Battle.Time, Battle.Kills, Battle.Souls, static_cast<float>(Battle.NumAllocated()),
		PeakUnits, AvgStep, WorstStepMs, AvgFrame, AvgFrame > 0.0 ? MsPerSecond / AvgFrame : 0.0);
	const double DenseStep = DenseSamples > 0 ? DenseStepMsSum / DenseSamples : 0.0;
	const double DenseFrame = DenseSamples > 0 ? DenseFrameMsSum / DenseSamples : 0.0;
	UE_LOG(LogYJJWave, Display, TEXT("[AutoPilot] DENSE 최대 유닛 %d의 %.0f%% 이상 구간 (%d프레임): 시뮬 틱 %.2fms, 프레임 평균 %.1fms (%.0f fps), 최악 프레임 %.1fms"),
		PeakUnits, DenseShare * 100.f, DenseSamples, DenseStep, DenseFrame, DenseFrame > 0.0 ? MsPerSecond / DenseFrame : 0.0, DenseWorstFrameMs);
	const double DenseCount = FMath::Max(DenseSamples, 1);
	UE_LOG(LogYJJWave, Display, TEXT("[AutoPilot] PHASE 격자 %.2fms · 생각·이동(병렬) %.2fms · 전투 %.2fms · 정리 %.2fms · 재배치(마지막) %.2fms | 유닛 그리기 CPU %.2fms (화면 안 %.0f마리) | 시뮬 대기 %.2fms"),
		DensePhaseMsSum[0] / DenseCount, DensePhaseMsSum[1] / DenseCount, DensePhaseMsSum[2] / DenseCount, DensePhaseMsSum[3] / DenseCount, DenseReorderMsSum / DenseCount,
		DenseUnitDrawMsSum / DenseCount, static_cast<double>(DenseVisibleSum) / DenseCount, DenseWaitMsSum / DenseCount);
}
