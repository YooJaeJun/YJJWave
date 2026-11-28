// 왕국의 마지막 냄비: 좌측 패널 솥 칸. 적을 쓰러뜨리면 뼈 숫자가 오르고, 오래 끓일수록 진해지고, 곰탕은 넘치기 전에 열어야 한다
#include "CanvasTypes.h"
#include "Data/YJJGameData.h"
#include "Data/YJJLoc.h"
#include "Engine/Canvas.h"
#include "Sim/YJJBattle.h"
#include "UI/YJJUIStyle.h"
#include "UI/YJJWaveHUD.h"

void AYJJWaveHUD::OpenPot()
{
	if (FYJJBattle* Current = EditBattle())
	{
		Current->OpenPot();
	}
}

void AYJJWaveHUD::UpdatePotPops(float Dt)
{
	const FYJJBattle* Current = Battle();
	if (!Current)
	{
		return;
	}
	const FYJJUILayout& L = UI();
	// 뼈가 늘면 모아 두었다가 조금씩 끊어 +N으로 튀긴다 (줄면 = 열었거나 넘침: 그냥 맞춘다)
	const float Bones = Current->PotBones();
	if (Bones > LastPotBones)
	{
		PotPending += Bones - LastPotBones;
	}
	LastPotBones = Bones;
	PotPopTimer -= Dt;
	if (PotPopTimer <= 0.f && PotPending >= 1.f)
	{
		const int32 Value = FMath::FloorToInt(PotPending);
		PotPending -= Value;
		PotPopTimer = L.PotPlusInterval;
		PotNumberPop = 1.f;
		if (PotPops.Num() >= FMath::RoundToInt(L.PotPlusMax))
		{
			PotPops.RemoveAt(0);
		}
		PotPops.Add({ Value, 0.f });
	}
	for (FYJJPotPop& Pop : PotPops)
	{
		Pop.Age += Dt;
	}
	PotPops.RemoveAll([&L](const FYJJPotPop& Pop) { return Pop.Age >= L.PotPlusTime; });
	PotNumberPop = FMath::Max(PotNumberPop - Dt * L.PotNumberPopSpeed, 0.f);
}

