"""대사 목소리 (AI 성우): 컷씬·말풍선 대사를 신경망 TTS(Microsoft Edge 한국어 음성)로 읽힌다.

  Tools/Art/.venv/Scripts/python.exe Tools/Audio/generate_voice.py
    → Tools/Audio/Generated/VO_Line_<이름>.wav + Sounds.csv Line_<이름> + 대사 시간을 목소리 길이에 맞춤
  그다음 Tools/Audio/BuildAudio.ps1 로 언리얼 에셋 임포트

읽을 줄: Cutscenes.csv·Dialogues.csv 에서 Voice 열이 Line_ 으로 시작하는 줄 (글자는 Localization.csv 한국어)
TTS: edge-tts (pip install edge-tts, 인터넷 필요 — 대사 글이 Microsoft 음성 서버로 간다). 가공: ffmpeg (앞뒤 빈 소리 자르기, 음량 맞추기만)
"""
import asyncio
import csv
import io
import os
import subprocess
import sys
import tempfile
import wave

import edge_tts

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DATA = os.path.join(ROOT, "Content", "YJJWave", "Data")
OUT = os.path.join(HERE, "Generated")
RATE = 44100
SILENCE = "-45dB"         # 앞뒤 빈 소리로 볼 크기
LINE_PAD = 0.5            # 목소리 뒤 대사 줄이 더 머무는 시간 (초)
MIN_LINE = 1.6            # 대사 줄 최소 시간
MIN_TITLE = 4.0           # 장 제목 카드 최소 시간
TITLE_SPEAKER = "Title"

# 화자별 목소리: 음성 이름, 빠르기, 음 높이 (edge-tts 형식)
MALE = "ko-KR-InJoonNeural"
YOUNG = "ko-KR-HyunsuMultilingualNeural"
VOICES = {
    "Title":     (MALE, "-12%", "-6Hz"),    # 해설: 느리고 낮게
    "Hero":      (MALE, "+0%", "-2Hz"),
    "Grunt":     (YOUNG, "+6%", "+4Hz"),
    "Veteran":   (YOUNG, "-2%", "-6Hz"),
    "General":   (MALE, "-6%", "-10Hz"),
    "GruntHero": (MALE, "-4%", "-8Hz"),
    "Necro":     (MALE, "-14%", "-12Hz"),
    "BoneKing":  (MALE, "-16%", "-20Hz"),
}
DEFAULT_VOICE = VOICES["Hero"]
SOUND_ROW = dict(volume=0.9, category="Voice", cooldown=0, pitch_min=1, pitch_max=1)


def read(path):
    with io.open(path, encoding="utf-8-sig", newline="") as f:
        return list(csv.DictReader(f))


def spoken(text):
    # 화면용 기호를 읽기 좋게: 제목 줄바꿈(\n)·가운뎃점은 쉼
    return text.replace("\\n", ". ").replace("·", ", ")


def lines():
    ko = {r["Key"]: r["ko"] for r in read(os.path.join(DATA, "Localization.csv"))}
    out = []
    for table in ("Cutscenes.csv", "Dialogues.csv"):
        for r in read(os.path.join(DATA, table)):
            voice = r.get("Voice", "")
            if voice.startswith("Line_") and r["TextKey"] in ko:
                out.append((voice, r["Speaker"], spoken(ko[r["TextKey"]])))
    return out


async def speak(text, voice, path):
    name, rate, pitch = voice
    await edge_tts.Communicate(text, name, rate=rate, pitch=pitch).save(path)


def process(src, dst):
    trim = "silenceremove=start_periods=1:start_threshold=%s" % SILENCE
    chain = [trim, "areverse", trim, "areverse", "loudnorm=I=-15:TP=-1.5:LRA=9", "aresample=%d" % RATE]
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", src, "-af", ",".join(chain),
                    "-ac", "1", "-c:a", "pcm_s16le", dst], check=True)


def add_sound_rows(names):
    path = os.path.join(DATA, "Sounds.csv")
    with io.open(path, encoding="utf-8-sig", newline="") as f:
        rows = list(csv.reader(f))
    have = {r[0] for r in rows if r}
    for name in names:
        if name not in have:
            rows.append([name, "VO_" + name, SOUND_ROW["volume"], SOUND_ROW["category"], SOUND_ROW["cooldown"],
                         SOUND_ROW["pitch_min"], SOUND_ROW["pitch_max"], "대사 목소리 (generate_voice.py)"])
    out = io.StringIO()
    csv.writer(out, lineterminator="\n").writerows(rows)
    with io.open(path, "w", encoding="utf-8-sig", newline="") as f:
        f.write(out.getvalue())


def length(path):
    with wave.open(path) as w:
        return w.getnframes() / float(w.getframerate())


def fit_durations(lengths):
    """대사 줄 시간 = 목소리 길이 + 여유 (장 제목 카드는 최소 MIN_TITLE)"""
    for table in ("Cutscenes.csv", "Dialogues.csv"):
        path = os.path.join(DATA, table)
        with io.open(path, encoding="utf-8-sig", newline="") as f:
            rows = list(csv.reader(f))
        head = rows[0]
        voice, duration, speaker = head.index("Voice"), head.index("Duration"), head.index("Speaker")
        for r in rows[1:]:
            if r[voice] in lengths:
                floor = MIN_TITLE if r[speaker] == TITLE_SPEAKER else MIN_LINE
                r[duration] = "%g" % max(floor, round(lengths[r[voice]] + LINE_PAD, 1))
        out = io.StringIO()
        csv.writer(out, lineterminator="\n").writerows(rows)
        with io.open(path, "w", encoding="utf-8-sig", newline="") as f:
            f.write(out.getvalue())


def main():
    os.makedirs(OUT, exist_ok=True)
    todo = lines()
    lengths = {}
    with tempfile.TemporaryDirectory() as tmp:
        for name, speaker, text in todo:
            raw = os.path.join(tmp, name + ".mp3")
            asyncio.run(speak(text, VOICES.get(speaker, DEFAULT_VOICE), raw))
            dst = os.path.join(OUT, "VO_" + name + ".wav")
            process(raw, dst)
            lengths[name] = length(dst)
            print("voice %s %s %.1fs" % (name, speaker, lengths[name]))
    add_sound_rows([name for name, _, _ in todo])
    fit_durations(lengths)
    print("lines", len(todo))


if __name__ == "__main__":
    sys.exit(main())
