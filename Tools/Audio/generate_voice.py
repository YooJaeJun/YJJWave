"""대사 목소리 (AI 성우): 컷씬·말풍선 대사를 TTS로 읽히고 화자마다 웅장하게 가공한다.

  python Tools/Audio/generate_voice.py            → Tools/Audio/Generated/VO_Line_<이름>.wav + Sounds.csv Line_<이름>
  그다음 Tools/Audio/BuildAudio.ps1 로 언리얼 에셋 임포트

읽을 줄: Cutscenes.csv·Dialogues.csv 에서 Voice 열이 Line_ 으로 시작하는 줄 (글자는 Localization.csv 한국어)
TTS: Windows 내장 한국어 음성 (System.Speech, 오프라인). 가공: ffmpeg (음 낮추기·저음·울림·음량 맞추기)
"""
import csv
import io
import json
import os
import subprocess
import sys
import tempfile
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DATA = os.path.join(ROOT, "Content", "YJJWave", "Data")
OUT = os.path.join(HERE, "Generated")
RATE = 44100
VOICE_NAME = "Microsoft Heami Desktop"
SPEAK_RATE = 0            # SAPI 말 빠르기 (-10~10)
SILENCE = "-45dB"         # 앞뒤 빈 소리로 볼 크기
LINE_PAD = 0.4            # 목소리 뒤 대사 줄이 더 머무는 시간 (초)

# 화자별 가공: pitch 음 높이 배율, tempo 말 빠르기, bass 저음 dB, echo (지연 ms, 크기) 목록, extra 덧붙이는 필터
PROFILES = {
    "Title":     dict(pitch=0.74, tempo=0.88, bass=7, echo=[(90, 0.35), (180, 0.22), (320, 0.12)]),
    "Hero":      dict(pitch=0.80, tempo=0.95, bass=6, echo=[(60, 0.25), (140, 0.12)]),
    "Grunt":     dict(pitch=1.04, tempo=1.05, bass=2, echo=[(40, 0.12)]),
    "Veteran":   dict(pitch=0.86, tempo=0.95, bass=4, echo=[(50, 0.18)]),
    "General":   dict(pitch=0.70, tempo=0.9, bass=8, echo=[(80, 0.3), (170, 0.15)]),
    "GruntHero": dict(pitch=0.74, tempo=0.92, bass=8, echo=[(110, 0.35), (230, 0.22), (400, 0.12)]),
    "Necro":     dict(pitch=0.82, tempo=0.85, bass=3, echo=[(70, 0.3), (150, 0.2)], extra="vibrato=f=6:d=0.25"),
    "BoneKing":  dict(pitch=0.60, tempo=0.85, bass=9, echo=[(120, 0.4), (260, 0.28), (450, 0.16)], extra="acrusher=bits=10:mix=0.15"),
}
DEFAULT_PROFILE = PROFILES["Hero"]
SOUND_ROW = dict(volume=0.9, category="Voice", cooldown=0, pitch_min=1, pitch_max=1)

PS_SPEAK = r"""
Add-Type -AssemblyName System.Speech
$jobs = [IO.File]::ReadAllText($args[0], [Text.Encoding]::UTF8) | ConvertFrom-Json
$s = New-Object System.Speech.Synthesis.SpeechSynthesizer
$s.SelectVoice($args[1])
$s.Rate = [int]$args[2]
foreach ($j in $jobs) { $s.SetOutputToWaveFile($j.path); $s.Speak($j.text) }
$s.Dispose()
"""


def read(path):
    with io.open(path, encoding="utf-8-sig", newline="") as f:
        return list(csv.DictReader(f))


def spoken(text):
    # 화면용 기호를 읽기 좋게: 제목 줄바꿈(\n)·가운뎃점은 쉼, 말줄임은 그대로 쉼
    return text.replace("\\n", ". ").replace("·", ", ").replace("…", "... ")


def lines():
    ko = {r["Key"]: r["ko"] for r in read(os.path.join(DATA, "Localization.csv"))}
    out = []
    for table in ("Cutscenes.csv", "Dialogues.csv"):
        for r in read(os.path.join(DATA, table)):
            voice = r.get("Voice", "")
            if voice.startswith("Line_") and r["TextKey"] in ko:
                out.append((voice, r["Speaker"], spoken(ko[r["TextKey"]])))
    return out


def process(src, dst, profile):
    p = profile["pitch"]
    delays = "|".join(str(d) for d, _ in profile["echo"])
    decays = "|".join(str(a) for _, a in profile["echo"])
    trim = "silenceremove=start_periods=1:start_threshold=%s" % SILENCE
    chain = [
        trim, "areverse", trim, "areverse",   # TTS 앞뒤 빈 소리
        "aresample=%d" % RATE,
        "asetrate=%d" % int(RATE * p),
        "aresample=%d" % RATE,
        "atempo=%.3f" % (profile["tempo"] / p),
        "bass=g=%d:f=120" % profile["bass"],
        "aecho=0.85:0.7:%s:%s" % (delays, decays),
    ]
    if profile.get("extra"):
        chain.append(profile["extra"])
    chain += ["highpass=f=55", "loudnorm=I=-15:TP=-1.5:LRA=9", "aresample=%d" % RATE]
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


def main():
    os.makedirs(OUT, exist_ok=True)
    todo = lines()
    with tempfile.TemporaryDirectory() as tmp:
        jobs = [dict(path=os.path.join(tmp, name + ".wav"), text=text) for name, _, text in todo]
        job_file = os.path.join(tmp, "jobs.json")
        with io.open(job_file, "w", encoding="utf-8") as f:
            json.dump(jobs, f, ensure_ascii=False)
        script = os.path.join(tmp, "speak.ps1")
        with io.open(script, "w", encoding="utf-8-sig") as f:
            f.write(PS_SPEAK)
        subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, job_file, VOICE_NAME, str(SPEAK_RATE)], check=True)
        lengths = {}
        for (name, speaker, _), job in zip(todo, jobs):
            dst = os.path.join(OUT, "VO_" + name + ".wav")
            process(job["path"], dst, PROFILES.get(speaker, DEFAULT_PROFILE))
            lengths[name] = length(dst)
            print("voice %s %s %.1fs" % (name, speaker, lengths[name]))
    add_sound_rows([name for name, _, _ in todo])
    fit_durations(lengths)
    print("lines", len(todo))


def length(path):
    with wave.open(path) as w:
        return w.getnframes() / float(w.getframerate())


def fit_durations(lengths):
    """대사 줄이 목소리보다 빨리 넘어가지 않게 Duration을 늘린다 (줄이지는 않는다)"""
    for table in ("Cutscenes.csv", "Dialogues.csv"):
        path = os.path.join(DATA, table)
        with io.open(path, encoding="utf-8-sig", newline="") as f:
            rows = list(csv.reader(f))
        head = rows[0]
        voice, duration = head.index("Voice"), head.index("Duration")
        for r in rows[1:]:
            if r[voice] in lengths:
                r[duration] = "%g" % max(float(r[duration]), round(lengths[r[voice]] + LINE_PAD, 1))
        out = io.StringIO()
        csv.writer(out, lineterminator="\n").writerows(rows)
        with io.open(path, "w", encoding="utf-8-sig", newline="") as f:
            f.write(out.getvalue())


if __name__ == "__main__":
    sys.exit(main())
