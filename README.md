# YJJWave — 물량 맞다이

수천~수만 마리 병사를 쏟아부어 끝없이 밀려오는 괴물 물결과 정면으로 맞붙는 디펜스. 언리얼 엔진 5.8 C++.

![4만 마리](DevLog/Shots/2026-11-22_Bench_40000.png)

- 기획: [Docs/Design.md](Docs/Design.md)
- 도구·데이터 표·조작: [Tools/README.md](Tools/README.md)
- 개발 일지·성능 표: [DevLog/devlog.md](DevLog/devlog.md)

## 성능 (i5-13400F, RX 7800 XT, 1920x1080)

| 아군 vs 적 | 시뮬 1틱 | 프레임 | fps |
|---|---|---|---|
| 5,000 vs 5,000 | 2.2ms | 3.8ms | 261 |
| 10,000 vs 10,000 | 3.9ms | 8.0ms | 124 |
| 20,000 vs 20,000 | 6.9ms | 14.7ms | 68 |

4만 마리에서 드로우 콜 58개, GPU 1.2ms (병목은 게임 스레드). 자세한 표: [DevLog/devlog.md](DevLog/devlog.md)

글꼴: 배민 도현체 / Do Hyeon (SIL OFL, `Content/YJJWave/Fonts/OFL-DoHyeon.txt`, Google Fonts).
