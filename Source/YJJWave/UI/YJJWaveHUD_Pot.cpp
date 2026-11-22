// 왕국의 마지막 냄비: 적의 뼈가 날아와 국이 끓는다. 오래 끓일수록 진해지고, 곰탕은 넘치기 전에 열어야 한다
#include "CanvasTypes.h"
#include "Data/YJJGameData.h"
#include "Data/YJJLoc.h"
#include "Engine/Canvas.h"
#include "Sim/YJJBattle.h"
#include "UI/YJJUIStyle.h"
#include "UI/YJJWaveHUD.h"

bool AYJJWaveHUD::IsOverPot(const FVector2f& World) const
{
	const FYJJBattle* Current = Battle();
	if (!Current || YJJData::Broth().Num() == 0)
	{
		return false;
	}
	// 냄비 발밑에서 그림 높이만큼 위까지, 그림 너비의 절반
	const FVector2f Feet = Current->PotPos();
	const float Size = UI().PotDrawSize;
	return FMath::Abs(World.X - Feet.X) <= Size * Half && World.Y <= Feet.Y && World.Y >= Feet.Y - Size * UI().PotAnchorY;
}

void AYJJWaveHUD::OpenPot()
{
	if (FYJJBattle* Current = EditBattle())
	{
		Current->OpenPot();
	}
}

void AYJJWaveHUD::DrawPot()
{
	const FYJJBattle* Current = Battle();
	const TConstArrayView<FYJJBrothStage> Broth = YJJData::Broth();
	if (!BuildingAtlas.Texture || !FxAtlas.Texture || Broth.Num() == 0)
	{
		return;
	}
	const FYJJUILayout& L = UI();
	const float Size = L.PotDrawSize * Zoom * (bPotHover ? L.PotHoverScale : 1.f);
	const FVector2f Feet = WorldToScreen(Current->PotPos());
	const FVector2f TopLeft(Feet.X - Size * Half, Feet.Y - Size * L.PotAnchorY);
	const int32 Stage = Current->PotStage();
	const float OverflowLeft = Current->PotOverflowLeft();

	// 솥과 장작불
	{
		const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, BuildingAtlas.Texture, 1);
		AddQuad(Batch, TopLeft, FVector2f(Size), BuildingAtlas.UV(FMath::RoundToInt(L.PotCell), 0), BuildingAtlas.CellUV, bPotHover ? YJJUI::Highlight : FLinearColor::White, false);
	}

	// 국물 면: 뼈가 들어갈수록 차오르고, 단계마다 색이 진해진다. 거품과 김은 단계가 오를수록 많이
	const FVector2f Rim = TopLeft + FVector2f(Size * Half, Size * L.PotRimY);
	const FVector2f RimSize(Size * L.PotRimW, Size * L.PotRimH);
	const float FirstBones = FMath::Max(Broth[0].Bones, KINDA_SMALL_NUMBER);
	const float Fill = FMath::Clamp(Current->PotBones() / FirstBones, 0.f, 1.f);
	const int32 NumStages = Broth.Num();
	const float Heat = static_cast<float>(Stage + 1) / NumStages;
	{
		const int32 Bubbles = FMath::RoundToInt(L.PotBubbles * Heat);
		const int32 Puffs = FMath::Min(Stage + 1, FMath::RoundToInt(L.PotSteamPuffs));
		const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, FxAtlas.Texture, 1 + Bubbles + Puffs);
		if (Current->PotBones() > 0.f)
		{
			const FLinearColor Color = Stage == INDEX_NONE ? YJJUI::WithAlpha(Broth[0].Color, Fill) : Broth[Stage].Color;
			const FVector2f Surface = RimSize * FMath::Lerp(L.PotFillMin, 1.f, Fill);
			AddQuad(Batch, Rim - Surface * Half, Surface, FxAtlas.UV(FMath::RoundToInt(L.FxBroth)), FxAtlas.CellUV, Color, false);
			const FLinearColor BubbleColor = FMath::Lerp(Color, FLinearColor::White, Half);
			for (int32 Index = 0; Index < Bubbles; ++Index)
			{
				// 자리는 번호로 고정, 크기는 박자에 맞춰 부풀었다 터진다
				const float Phase = FMath::Frac(AnimClock * L.PotBubbleSpeed * (1.f + Index * PotBubbleTempo) + Index * Half);
				const FVector2f Spot = Rim + FVector2f(FMath::Sin(Index * PotBubbleSeedX) * RimSize.X * PotBubbleSpreadX, FMath::Cos(Index * PotBubbleSeedY) * RimSize.Y * PotBubbleSpreadY);
				const float Bubble = RimSize.X * L.PotBubbleSize * FMath::Sin(Phase * UE_PI);
				AddQuad(Batch, Spot - FVector2f(Bubble * Half), FVector2f(Bubble), FxAtlas.UV(FMath::RoundToInt(L.FxBroth)), FxAtlas.CellUV, BubbleColor, false);
			}
		}
		for (int32 Index = 0; Index < Puffs; ++Index)
		{
			const float Phase = FMath::Frac(AnimClock * Half + static_cast<float>(Index) / FMath::Max(Puffs, 1));
			const float Puff = L.PotSteamPuffSize * Zoom * FMath::Lerp(Half, 1.f, Phase);
			const FVector2f Spot = Rim + FVector2f(FMath::Sin(Phase * FullTurn + Index) * RimSize.X * PotSteamSpread, -L.PotSteamRise * Zoom * Phase);
			AddQuad(Batch, Spot - FVector2f(Puff * Half), FVector2f(Puff), FxAtlas.UV(FMath::RoundToInt(L.FxSteam)), FxAtlas.CellUV, YJJUI::WithAlpha(FLinearColor::White, L.PotSteamAlpha * FMath::Sin(Phase * UE_PI)), false);
		}
	}

	// 뚜껑: 평소엔 비스듬히 걸쳐 두고, 넘치기 직전엔 덜컹거리고, 열면 솟았다 떨어진다
	{
		float Lift = 0.f;
		float Tilt = PotLidAjarTilt;
		const float LidAge = Current->PotLidAge();
		if (LidAge >= 0.f && LidAge < L.PotLidTime)
		{
			const float T = LidAge / FMath::Max(L.PotLidTime, KINDA_SMALL_NUMBER);
			Lift = FMath::Sin(T * UE_PI) * L.PotLidLift * Zoom;
			Tilt += T * FullTurn;
		}
		if (OverflowLeft >= 0.f)
		{
			const float Danger = 1.f - OverflowLeft / FMath::Max(YJJData::Tune().PotOverflowTime, KINDA_SMALL_NUMBER);
			Lift += FMath::Abs(FMath::Sin(AnimClock * L.PotRattleSpeed)) * L.PotRattle * Zoom * (1.f + Danger);
		}
		const FVector2f Ajar(Size * PotLidAjarX, -Size * PotLidAjarY);
		const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, BuildingAtlas.Texture, 1);
		AddQuadRotated(Batch, TopLeft + FVector2f(Size * Half) + Ajar - FVector2f(0.f, Lift), FVector2f(Size), Tilt, BuildingAtlas.UV(FMath::RoundToInt(L.PotLidCell), 0), BuildingAtlas.CellUV, FLinearColor::White, false);
	}

	// 위 글자: 지금 단계와 뼈 (마지막 단계면 넘치기까지 남은 시간)
	const FVector2f Label = WorldToVirtual(Current->PotPos() - FVector2f(0.f, L.PotDrawSize * L.PotAnchorY)) - FVector2f(0.f, L.PotLabelGap + L.SmallText);
	FString Text;
	FLinearColor Color = YJJUI::Paper;
	if (Stage == INDEX_NONE)
	{
		Text = YJJLoc::Format(TEXT("Pot.Empty"), { FMath::FloorToInt(Current->PotBones()), FMath::RoundToInt(Broth[0].Bones) });
	}
	else if (OverflowLeft >= 0.f)
	{
		Text = YJJLoc::Format(TEXT("Pot.Full"), { YJJLoc::Get(Broth[Stage].NameKey), FMath::CeilToInt(OverflowLeft) });
		Color = YJJUI::Bad;
	}
	else
	{
		Text = YJJLoc::Format(TEXT("Pot.Stage"), { YJJLoc::Get(Broth[Stage].NameKey), FMath::FloorToInt(Current->PotBones()), FMath::RoundToInt(Current->PotNextBones()) });
		Color = YJJUI::Highlight;
	}
	DrawLabel(Text, Label.X, Label.Y, L.SmallText, Color, Half);
}