float AYJJWaveHUD::DrawPotPanel(float X, float Y, float W)
{
	const FYJJBattle* Current = Battle();
	const TConstArrayView<FYJJBrothStage> Broth = YJJData::Broth();
	if (!BuildingAtlas.Texture || !FxAtlas.Texture || Broth.Num() == 0)
	{
		return 0.f;
	}
	const FYJJUILayout& L = UI();
	const FYJJTuning& T = YJJData::Tune();
	const float Pad = L.PotUiPad;
	const float H = L.PotUiH;
	const int32 Stage = Current->PotStage();
	const float OverflowLeft = Current->PotOverflowLeft();
	const bool bCan = Current->CanOpenPot() && !Current->IsChoosing();

	// 솥 그림 자리: 칸 왼쪽에 발을 붙인다 (김·뚜껑만 위로 살짝)
	const float BaseSize = L.PotUiSize;
	const FVector2f Feet(X + Pad + L.PotUiCol * Half, Y + H - L.PotUiFeet);
	const float Top = FMath::Min(Y, Feet.Y - BaseSize * L.PotAnchorY);
	if (!Current->IsChoosing())
	{
		AddButton(EButton::Pot, 0, X, Top, W, Y + H - Top);
	}
	const bool bHover = HoverButton(EButton::Pot);

	const float Glow = bCan ? Half + Half * FMath::Sin(AnimClock * L.GlowSpeed) : 0.f;
	DrawPanel(X, Y, W, H, FMath::Lerp(YJJUI::PanelTint, YJJUI::Highlight, Glow * L.PotUiGlow), FMath::RoundToInt(L.UiPanelDark));

	// 여기부터 솥 그림은 화면 픽셀로 (가상 좌표 × ViewScale)
	const float Scale = ViewScale;
	const float Size = BaseSize * (bHover ? L.PotHoverScale : 1.f);
	const FVector2f TopLeftV(Feet.X - Size * Half, Feet.Y - Size * L.PotAnchorY);
	const FVector2f TopLeft = ViewOffset + TopLeftV * Scale;
	const float SizePx = Size * Scale;

	// 솥과 장작불
	{
		const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, BuildingAtlas.Texture, 1);
		AddQuad(Batch, TopLeft, FVector2f(SizePx), BuildingAtlas.UV(FMath::RoundToInt(L.PotCell), 0), BuildingAtlas.CellUV, bHover ? YJJUI::Highlight : FLinearColor::White, false);
	}

	// 국물 면: 뼈가 들어갈수록 차오르고, 단계마다 색이 진해진다. 거품과 김은 단계가 오를수록 많이
	const FVector2f Rim = TopLeft + FVector2f(SizePx * Half, SizePx * L.PotRimY);
	const FVector2f RimSize(SizePx * L.PotRimW, SizePx * L.PotRimH);
	const float FirstBones = FMath::Max(Broth[0].Bones, KINDA_SMALL_NUMBER);
	const float Fill = FMath::Clamp(Current->PotBones() / FirstBones, 0.f, 1.f);
	const int32 NumStages = Broth.Num();
	const float Heat = static_cast<float>(Stage + 1) / NumStages;
	{
		const int32 Bubbles = FMath::RoundToInt(L.PotBubbles * Heat);
		const int32 Puffs = FMath::Min(Stage + 1, FMath::RoundToInt(L.PotSteamPuffs));
		// 뚜껑을 열거나 끓어 넘칠 때 크게 한 번 (넘칠 땐 더 크게)
		const float Ages[] = { Current->PotLidAge(), Current->PotOverflowAge() };
		const float BurstScales[] = { 1.f, T.PotOverflowSteamScale };
		const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, FxAtlas.Texture, 1 + Bubbles + Puffs + static_cast<int32>(UE_ARRAY_COUNT(Ages)));
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
			const float Puff = L.PotSteamPuffSize * Scale * FMath::Lerp(Half, 1.f, Phase);
			const FVector2f Spot = Rim + FVector2f(FMath::Sin(Phase * FullTurn + Index) * RimSize.X * PotSteamSpread, -L.PotSteamRise * Scale * Phase);
			AddQuad(Batch, Spot - FVector2f(Puff * Half), FVector2f(Puff), FxAtlas.UV(FMath::RoundToInt(L.FxSteam)), FxAtlas.CellUV, YJJUI::WithAlpha(FLinearColor::White, L.PotSteamAlpha * FMath::Sin(Phase * UE_PI)), false);
		}
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Ages)); ++Index)
		{
			if (Ages[Index] < 0.f || Ages[Index] >= T.PotSteamTime)
			{
				continue;
			}
			const float Burst = Ages[Index] / FMath::Max(T.PotSteamTime, KINDA_SMALL_NUMBER);
			const float Puff = T.PotSteamSize * BurstScales[Index] * Scale * FMath::Lerp(L.DustStartScale, L.DustEndScale, Burst);
			const FVector2f Spot = Rim - FVector2f(0.f, L.PotSteamRise * Scale * Burst);
			AddQuad(Batch, Spot - FVector2f(Puff * Half), FVector2f(Puff), FxAtlas.UV(FMath::RoundToInt(L.FxSteam)), FxAtlas.CellUV, YJJUI::WithAlpha(FLinearColor::White, 1.f - Burst), false);
		}
	}

	// 뚜껑: 평소엔 비스듬히 걸쳐 두고, 넘치기 직전엔 덜컹거리고, 열면 솟았다 떨어진다
	{
		float Lift = 0.f;
		float Tilt = PotLidAjarTilt;
		const float LidAge = Current->PotLidAge();
		if (LidAge >= 0.f && LidAge < L.PotLidTime)
		{
			const float LidT = LidAge / FMath::Max(L.PotLidTime, KINDA_SMALL_NUMBER);
			Lift = FMath::Sin(LidT * UE_PI) * L.PotLidLift * Scale;
			Tilt += LidT * FullTurn;
		}
		if (OverflowLeft >= 0.f)
		{
			const float Danger = 1.f - OverflowLeft / FMath::Max(T.PotOverflowTime, KINDA_SMALL_NUMBER);
			Lift += FMath::Abs(FMath::Sin(AnimClock * L.PotRattleSpeed)) * L.PotRattle * Scale * (1.f + Danger);
		}
		const FVector2f Ajar(SizePx * PotLidAjarX, -SizePx * PotLidAjarY);
		const FYJJQuadBatch Batch = BeginBatch(Canvas->Canvas, BuildingAtlas.Texture, 1);
		AddQuadRotated(Batch, TopLeft + FVector2f(SizePx * Half) + Ajar - FVector2f(0.f, Lift), FVector2f(SizePx), Tilt, BuildingAtlas.UV(FMath::RoundToInt(L.PotLidCell), 0), BuildingAtlas.CellUV, FLinearColor::White, false);
	}

	// 오른쪽 글자 칸: 단계 이름 (넘치기 직전엔 붉게 남은 초) → 뼈 그림 + 큰 숫자 / 다음 단계 → 단계 눈금 게이지
	const float TextX = X + Pad + L.PotUiCol + L.IconGap;
	const float TextW = X + W - Pad - TextX;
	const FString Name = OverflowLeft >= 0.f
		? YJJLoc::Format(TEXT("Pot.Full"), { YJJLoc::Get(Broth[Stage].NameKey), FMath::CeilToInt(OverflowLeft) })
		: Stage == INDEX_NONE ? YJJLoc::Get(TEXT("Pot.Raw")) : YJJLoc::Get(Broth[Stage].NameKey);
	// 열 수 있으면 이름 옆에 "뚜껑 열기 (Q)"
	const FString Line = bCan && OverflowLeft < 0.f ? YJJLoc::Format(TEXT("Pot.Ready"), { Name }) : Name;
	DrawLabel(Line, TextX, Y + Pad, L.SmallText, PotStatusColor(), 0.f, TextW);

	const float RowY = Y + Pad + L.SmallText + L.PotUiLineGap;
	const float RowMid = RowY + L.PotNumberText * Half;
	DrawIcon(FMath::RoundToInt(L.UiBone), TextX, RowMid - L.PotUiBoneIcon * Half, L.PotUiBoneIcon);
	const float NumX = TextX + L.PotUiBoneIcon + L.IconGap;
	const FString Count = FString::FromInt(FMath::FloorToInt(Current->PotBones()));
	float CountW = 0.f;
	MakeFont(Count, L.PotNumberText, 1.f, 0.f, CountW);
	const float NumSize = L.PotNumberText * (1.f + PotNumberPop * L.PotNumberPopScale);
	DrawLabelLeftMiddle(Count, NumX, RowMid, NumSize, FMath::Lerp(YJJUI::Paper, YJJUI::Highlight, PotNumberPop), 0.f);
	const float Next = Current->PotNextBones();
	if (Next > 0.f)
	{
		DrawLabelLeftMiddle(YJJLoc::Format(TEXT("Pot.Next"), { FMath::RoundToInt(Next) }), NumX + CountW + L.PotUiNextGap, RowMid, L.SmallText, YJJUI::Muted, 0.f);
	}
	// +뼈: 칸 오른쪽 끝에서 떠오르며 사라진다 (단계 이름과 겹치지 않게 조금만)
	for (const FYJJPotPop& Pop : PotPops)
	{
		const float PopT = Pop.Age / FMath::Max(L.PotPlusTime, KINDA_SMALL_NUMBER);
		DrawLabelLeftMiddle(YJJLoc::Format(TEXT("Pot.Plus"), { Pop.Value }), TextX + TextW - L.PotPlusW, RowMid - L.PotPlusRise * PopT, L.MidText, YJJUI::WithAlpha(YJJUI::Highlight, 1.f - PopT * PopT), 0.f);
	}

	const float MaxBones = FMath::Max(Broth.Last().Bones, KINDA_SMALL_NUMBER);
	const float BarY = Y + H - Pad - L.BarH;
	DrawBar(TextX, BarY, TextW, L.BarH, Current->PotBones() / MaxBones, Stage == INDEX_NONE ? YJJUI::Muted : Broth[Stage].Color);
	for (const FYJJBrothStage& Mark : Broth)
	{
		DrawBox(TextX + TextW * Mark.Bones / MaxBones - L.PotTickW * Half, BarY - L.PotTickOver, L.PotTickW, L.BarH + L.PotTickOver * 2.f, YJJUI::Ink);
	}

	return H;
}

