#pragma once

#include "CoreMinimal.h"

/** 화면 색 (아트 팔레트 Tools/Art/palette.py 와 맞춘다) */
namespace YJJUI
{
	inline const FLinearColor Ink = FLinearColor(0.027f, 0.012f, 0.008f, 1.f);          // 먹선 (진한 고동)
	inline const FLinearColor Paper = FLinearColor(1.f, 0.955f, 0.86f, 1.f);            // 글자 기본 (따뜻한 흰색)
	inline const FLinearColor Gold = FLinearColor(1.f, 0.72f, 0.1f, 1.f);
	inline const FLinearColor Soul = FLinearColor(0.35f, 0.95f, 0.75f, 1.f);
	inline const FLinearColor Ally = FLinearColor(0.3f, 0.62f, 1.f, 1.f);
	inline const FLinearColor Enemy = FLinearColor(1.f, 0.25f, 0.18f, 1.f);
	inline const FLinearColor Good = FLinearColor(0.45f, 0.95f, 0.3f, 1.f);
	inline const FLinearColor Bad = FLinearColor(1.f, 0.3f, 0.25f, 1.f);
	inline const FLinearColor Muted = FLinearColor(0.55f, 0.5f, 0.45f, 1.f);
	inline const FLinearColor PanelTint = FLinearColor(1.f, 1.f, 1.f, 1.f);
	inline const FLinearColor Shade = FLinearColor(0.f, 0.f, 0.f, 0.55f);
	inline const FLinearColor BarBack = FLinearColor(0.06f, 0.03f, 0.02f, 0.85f);
	// 게이지 색은 화면에 보이는 색(sRGB)으로 적는다. 선형 값으로 적으면 화면에서 흐린 파스텔이 된다
	inline const FLinearColor LevelBar = FLinearColor(FColor(255, 190, 40));          // 남은 시간 글자와 같은 노랑
	inline const FLinearColor HeroBar = FLinearColor(FColor(46, 146, 52));            // 진한 초록: 흰 숫자가 잘 보이게
	inline const FLinearColor BossBar = FLinearColor(FColor(190, 30, 52));
	inline const FLinearColor BossText = FLinearColor(FColor(255, 96, 110));          // 보스 이름: 게이지와 같은 붉은 계열
	inline const FLinearColor Highlight = FLinearColor(1.f, 0.9f, 0.35f, 1.f);
	inline const FLinearColor HitTint = FLinearColor(1.f, 0.35f, 0.3f, 1.f);
	inline const FLinearColor Field = FLinearColor(0.09f, 0.16f, 0.05f, 1.f);           // 전장 밖 바탕
	inline const FLinearColor PerkLocked = FLinearColor(0.35f, 0.33f, 0.4f, 1.f);
	inline const FLinearColor PerkReach = FLinearColor(0.75f, 0.9f, 1.f, 1.f);
	inline const FLinearColor PerkKeystone = FLinearColor(0.85f, 0.45f, 1.f, 1.f);
	inline const FLinearColor PerkTaken = FLinearColor(1.f, 0.78f, 0.25f, 1.f);
	inline const FLinearColor LinkIdle = FLinearColor(0.25f, 0.23f, 0.3f, 1.f);
	inline const FLinearColor LinkTaken = FLinearColor(1.f, 0.7f, 0.2f, 1.f);
	inline const FLinearColor TreeBack = FLinearColor(0.02f, 0.02f, 0.05f, 0.92f);

	inline FLinearColor WithAlpha(const FLinearColor& Color, float Alpha) { return FLinearColor(Color.R, Color.G, Color.B, Color.A * Alpha); }
}
