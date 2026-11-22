"""YJJWave 소리를 파형부터 직접 합성한다 (외부 음원 없음 → 상업적 사용 자유, 저작권 걱정 없음).

  BGM_*    배경음 (반복)          SFX_*    효과음          VO_*    캐릭터 보이스 (웅얼웅얼 음절)
실행: Tools/Audio/BuildAudio.ps1  (WAV 생성 → 언리얼 에셋 임포트)
"""
import math
import os
import random
import struct
import sys
import wave

RATE = 22050
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "Generated")
TAU = math.tau


# ------------------------------------------------------------------------------------------------
# 기본 도구

def silence(seconds):
    return [0.0] * int(seconds * RATE)


def osc(kind, phase):
    p = phase % 1.0
    if kind == "sine":
        return math.sin(TAU * p)
    if kind == "square":
        return 1.0 if p < 0.5 else -1.0
    if kind == "pulse":
        return 1.0 if p < 0.25 else -1.0
    if kind == "saw":
        return 2.0 * p - 1.0
    return 4.0 * abs(p - 0.5) - 1.0   # triangle


def tone(freq, seconds, kind="square", volume=0.5, attack=0.005, release=0.06, end_freq=None, vibrato=0.0):
    n = int(seconds * RATE)
    out = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / RATE
        u = i / max(n - 1, 1)
        f = freq if end_freq is None else freq * (end_freq / freq) ** u
        if vibrato:
            f *= 1.0 + vibrato * math.sin(TAU * 6.0 * t)
        phase += f / RATE
        env = min(t / attack, 1.0) * min(max(seconds - t, 0.0) / release, 1.0)
        out[i] = osc(kind, phase) * volume * env
    return out


def noise(seconds, volume=0.5, decay=8.0, lowpass=0.0, seed=1):
    rng = random.Random(seed)
    n = int(seconds * RATE)
    out = [0.0] * n
    last = 0.0
    for i in range(n):
        s = rng.uniform(-1.0, 1.0)
        if lowpass:
            last += (s - last) * lowpass
            s = last
        out[i] = s * volume * math.exp(-decay * i / RATE)
    return out


def mix(*tracks):
    n = max(len(t) for t in tracks)
    out = [0.0] * n
    for t in tracks:
        for i, s in enumerate(t):
            out[i] += s
    return out


def place(dest, track, seconds, gain=1.0):
    start = int(seconds * RATE)
    for i, s in enumerate(track):
        j = start + i
        if j >= len(dest):
            break
        dest[j] += s * gain


def seq(*tracks):
    out = []
    for t in tracks:
        out.extend(t)
    return out


def save(name, samples, peak=0.85):
    top = max(1e-6, max(abs(s) for s in samples))
    scale = min(peak / top, 1.0) if top > peak else 1.0
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * scale)) * 32767)) for s in samples))
    print("wrote", name, "%.1fs" % (len(samples) / RATE))


def note(name):
    """'C4' → 주파수"""
    names = {"C": -9, "D": -7, "E": -5, "F": -4, "G": -2, "A": 0, "B": 2}
    semis = names[name[0]] + (1 if "#" in name else 0)
    octave = int(name[-1])
    return 440.0 * 2 ** ((semis + (octave - 4) * 12) / 12)


# ------------------------------------------------------------------------------------------------
# 드럼

def kick():
    return mix(tone(110, 0.18, "sine", 0.9, end_freq=40, release=0.12), noise(0.02, 0.3, 60, 0.3, 3))


def snare(seed=5):
    return mix(noise(0.16, 0.5, 18, 0.6, seed), tone(190, 0.08, "triangle", 0.25, end_freq=150))


def hat(seed=9):
    return noise(0.04, 0.18, 70, 0.0, seed)


# ------------------------------------------------------------------------------------------------
# 배경음

