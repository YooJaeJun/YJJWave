// 게임 중 말풍선 대화 (Dialogues.csv): 판 사건(시작·웨이브·보스·진급·냄비·영웅 위기)을 듣고
// 조건이 맞는 대화를 한 판에 한 번, 화자 머리 위 말풍선으로 주고받는다. 컷씬 중에는 멈춘다
#include "Audio/YJJAudio.h"
#include "Data/YJJGameData.h"
#include "Data/YJJLoc.h"
#include "Engine/Canvas.h"
#include "Sim/YJJBattle.h"
#include "UI/YJJUIStyle.h"
#include "UI/YJJWaveHUD.h"
#include "YJJWaveGameMode.h"

bool AYJJWaveHUD::TalkMatches(const FYJJTalk& Talk, const FYJJBattleEvent& Event) const
{
	const FYJJBattle* Current = Battle();
	return Talk.Trigger == Event.Trigger
		&& (Talk.Stage == INDEX_NONE || Talk.Stage == Current->Stage)
		&& (Talk.Wave == 0 || Talk.Wave == Event.Wave)
		&& (Talk.MinWave == 0 || Event.Wave >= Talk.MinWave)
		&& (Talk.Param == INDEX_NONE || Talk.Param == Event.Unit);
}

void AYJJWaveHUD::TakeEvents()
{
	FYJJBattle* Current = Battle();
	const TConstArrayView<FYJJTalk> Talks = YJJData::Talks();
	if (TalkUsed.Num() != Talks.Num())
	{
		TalkUsed.Init(0, Talks.Num());
	}
	for (const FYJJBattleEvent& Event : Current->Events)
	{
		// 큰 일은 화면 배너도
		if (Event.Trigger == EYJJTalkTrigger::Promote && YJJData::Units().IsValidIndex(Event.Unit) && YJJData::Unit(Event.Unit).Tier >= UI().PromoteBannerTier)
		{
			AddBanner(YJJLoc::Format(TEXT("Promote.Banner"), { YJJLoc::Get(YJJData::Unit(Event.Unit).NameKey) }), YJJUI::Highlight);
		}
		else if (Event.Trigger == EYJJTalkTrigger::PotOpen && YJJData::Broth().IsValidIndex(Event.Unit))
		{
			AddBanner(YJJLoc::Format(TEXT("Pot.OpenBanner"), { YJJLoc::Get(YJJData::Broth()[Event.Unit].NameKey) }), YJJUI::Good);
		}
		else if (Event.Trigger == EYJJTalkTrigger::PotOverflow)
		{
			AddBanner(YJJLoc::Get(TEXT("Pot.OverflowBanner")), YJJUI::Bad);
		}
		for (int32 Index = 0; Index < Talks.Num(); ++Index)
		{
			if (!TalkUsed[Index] && Talks[Index].Lines.Num() > 0 && TalkMatches(Talks[Index], Event))
			{
				TalkUsed[Index] = 1;
				if (Random.FRand() <= Talks[Index].Chance)
				{
					TalkQueue.Add(Index);
				}
			}
		}
	}
	Current->Events.Reset();
}

void AYJJWaveHUD::UpdateTalks(float Dt)
{
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	if (Mode && Mode->CutsceneLine())
	{
		return;
	}
	const TConstArrayView<FYJJTalk> Talks = YJJData::Talks();
	if (TalkNow.Talk == INDEX_NONE || !Talks.IsValidIndex(TalkNow.Talk))
	{
		if (TalkQueue.Num() == 0)
		{
			return;
		}
		TalkNow = FYJJTalkPlay();
		TalkNow.Talk = TalkQueue[0];
		TalkQueue.RemoveAt(0);
		TalkNow.Age = -Talks[TalkNow.Talk].Lines[0].Delay;
	}
	const FYJJTalk& Talk = Talks[TalkNow.Talk];
	TalkNow.Age += Dt;
	const FYJJTalkLine& Line = Talk.Lines[TalkNow.Line];
	if (TalkNow.Age >= 0.f && !TalkNow.bVoiced)
	{
		// 녹음한 대사가 없으면 그 유닛 웅얼웅얼
		TalkNow.bVoiced = true;
		const bool bLine = FYJJAudio::Has(Line.VoiceKey);
		FYJJAudio::Play(this, bLine || !YJJData::Units().IsValidIndex(Line.Speaker) ? Line.VoiceKey : YJJData::Unit(Line.Speaker).VoiceKey);
	}
	if (TalkNow.Age >= Line.Duration)
	{
		++TalkNow.Line;
		TalkNow.bVoiced = false;
		if (Talk.Lines.IsValidIndex(TalkNow.Line))
		{
			TalkNow.Age = -Talk.Lines[TalkNow.Line].Delay;
		}
		else
		{
			TalkNow = FYJJTalkPlay();
		}
	}
}

