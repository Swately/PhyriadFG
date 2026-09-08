# E2b — plain img2img cannot do this, and the picture proves it

**Date** 2026-09-07 · **Rig** RTX 4090 · **Runtime** stable-diffusion.cpp Vulkan (see
[E2a](E2A_SDTURBO_VULKAN.md)) · **Model** SD-Turbo, f16 + flash attention, TAESD decoder
**Content** Honkai: Star Rail, pause menu, captured live by the FG's own `--qdump` (15 truth-less
triples, 1920x1080, `WGC: capturing window 'Honkai: Star Rail'`), centre-cropped to 512x512.
The operator had removed DLAA and set render scale to 1.0, so the source carries real, unsmeared edges.

## What was being tested

The scope document's stop rule for E2: stylize each frame twice with different seeds and compare, and
**if "estilo simpsons" is not dramatically more stable than "hiperrealista", the subtraction-vs-addition
thesis is wrong.** That test never got to run, because a prior assumption failed first.

## The two-seed probe returned an impossible result, and that was the finding

Three very different prompts — Simpsons, Looney Tunes, hyperrealistic — produced a two-seed mean
absolute difference of **2.41/255 for all three**, and Sobel energies of 5.87 / 5.87 / 5.86. Identical
to two decimals across prompts is not a measurement, it is a broken experiment, and it was treated as
one rather than written down.

Cause, found by looking at the image: at `--strength 0.45 --steps 2` **the prompt was doing nothing at
all.** The output was the source frame with the HUD text turned to gibberish and no style applied. All
three prompts agreed because none of them was reaching the picture.

## The strength ladder — the real result

Same frame, same seed, same prompt ("The Simpsons cartoon style, flat solid colors, thick black
outlines, no shading"), sweeping denoising strength. `diff` is mean absolute difference from the source
in 0-255; `detail` is Sobel edge energy relative to the source:

| strength | effective steps | diff from source | detail | what the image shows |
|---|---|---|---|---|
| 0.45 | **1** | **9.94** | 79 % | the game, intact. **No style at all** — only the HUD text destroyed |
| 0.50 | 2 | 50.65 | 94 % | the game is gone |
| 0.55 | 2 | 50.65 | 94 % | the game is gone |
| 0.70 | 2 | 50.65 | 94 % | a Simpsons living room. No character, no menu |
| 0.85 | 3 | 68.68 | 157 % | a crowd of Simpsons characters |
| 0.95 | 3 | 67.71 | 168 % | a crowd of Simpsons characters — flawless style, zero relation to the frame |

**There is no strength that gives both the style and the game.** The transition is not a slope; it is
a step between strength 0.45 and 0.50.

**And the mechanism is cruder than "strength".** `sd-cli` runs `int(steps x strength)` actual steps, so
at `--steps 4` the values 0.50, 0.55 and 0.70 all resolve to **2 steps** — which is why three different
strengths return byte-identical difference figures. The real control is an integer step count, and with
SD-Turbo **one step preserves the frame and two steps replace it.** There is no dial between them.

## What this establishes

1. **Plain img2img with a text-to-image model is the wrong tool for this job**, and that is a
   structural fact, not a tuning failure. SD-Turbo is a *generator*, not an *editor*: it has no term
   that says "keep this geometry". Asked to change the picture at all, it changes the subject.
2. **The scope document's expressiveness ceiling is confirmed visually rather than argued.** It said an
   img2img filter changes APPEARANCE, not CONTENT, and that turning a game frame into a Simpsons
   character is re-drawing rather than filtering. The 0.95 panel is exactly that prediction: perfect
   Simpsons, no game.
3. **The HUD problem is real and appears at the very first usable setting.** At strength 0.45 — the one
   setting that preserved the frame — the only visible change was the menu text turning to gibberish.
   Capture is post-composite and there is no UI mask.
4. **The two-seed strobe test is still unrun**, and cannot be run until a configuration exists that
   produces style AND structure. Its stop rule stands, unexercised.

## The next thing to try, and why it is not a guess

The missing ingredient has a name and the scope document already priced it: **structural conditioning**.
ControlNet (canny / lineart / softedge) supplies the "keep this geometry" term that plain img2img
lacks, at a cost the literature puts at **+20-30 % of step time**. And PhyriadFG has an unusual asset
for driving it — `shaders/igpu_field.comp` already produces a **per-pixel Sobel magnitude field plus a
thresholded edge class** on the iGPU, every frame, for the frame generator's own purposes.

Whether a raw Sobel magnitude field usefully drives a ControlNet is, per the scope document,
**unanswered in the published literature** — canny models are trained on thinned, hysteresis-linked
contours and this shader has neither non-maximum suppression nor hysteresis, so softedge/scribble are
the better bet than canny. That is reasoning, not measurement, and it is the next experiment.

*Made with my soul - Swately <3*