def song(bpm, bars, chords, melody, lead_kind, bass_kind, swing_drums, seed):
    beat = 60.0 / bpm
    length = bars * 4 * beat
    out = silence(length)
    rng = random.Random(seed)
    for bar in range(bars):
        root, kind = chords[bar % len(chords)]
        base = note(root)
        third = base * (2 ** ((3 if kind == "m" else 4) / 12))
        fifth = base * 2 ** (7 / 12)
        t0 = bar * 4 * beat
        # 베이스: 8분음표로 근음-5도
        for k in range(8):
            f = base / 2 if k % 2 == 0 else fifth / 2
            place(out, tone(f, beat * 0.45, bass_kind, 0.32, release=0.05), t0 + k * beat * 0.5)
        # 화음: 한 박마다 짧게 (행진 느낌)
        for k in range(4):
            for f in (base, third, fifth):
                place(out, tone(f, beat * 0.25, "triangle", 0.08, release=0.06), t0 + k * beat + beat * 0.5)
        # 드럼
        for k in range(4):
            place(out, kick(), t0 + k * beat, 0.9 if k % 2 == 0 else 0.6)
            if k % 2 == 1:
                place(out, snare(rng.randint(1, 99)), t0 + k * beat, 0.8)
            for h in range(2):
                place(out, hat(rng.randint(1, 99)), t0 + k * beat + h * beat * 0.5, 1.0)
            if swing_drums and k == 3:
                place(out, snare(rng.randint(1, 99)), t0 + k * beat + beat * 0.75, 0.5)
        # 멜로디
        for k, n in enumerate(melody[bar % len(melody)]):
            if n:
                place(out, tone(note(n), beat * 0.42, lead_kind, 0.2, vibrato=0.006, release=0.08), t0 + k * beat * 0.5)
    return out


def bgm_battle():
    chords = [("C3", ""), ("A2", "m"), ("F2", ""), ("G2", "")]
    melody = [
        ["E5", None, "G5", "E5", "C5", None, "D5", "E5"],
        ["C5", None, "E5", "C5", "A4", None, "B4", "C5"],
        ["A4", "C5", "F5", None, "E5", "D5", "C5", None],
        ["D5", None, "G5", "F5", "E5", "D5", "B4", None],
    ]
    return song(132, 16, chords, melody, "pulse", "square", False, 11)


def bgm_boss():
    chords = [("A2", "m"), ("F2", ""), ("D3", "m"), ("E3", "")]
    melody = [
        ["A4", "A4", "C5", "A4", "E5", None, "D5", "C5"],
        ["F4", "A4", "C5", None, "D5", "C5", "A4", None],
        ["D5", "F5", "A5", None, "G5", "F5", "E5", "D5"],
        ["E5", None, "G#5", "E5", "B4", None, "E5", None],
    ]
    return song(150, 16, chords, melody, "saw", "saw", True, 23)


# ------------------------------------------------------------------------------------------------
# 효과음

def meteor_fall():
    """떨어지는 운석: 높은 휘파람이 내려가며 바람 소리가 커진다 (Tuning MeteorFallTime 0.6초와 같은 길이)"""
    whistle = tone(1500, 0.6, "sine", 0.22, end_freq=380, release=0.05)
    n = len(whistle)
    wind = noise(0.6, 0.35, 0.5, 0.2, 7)
    wind = [s * (i / max(n - 1, 1)) ** 2 for i, s in enumerate(wind[:n])]
    return mix(whistle, wind)


def explosion():
    """운석 폭발: 쿵 (낮은 사인) + 퍼지는 잡음 + 튀는 파편 소리"""
    thump = tone(90, 0.55, "sine", 0.95, end_freq=28, attack=0.002, release=0.35)
    body = noise(1.1, 0.85, 3.5, 0.06, 8)
    crackle = []
    rng = random.Random(21)
    for k in range(14):
        crackle = mix(crackle, silence(rng.uniform(0.05, 0.6)) + noise(0.03, 0.35, 60, 0.6, 30 + k))
    return mix(thump, body, crackle)


def crowd_shout():
    """돌격 함성: 졸병 스무 명이 저마다 다른 높이로 "와아아!" (조금씩 늦게 시작)"""
    rng = random.Random(42)
    out = []
    for k in range(22):
        voice = silence(rng.uniform(0.0, 0.18)) + syllable(rng.uniform(170, 330), rng.choice("aao"), rng.uniform(0.55, 0.85), rise=rng.uniform(1.05, 1.25), breath=0.25, seed=100 + k)
        out = mix(out, [s * 0.55 for s in voice])
    return out