void AYJJWaveHUD::TryHeroCharge()
{
	// 마우스가 전장 위면 그쪽으로, 아니면 영웅 앞(오른쪽)으로 내달린다
	FYJJBattle* Current = EditBattle();
	if (!Current || Current->IsChoosing())
	{
		return;
	}
	const FVector2f Mouse = ScreenToWorld(MouseScreen());
	const FVector2f Toward = IsMouseOverUI() ? Current->HeroPos() + FVector2f(UI().ChargeDefaultReach, 0.f) : Mouse;
	Current->HeroCharge(Toward);
}

int32 AYJJWaveHUD::SelectedSlot() const
{
	// 고른 종류 중 마지막 자리에서 가장 가까운 것 (재배치로 슬롯 번호가 바뀌어도 따라간다). 없으면 영웅
	const FYJJHordeView& Units = View();
	const int32 Wanted = PickedType != INDEX_NONE ? PickedType : Battle()->HeroDef().Unit;
	const FVector2f Near = PickedType != INDEX_NONE ? PickedPos : Battle()->HeroPos();
	int32 Best = INDEX_NONE;
	float BestSq = MAX_flt;
	for (int32 Slot = 0; Slot < Units.HighWater; ++Slot)
	{
		if (Units.Alive[Slot] && Units.Type[Slot] == Wanted)
		{
			const float DistSq = FVector2f::DistSquared(Units.Pos[Slot], Near);
			if (DistSq < BestSq)
			{
				BestSq = DistSq;
				Best = Slot;
			}
		}
	}
	return Best;
}