void AYJJWaveHUD::DrawPotTooltip()
{
	const FYJJBattle* Current = Battle();
	const TConstArrayView<FYJJBrothStage> Broth = YJJData::Broth();
	if (!bPotHover || Current->bGameOver || Broth.Num() == 0)
	{
		return;
	}
	const int32 Stage = Current->PotStage();
	const FYJJBrothStage& Shown = Broth[FMath::Max(Stage, 0)];
	TArray<TPair<FString, FLinearColor>> Lines;
	Lines.Emplace(YJJLoc::Get(TEXT("Pot.Title")), YJJUI::Highlight);
	const FYJJUILayout& L = UI();
	const float W = L.TipW;
	const float Pad = L.TextPad;
	for (const FString& Wrapped : WrapText(YJJLoc::Get(TEXT("Pot.Desc")), L.SmallText, W - Pad * 2.f))
	{
		Lines.Emplace(Wrapped, YJJUI::Muted);
	}
	const FString Effect = FString::Printf(TEXT("%s: %s"), *YJJLoc::Get(Shown.NameKey),
		*YJJLoc::Format(TEXT("Pot.Effect"), { FMath::RoundToInt(Shown.HP * Percent), FMath::RoundToInt(Shown.Damage * Percent), FMath::RoundToInt(Shown.Heal * Percent) }));
	for (const FString& Wrapped : WrapText(Effect, L.SmallText, W - Pad * 2.f))
	{
		Lines.Emplace(Wrapped, Stage == INDEX_NONE ? YJJUI::Muted : YJJUI::Paper);
	}
	if (Current->BrothServed() > 0)
	{
		Lines.Emplace(YJJLoc::Format(TEXT("Pot.Served"), { Current->BrothServed(), FMath::RoundToInt(Current->BrothHP() * Percent), FMath::RoundToInt(Current->BrothDamage() * Percent) }), YJJUI::Good);
	}
	if (Stage != INDEX_NONE)
	{
		Lines.Emplace(YJJLoc::Get(TEXT("Pot.Hint")), YJJUI::Highlight);
	}

	const FVector2f Mouse = ToVirtual(MouseScreen());
	const float LineH = L.TipLineH;
	const float H = Pad * 2.f + LineH * Lines.Num();
	const float X = FMath::Min(Mouse.X + L.TipOffset, VirtualW - W);
	const float Y = FMath::Clamp(Mouse.Y - H, 0.f, VirtualHeight - H);
	DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));
	for (int32 Line = 0; Line < Lines.Num(); ++Line)
	{
		DrawLabel(Lines[Line].Key, X + Pad, Y + Pad + LineH * Line, L.SmallText, Lines[Line].Value, 0.f, W - Pad * 2.f);
	}
}