def sfx():
    return {
        "SFX_Hit1": mix(noise(0.09, 0.6, 30, 0.35, 1), tone(160, 0.07, "sine", 0.5, end_freq=80)),
        "SFX_Hit2": mix(noise(0.08, 0.6, 35, 0.4, 2), tone(200, 0.06, "sine", 0.45, end_freq=90)),
        "SFX_Hit3": mix(noise(0.1, 0.55, 28, 0.3, 4), tone(140, 0.08, "sine", 0.5, end_freq=70)),
        "SFX_Arrow": mix(noise(0.18, 0.35, 12, 0.08, 6), tone(1400, 0.12, "sine", 0.06, end_freq=700)),
        "SFX_Death": seq(tone(520, 0.05, "square", 0.3, end_freq=900), tone(300, 0.1, "triangle", 0.3, end_freq=120)),
        "SFX_MeteorFall": meteor_fall(),
        "SFX_MeteorBoom": explosion(),
        "SFX_Rally": crowd_shout(),
        "SFX_Build": seq(mix(noise(0.05, 0.6, 40, 0.5, 10), tone(900, 0.06, "square", 0.25)), silence(0.06),
                         mix(noise(0.05, 0.6, 40, 0.5, 11), tone(1100, 0.08, "square", 0.25))),
        "SFX_Upgrade": seq(*[tone(note(n), 0.07, "pulse", 0.3) for n in ("C5", "E5", "G5", "C6")]),
        "SFX_Buy": seq(tone(note("E6"), 0.05, "square", 0.25), tone(note("B6"), 0.12, "square", 0.25, release=0.1)),
        "SFX_Card": seq(noise(0.08, 0.3, 20, 0.2, 12), tone(note("A5"), 0.08, "triangle", 0.3), tone(note("E6"), 0.16, "triangle", 0.3)),
        "SFX_Banner": seq(*[tone(note(n), 0.09, "pulse", 0.32) for n in ("G4", "C5", "E5")], tone(note("G5"), 0.3, "pulse", 0.35, release=0.2)),
        "SFX_BossAppear": mix(tone(55, 1.4, "saw", 0.6, vibrato=0.02, release=0.6), tone(82, 1.4, "saw", 0.35, release=0.6), noise(1.2, 0.25, 2, 0.04, 13)),
        "SFX_Victory": seq(*[tone(note(n), 0.14, "pulse", 0.35) for n in ("C5", "E5", "G5", "C6", "G5")], tone(note("C6"), 0.6, "pulse", 0.4, vibrato=0.01, release=0.4)),
        "SFX_Defeat": seq(*[tone(note(n), 0.25, "triangle", 0.4) for n in ("G4", "F4", "E4")], tone(note("C4"), 0.8, "triangle", 0.45, release=0.6)),
        "SFX_Click": tone(1200, 0.04, "square", 0.2, end_freq=800, release=0.03),
        "SFX_Rescue": seq(*[tone(note(n), 0.05, "sine", 0.25) for n in ("C6", "E6", "G6")]),
        "SFX_Raise": mix(tone(220, 0.7, "saw", 0.2, end_freq=110, vibrato=0.04), noise(0.7, 0.2, 3, 0.05, 14)),
    }


# ------------------------------------------------------------------------------------------------
# 보이스: 모음 두 개의 공명(포먼트)을 거친 톱니파 = 웅얼거리는 목소리. 캐릭터마다 높이·모음·말투가 다르다

VOWELS = {"a": (800, 1200), "e": (500, 1900), "i": (300, 2300), "o": (500, 900), "u": (350, 800)}


