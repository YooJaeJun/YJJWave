"""효과음 팩 (Kenney CC0, Tools/Audio/Packs/Kenney) 에서 고른 소리로 합성 효과음을 덮어쓴다.

  python Tools/Audio/apply_sfx_pack.py   (BuildAudio.ps1 이 generate_audio.py 다음에 부른다)

ogg → wav (44.1kHz 모노) + 음량 맞추기. 표에 없는 소리(배경음·유성 낙하 등)는 합성한 그대로 둔다.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PACK = os.path.join(HERE, "Packs", "Kenney")
OUT = os.path.join(HERE, "Generated")
RATE = 44100
LOUDNESS = "loudnorm=I=-14:TP=-1:LRA=7"   # 예전 합성음보다 또렷하고 크게

# 게임 소리 이름 → 팩 파일
SOUNDS = {
    "SFX_Hit1": "impactPunch_medium_000",
    "SFX_Hit2": "impactPunch_medium_001",
    "SFX_Hit3": "impactPunch_medium_002",
    "SFX_Arrow": "knifeSlice",
    "SFX_Death": "impactSoft_medium_000",
    "SFX_MeteorBoom": "impactPunch_heavy_000",
    "SFX_Rally": "confirmation_004",
    "SFX_Build": "impactWood_medium_000",
    "SFX_Upgrade": "confirmation_002",
    "SFX_Buy": "handleCoins",
    "SFX_Card": "bookFlip1",
    "SFX_Banner": "maximize_006",
    "SFX_BossAppear": "impactBell_heavy_000",
    "SFX_Victory": "confirmation_003",
    "SFX_Defeat": "error_008",
    "SFX_Click": "click_002",
    "SFX_Rescue": "pluck_001",
    "SFX_Raise": "creak1",
    "SFX_Star": "select_006",
    "SFX_PotOpen": "metalPot1",
    "SFX_PotOverflow": "metalPot3",
    "SFX_Smack": "impactMetal_heavy_001",
    "SFX_Swing": "drawKnife2",
}


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, source in SOUNDS.items():
        src = os.path.join(PACK, source + ".ogg")
        dst = os.path.join(OUT, name + ".wav")
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", src, "-af", LOUDNESS + ",aresample=%d" % RATE,
                        "-ac", "1", "-c:a", "pcm_s16le", dst], check=True)
        print("sfx", name, "<-", source)


if __name__ == "__main__":
    sys.exit(main())