void AYJJWaveHUD::DrawUnitInfo()
{
	// 전장 아래 가운데 나무 판: [얼굴] 이름 / 체력 / 공격력 / 특성
	const FYJJBattle* Current = Battle();
	if (Current->bGameOver)
	{
		return;
	}
	int32 Slot = SelectedSlot();
	if (Slot == INDEX_NONE && PickedType != INDEX_NONE)
	{
		// 고른 유닛이 쓰러졌다: 영웅으로 돌아간다
		PickedType = INDEX_NONE;
		Slot = SelectedSlot();
	}
	if (Slot == INDEX_NONE)
	{
		return;
	}
	const FYJJHordeView& Units = View();
	PickedPos = PickedType != INDEX_NONE ? Units.Pos[Slot] : PickedPos;
	const FYJJUnitDef& Def = YJJData::Unit(Units.Type[Slot]);
	const FYJJUILayout& L = UI();
	const float Pad = L.PanelPad;
	const float Portrait = L.HeroBarPortrait;
	const float W = L.UnitInfoW;
	const float H = Portrait + Pad * 2.f;
	const float X = WorldCenterVirtualX() - W * Half;
	const float Y = VirtualHeight - H - L.HeroBarMargin;
	DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));
	// 얼굴: 영웅은 영웅 얼굴, 아군 병사는 병영 그림, 적은 해골
	int32 Icon = FMath::RoundToInt(L.UiSkull);
	if (Def.bHero)
	{
		Icon = Current->HeroDef().Portrait;
	}
	else if (Def.Team == EYJJTeam::Player)
	{
		const int32 Root = Def.PromoteRoot != INDEX_NONE ? Def.PromoteRoot : Units.Type[Slot];
		const int32 Building = YJJData::Buildings().IndexOfByPredicate([Root](const FYJJBuildingDef& B) { return B.Unit == Root; });
		Icon = Building != INDEX_NONE ? YJJData::Building(Building).IconCol : FMath::RoundToInt(L.UiSword);
	}
	DrawIcon(Icon, X + Pad, Y + Pad, Portrait);
	const float TX = X + Pad + Portrait + Pad;
	const float TW = X + W - Pad - TX;
	DrawLabel(YJJLoc::Get(Def.NameKey), TX, Y + Pad, L.SmallText, Def.Team == EYJJTeam::Player ? YJJUI::Highlight : YJJUI::BossText, 0.f, TW * Half);
	DrawLabel(YJJLoc::Format(TEXT("UI.UnitAtk"), { FormatCount(FMath::RoundToFloat(Units.Damage[Slot])) }), X + W - Pad, Y + Pad, L.SmallText, YJJUI::Paper, 1.f);
	const float BarY = Y + Pad + L.SmallText + Pad * Half;
	DrawBar(TX, BarY, TW, L.TextBarH, Units.HP[Slot] / FMath::Max(Units.MaxHP[Slot], KINDA_SMALL_NUMBER), Def.Team == EYJJTeam::Player ? YJJUI::HeroBar : YJJUI::BossBar);
	DrawBarLabel(YJJLoc::Format(TEXT("UI.UnitHP"), { FormatCount(FMath::CeilToFloat(Units.HP[Slot])), FormatCount(Units.MaxHP[Slot]) }), TX, BarY, TW, L.TextBarH);
	// 영웅: 스킬 셋 (AI 가 저절로 쓴다) 아이콘과 남은 쿨타임. 나머지: 싸우는 방식
	const float RowY = BarY + L.TextBarH + Pad * Half;
	if (Def.bHero)
	{
		const TArray<int32>& Skills = Current->HeroDef().Skills;
		const float SkillIcon = L.HeroSkillIcon;
		const float Step = TW / FMath::Max(Skills.Num(), 1);
		for (int32 Index = 0; Index < Skills.Num(); ++Index)
		{
			const FYJJSkillDef& Skill = YJJData::Skill(Skills[Index]);
			const float Ready = Current->HeroSkillReady(Index);
			const bool bReady = Ready >= 1.f;
			const float SX = TX + Index * Step;
			DrawIcon(Skill.Icon, SX, RowY, SkillIcon, bReady ? FLinearColor::White : YJJUI::WithAlpha(FLinearColor::White, L.HeroSkillDimAlpha));
			const FString Left = bReady ? YJJLoc::Get(TEXT("UI.SkillReady")) : YJJLoc::Format(TEXT("UI.SkillCooldown"), { FMath::CeilToInt((1.f - FMath::Max(Ready, 0.f)) * Skill.Cooldown) });
			DrawLabel(Left, SX + SkillIcon + Pad * Quarter, RowY + (SkillIcon - L.SmallText) * Half, L.SmallText, bReady ? YJJUI::Good : YJJUI::Muted, 0.f, Step - SkillIcon - Pad * Half);
		}
		return;
	}
	TArray<FString> Traits;
	{
		Traits.Add(YJJLoc::Get(Def.ProjectileSpeed > 0.f ? TEXT("Trait.Ranged") : TEXT("Trait.Melee")));
		if (Def.Splash > 0.f)
		{
			Traits.Add(YJJLoc::Get(TEXT("Trait.Splash")));
		}
		if (Def.bBoss)
		{
			Traits.Add(YJJLoc::Get(TEXT("Trait.Boss")));
		}
		if (Def.bCamp)
		{
			Traits.Add(YJJLoc::Get(TEXT("Trait.Camp")));
		}
	}
	DrawLabel(FString::Join(Traits, TEXT(" · ")), TX, RowY, L.SmallText, YJJUI::Muted, 0.f, TW);
}

