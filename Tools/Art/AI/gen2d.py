"""AI 원화 생성: FLUX.1-schnell (Hugging Face Space, Apache-2.0)로 캐릭터 원화를 뽑는다.
실행: YJJFantasy/Tools/AI3D/.venv 의 파이썬 (gradio_client), HF_TOKEN 사용자 환경변수
  python gen2d.py <이름> "<프롬프트>" [시드 ...]
결과: Tools/Art/AI/Raw/<이름>_<시드>.png (1024x1024, 흰 바탕)"""
import os
import shutil
import sys

from gradio_client import Client

SPACE = "black-forest-labs/FLUX.1-schnell"
# 병맛 낙서풍: 아주 단순한 흰 몸통, 점 눈, 무표정하거나 광기 어린 얼굴, 얇은 먹선, 납작한 몇 가지 색, 엉뚱한 비율
STYLE = ("extremely simple minimalist doodle, mobile game unit sprite, plain white body with thin uneven black outline, "
         "tiny dot eyes, deadpan or slightly deranged expression, flat colors, no shading, no gradients, "
         "absurd surreal proportions, goofy but a little creepy, crude MS paint feel, full body, side view facing right, "
         "centered, plain flat white background, no text, no ground shadow")
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Raw")


def main():
    name, prompt = sys.argv[1], sys.argv[2]
    seeds = [int(s) for s in sys.argv[3:]] or [1]
    os.makedirs(OUT, exist_ok=True)
    client = Client(SPACE, token=os.environ.get("HF_TOKEN") or None, verbose=False)
    for seed in seeds:
        result, used = client.predict(prompt=f"{prompt}, {STYLE}", seed=seed, randomize_seed=False,
                                      width=1024, height=1024, num_inference_steps=4, api_name="/infer")
        path = result["path"] if isinstance(result, dict) else result
        out = os.path.join(OUT, f"{name}_{int(used)}.png")
        shutil.copyfile(path, out)
        print("wrote", out, flush=True)


if __name__ == "__main__":
    main()