def resonate(src, freq, q):
    """2차 공명 필터 (밴드패스)"""
    w = TAU * freq / RATE
    alpha = math.sin(w) / (2 * q)
    b0, b2 = alpha, -alpha
    a0, a1, a2 = 1 + alpha, -2 * math.cos(w), 1 - alpha
    x1 = x2 = y1 = y2 = 0.0
    out = [0.0] * len(src)
    for i, x in enumerate(src):
        y = (b0 * x + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1 = x1, x
        y2, y1 = y1, y
        out[i] = y
    return out


def syllable(pitch, vowel, seconds, rise=1.0, breath=0.0, seed=1):
    n = int(seconds * RATE)
    src = [0.0] * n
    phase = 0.0
    rng = random.Random(seed)
    for i in range(n):
        u = i / max(n - 1, 1)
        f = pitch * (1 + (rise - 1) * u)
        phase += f / RATE
        src[i] = osc("saw", phase) + (rng.uniform(-1, 1) * breath)
    f1, f2 = VOWELS[vowel]
    voiced = mix(resonate(src, f1, 4.0), [s * 0.6 for s in resonate(src, f2, 6.0)])
    attack, release = 0.015, min(0.06, seconds * 0.4)
    return [s * min(i / RATE / attack, 1.0) * min((seconds - i / RATE) / release, 1.0) * 0.5 for i, s in enumerate(voiced)]


def babble(pitch, vowels, length=0.09, gap=0.025, rise=1.0, breath=0.0, seed=1):
    out = []
    for k, v in enumerate(vowels):
        out += syllable(pitch * (1 + 0.08 * math.sin(k * 2.1 + seed)), v, length, rise if k == len(vowels) - 1 else 1.0, breath, seed + k)
        out += silence(gap)
    return out


def voices():
    v = {}
    # 졸병: 높고 짧은 '엇! 엣!'
    v["VO_Grunt_0"] = babble(330, "ea", 0.07, rise=1.3, seed=1)
    v["VO_Grunt_1"] = babble(350, "o", 0.1, rise=1.4, seed=2)
    v["VO_Grunt_2"] = babble(310, "aie", 0.06, seed=3)
    v["VO_Archer_0"] = babble(280, "ia", 0.08, rise=1.2, seed=4)
    v["VO_Archer_1"] = babble(300, "e", 0.11, rise=1.3, seed=5)
    v["VO_Shield_0"] = babble(170, "oa", 0.1, seed=6)
    v["VO_Shield_1"] = babble(160, "u", 0.14, rise=0.9, seed=7)
    v["VO_Brute_0"] = babble(110, "ao", 0.13, rise=0.85, breath=0.2, seed=8)
    v["VO_Brute_1"] = babble(100, "u", 0.18, rise=1.1, breath=0.25, seed=9)
    # 영웅: '가자!' 같은 두 음절, 끝을 올린다
    v["VO_Hero_0"] = babble(200, "aa", 0.12, gap=0.03, rise=1.35, seed=10)
    v["VO_Hero_1"] = babble(190, "oa", 0.13, gap=0.03, rise=1.3, seed=11)
    v["VO_Hero_2"] = babble(210, "eia", 0.1, gap=0.02, rise=1.25, seed=12)
    # 적
    v["VO_Skeleton_0"] = mix(babble(240, "a", 0.08, breath=0.5, seed=13), noise(0.12, 0.25, 15, 0.6, 30))
    v["VO_Imp_0"] = babble(520, "iii", 0.05, gap=0.015, rise=1.2, seed=14)
    v["VO_Fatty_0"] = babble(90, "ou", 0.2, rise=0.8, breath=0.3, seed=15)
    v["VO_Necro_0"] = babble(150, "iu", 0.18, breath=0.8, seed=16)
    v["VO_BoneKing_0"] = mix(babble(70, "oa", 0.3, rise=0.8, breath=0.5, seed=17), tone(55, 0.7, "saw", 0.25, release=0.3))
    # 컷씬 해설: 낮고 차분한 웅얼거림
    v["VO_Narrator_0"] = babble(140, "aeo", 0.1, gap=0.04, seed=18)
    return v


def main():
    os.makedirs(OUT, exist_ok=True)
    save("BGM_Battle", bgm_battle(), peak=0.7)
    save("BGM_Boss", bgm_boss(), peak=0.7)
    for name, samples in list(sfx().items()) + list(voices().items()):
        save(name, samples)


if __name__ == "__main__":
    main()