FLinearColor AYJJWaveHUD::PotStatusColor() const
{
	const FYJJBattle* Current = Battle();
	return Current->PotStage() == INDEX_NONE ? YJJUI::Paper : Current->PotOverflowLeft() >= 0.f ? YJJUI::Bad : YJJUI::Highlight;
}

void AYJJWaveHUD::DrawSpellTooltip()
{
	if (!HoverButton(EButton::Spell) || Battle()->bGameOver)
	{
		return;
	}
	const FYJJButton* Button = ButtonAt(ToVirtual(MouseScreen()));
	if (!Button || Button->Index < 0 || Button->Index >= static_cast<int32>(EYJJSpell::Count))
	{
		return;
	}
	const EYJJSpell Spell = static_cast<EYJJSpell>(Button->Index);
	const FString Key = YJJData::SpellKey(Spell);
	const FYJJUILayout& L = UI();
	const float W = L.TipW;
	const float Pad = L.TextPad;
	// 이름 → 무엇을 하는지 → Lv 수치
	TArray<TPair<FString, FLinearColor>> Lines;
	Lines.Emplace(YJJLoc::Get(TEXT("Spell.") + Key), YJJUI::Highlight);
	for (const FString& Wrapped : WrapText(YJJLoc::Get(TEXT("SpellDesc.") + Key), L.SmallText, W - Pad * 2.f))
	{
		Lines.Emplace(Wrapped, YJJUI::Paper);
	}
	TArray<FString> Stats;
	SpellText(Spell).ParseIntoArray(Stats, TEXT("\n"));
	for (const FString& Stat : Stats)
	{
		for (const FString& Wrapped : WrapText(Stat, L.SmallText, W - Pad * 2.f))
		{
			Lines.Emplace(Wrapped, YJJUI::Muted);
		}
	}
	const FVector2f Mouse = ToVirtual(MouseScreen());
	const float LineH = L.TipLineH;
	const float H = Pad * 2.f + LineH * Lines.Num();
	const float X = FMath::Min(Mouse.X + L.TipOffset, VirtualW - W);
	const float Y = FMath::Clamp(Mouse.Y - H - L.TipOffset, 0.f, VirtualHeight - H);
	DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));
	for (int32 Line = 0; Line < Lines.Num(); ++Line)
	{
		DrawValueLabel(Lines[Line].Key, X + Pad, Y + Pad + LineH * Line, L.SmallText, Lines[Line].Value, W - Pad * 2.f);
	}
}