int32 AYJJWaveHUD::FindSpeaker(int32 Unit) const
{
	const FYJJHordeView& Horde = View();
	int32 Best = INDEX_NONE;
	float BestDist = TNumericLimits<float>::Max();
	for (int32 Slot = 0; Slot < Horde.HighWater; ++Slot)
	{
		if (Horde.Alive[Slot] && Horde.Type[Slot] == Unit)
		{
			const float Dist = FVector2f::DistSquared(Horde.Pos[Slot], CamCenter);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				Best = Slot;
			}
		}
	}
	return Best;
}

void AYJJWaveHUD::DrawTalks()
{
	const TConstArrayView<FYJJTalk> Talks = YJJData::Talks();
	const AYJJWaveGameMode* Mode = GetWorld()->GetAuthGameMode<AYJJWaveGameMode>();
	if (!Talks.IsValidIndex(TalkNow.Talk) || TalkNow.Age < 0.f || (Mode && Mode->CutsceneLine()) || bTreeOpen)
	{
		return;
	}
	const FYJJUILayout& L = UI();
	const FYJJTalkLine& Line = Talks[TalkNow.Talk].Lines[TalkNow.Line];
	if (!YJJData::Units().IsValidIndex(Line.Speaker))
	{
		return;
	}
	const FYJJUnitDef& Speaker = YJJData::Unit(Line.Speaker);

	// 꼬리 끝: 화자 머리 위. 화자가 아직 없으면 (졸병이 나오기 전) 영웅 곁에서 이름만 달고
	int32 Slot = FindSpeaker(Line.Speaker);
	if (Slot == INDEX_NONE)
	{
		Slot = FindSpeaker(YJJData::HeroUnit());
	}
	const float Left = L.SidePanelW + L.BubbleScreenMargin;
	FVector2f Head(WorldCenterVirtualX(), L.BubbleScreenMargin * 2.f + L.BubbleMaxW * Half);
	if (Slot != INDEX_NONE)
	{
		const FYJJUnitDef& Anchor = YJJData::Unit(View().Type[Slot]);
		Head = WorldToVirtual(View().Pos[Slot] - FVector2f(0.f, Anchor.DrawSize * L.BubbleHeadShare));
	}

	// 크기는 다 나온 글 기준 (치는 동안 상자가 커지지 않게)
	const FString& Full = YJJLoc::Get(Line.TextKey);
	const FString Name = YJJLoc::Get(Speaker.NameKey);
	const float Pad = L.BubblePad;
	const TArray<FString> Rows = WrapText(Full, L.MidText, L.BubbleMaxW - Pad * 2.f);
	float TextW = 0.f;
	for (const FString& Row : Rows)
	{
		float RowW = 0.f;
		MakeFont(Row, L.MidText, 1.f, 0.f, RowW);
		TextW = FMath::Max(TextW, RowW / ViewScale);
	}
	float NameW = 0.f;
	MakeFont(Name, L.SmallText, 1.f, 0.f, NameW);
	const float W = FMath::Min(FMath::Max(TextW, NameW / ViewScale) + Pad * 2.f, L.BubbleMaxW);
	const float H = Pad * 2.f + L.SmallText + L.TipLineH * Rows.Num();
	const float X = FMath::Clamp(Head.X - W * Half, Left, VirtualW - W - L.BubbleScreenMargin);
	const float Y = FMath::Max(Head.Y - L.BubbleGap - L.BubbleTail - H, L.BubbleScreenMargin);

	// 나타났다 사라진다
	const float Fade = FMath::Max(L.BubbleFade, KINDA_SMALL_NUMBER);
	const float Alpha = FMath::Clamp(FMath::Min(TalkNow.Age / Fade, (Line.Duration - TalkNow.Age) / Fade), 0.f, 1.f);
	const FVector2f TailFrom(FMath::Clamp(Head.X, X + Pad, X + W - Pad), Y + H);
	DrawLine(TailFrom, FVector2f(Head.X, Head.Y - L.BubbleGap), YJJUI::WithAlpha(YJJUI::Ink, Alpha), L.BubbleTail);
	DrawPanel(X, Y, W, H, YJJUI::WithAlpha(YJJUI::PanelTint, Alpha), FMath::RoundToInt(L.UiPanelDark));
	DrawLabel(Name, X + Pad, Y + Pad, L.SmallText, YJJUI::WithAlpha(Speaker.Team == EYJJTeam::Enemy ? YJJUI::BossText : YJJUI::Highlight, Alpha));

	// 글자가 하나씩 (줄을 넘어가며)
	int32 CharsLeft = FMath::FloorToInt(TalkNow.Age * L.BubbleTypeSpeed);
	for (int32 Row = 0; Row < Rows.Num() && CharsLeft > 0; ++Row)
	{
		DrawLabel(Rows[Row].Left(CharsLeft), X + Pad, Y + Pad + L.SmallText + L.TipLineH * Row, L.MidText, YJJUI::WithAlpha(YJJUI::Paper, Alpha));
		CharsLeft -= Rows[Row].Len();
	}
}
