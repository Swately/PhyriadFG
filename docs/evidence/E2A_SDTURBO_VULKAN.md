# E2a — SD-Turbo on stable-diffusion.cpp / Vulkan, measured on this 4090

**Date** 2026-09-07 · **Rig** RTX 4090, driver 610.88, machine idle
**Runtime** `stable-diffusion.cpp`, release `master-845-80bac2d` (published 2026-09-06), **Vulkan**
backend, `sd-master-80bac2d-bin-win-vulkan-x64.zip`, **38.9 MB** — MIT, pure C/C++, and it runs on the
same Vulkan API PhyriadFG already uses, which is why it was preferred over the 336 MB CUDA build.
**Model** `stabilityai/sd-turbo`, single-file `sd_turbo.safetensors`, **5.215 GB F32** as published
(1,229 tensors; header offsets verified against the file tail — COMPLETE, not a truncated transfer).
**Decoder** `madebyollin/taesd`, **MIT**, 9.8 MB.

## The licence question, settled

The scope document flagged SD-Turbo's licence as **disputed and on the critical path** — the HF tag
reportedly said `sai-nc-community` while the model card said commercial use was intended — and called
reading the actual file "a 20-minute task that gates the whole shortlist". Done, today:

- **The HuggingFace API reports `license: None`.** There is no `sai-nc-community` tag. The stale-tag
  reading was correct.
- **`LICENSE.md` in the repo is the STABILITY AI COMMUNITY LICENSE AGREEMENT, Last Updated July 5, 2024.**

What it grants, quoted:

- **§2 Research & Non-Commercial** — "use, reproduce, distribute, and create Derivative Works of, and
  make modifications to" the materials, free, where Non-Commercial explicitly includes *"personal use
  (i.e., hobbyist) or evaluation and testing"*. **A free open-source hobby project is covered outright.**
- **§3 Commercial** — also free, below **USD $1,000,000** annual revenue, on registration at
  stability.ai/community-license.

What it obliges, and this shapes the product. **§4(a) applies if you distribute the materials *"or a
product or service that uses any portion of them"*** — so it binds even if the weights are not bundled:

1. provide a copy of the Agreement to the third party;
2. ship a `Notice` file containing *"This Stability AI Model is licensed under the Stability AI
   Community License, Copyright © Stability AI Ltd. All Rights Reserved"*;
3. **prominently display "Powered by Stability AI"** on a related website, user interface, blogpost,
   about page, or product documentation.

Two restrictions to carry: **§4(b)** incorporates an external AUP by reference that Stability may
change unilaterally, and forbids using outputs *"to create or improve any foundational generative AI
model"* — a task-specific stylization student is not a foundation model, but the sentence is on record.
**§4(c)(iii)**: the operator owns the outputs.

**Recommended shape: the weights as an OPTIONAL SEPARATE DOWNLOAD under their own licence**, never
inside the MIT release. His code stays MIT with no tension; he carries only the attribution. This is
the same conclusion the scope document reached for size reasons, now with a licence reason too.

## The measurement the literature did not have

The scope document named this as the second experiment: *"nobody has published a Vulkan-vs-TensorRT
ratio for it."* Here it is. All figures RTX 4090, batch 1, 1 step, machine idle, from the runtime's
own instrumentation:

| configuration | UNet step | notes |
|---|---|---|
| F32, no flash attention (as-published weights) | **4,200 ms** | 4.20 s/it — the naive run |
| **f16 + `--diffusion-fa`, 512x512** | **~95 ms** (10.50 / 10.35 / 10.55 it/s over three runs) | |
| **f16 + `--diffusion-fa`, 960x544** | **~147 ms** (6.80 it/s) | 3.9x the pixels for 1.55x the time — sublinear |
| *the TensorRT anchor from the literature, 512x512* | *10.65 ms* | *measured elsewhere, not here* |

> **The Vulkan-via-stable-diffusion.cpp route is ~9x slower than the TensorRT anchor.**

Running the published F32 weights without flash attention costs **44x** more than f16 with it. Anyone
quoting a number from this runtime without stating precision and attention is quoting noise.

**VRAM: 2,431.54 MB total** (text encoders 770.53 MB + diffusion + VAE), reported by the runtime.
Against BF6's measured 11.34 GB on a 24 GB card, this fits with room.

## What it means for the rate

| runtime | step at 512x512 | styled fps ceiling | fits 30 styled fps? |
|---|---|---|---|
| stable-diffusion.cpp / Vulkan | ~95 ms | **~10 fps** | **no** |
| TensorRT (anchor, unverified here) | 10.65 ms | ~90 fps | yes |

[E1c](E1C_HEADROOM.md) measured the contention factor at **1.00x** with the game capped to 30 fps, so
that ~10 fps ceiling is real rather than degraded — the filter would get the GPU at full speed. But it
is a **ceiling of ~10 styled frames per second**, not 30.

The clean trade is now measured on both sides rather than argued: the Vulkan route is 38.9 MB, MIT,
same-API, no CUDA pinning and no CUDA-to-Vulkan interop (measured at 8.03 ms contended, 24 % of a
33.3 ms budget for doing nothing) — and it caps the filter at ~10 fps. The CUDA/TensorRT route reaches
30 fps and costs 262-346 MB, a CUDA/cuDNN dependency on the end user's machine, and that interop.

## Honest limits

1. **The encode/decode timings are polluted by per-run model loading.** `sd-cli` loads, runs once and
   exits, so its 0.35-0.89 s VAE figures include a load that a resident integration pays once. **The
   sampling figure is the clean one** and is what is quoted above.
2. **TAESD helped less than expected here** (decode 0.89 s to 0.59 s to 0.35 s across configurations)
   for the same reason. Its real value has not been isolated.
3. **No image quality was judged.** These are timings. The two-seed strobe probe on REAL captured game
   frames — E2's other half, and the one with a stop rule attached — has not been run.
4. **The 10.65 ms TensorRT anchor was not reproduced here.** The ~9x ratio compares this measurement
   against a literature figure, not against a TensorRT run on this machine.

*Made with my soul - Swately <3*