void AYJJWaveHUD::DrawPotTooltip()
{
	const FYJJBattle* Current = Battle();
	const TConstArrayView<FYJJBrothStage> Broth = YJJData::Broth();
	if (!HoverButton(EButton::Pot) || Current->bGameOver || Broth.Num() == 0)
	{
		return;
	}
	// 무엇을 하면 무엇이 되는지 한눈에: 뼈가 모인다 → 단계표 (지금 단계 밝게) → 열면 → 넘침 주의
	const FYJJUILayout& L = UI();
	const float W = L.TipW;
	const float Pad = L.TextPad;
	const int32 Stage = Current->PotStage();
	TArray<TPair<FString, FLinearColor>> Lines;
	Lines.Emplace(YJJLoc::Get(TEXT("Pot.Title")), YJJUI::Highlight);
	for (const TCHAR* Key : { TEXT("Pot.Desc1"), TEXT("Pot.Desc2") })
	{
		for (const FString& Wrapped : WrapText(YJJLoc::Get(Key), L.SmallText, W - Pad * 2.f))
		{
			Lines.Emplace(Wrapped, YJJUI::Paper);
		}
	}
	for (int32 Index = 0; Index < Broth.Num(); ++Index)
	{
		const FYJJBrothStage& Row = Broth[Index];
		const FString Text = YJJLoc::Format(TEXT("Pot.Row"), { YJJLoc::Get(Row.NameKey), FMath::RoundToInt(Row.Bones), FMath::RoundToInt(Row.HP * Percent), FMath::RoundToInt(Row.Heal * Percent) });
		Lines.Emplace(Text, Index == Stage ? YJJUI::Highlight : YJJUI::Muted);
	}
	for (const FString& Wrapped : WrapText(YJJLoc::Format(TEXT("Pot.Warn"), { FMath::RoundToInt(YJJData::Tune().PotOverflowTime) }), L.SmallText, W - Pad * 2.f))
	{
		Lines.Emplace(Wrapped, YJJUI::Bad);
	}
	if (Current->BrothServed() > 0)
	{
		Lines.Emplace(YJJLoc::Format(TEXT("Pot.Served"), { Current->BrothServed(), FMath::RoundToInt(Current->BrothHP() * Percent), FMath::RoundToInt(Current->BrothDamage() * Percent) }), YJJUI::Good);
	}

	// 솥 판 위쪽에 띄운다 (마우스가 아래쪽이라 아래로는 자리가 없다)
	const FVector2f Mouse = ToVirtual(MouseScreen());
	const float LineH = L.TipLineH;
	const float H = Pad * 2.f + LineH * Lines.Num();
	const float X = FMath::Min(Mouse.X + L.TipOffset, VirtualW - W);
	const float Y = FMath::Clamp(Mouse.Y - H - L.TipOffset, 0.f, VirtualHeight - H);
	DrawPanel(X, Y, W, H, YJJUI::PanelTint, FMath::RoundToInt(L.UiPanelDark));
	for (int32 Line = 0; Line < Lines.Num(); ++Line)
	{
		DrawValueLabel(Lines[Line].Key, X + Pad, Y + Pad + LineH * Line, L.SmallText, Lines[Line].Value, W - Pad * 2.f);
	}
}
