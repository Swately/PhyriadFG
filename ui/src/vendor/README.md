# vendored from `catalog/web/tokens` (phyriad-tokens 1.3.0)

Copied, not linked: Tauri serves `ui/src` as `frontendDist`, so anything the window loads has to be
inside it. Do not edit these two files here — edit the substrate and re-copy, or the next sync
silently reverts the edit.

    catalog/web/tokens/{phyriad.tokens.css,phyriad.base.css}  ->  ui/src/vendor/

Verify the substrate before copying: `node testbench.mjs` in `catalog/web/tokens` (107/107 at 1.3.0).

The palette is selected in `../index.html` with `data-palette="maid"`. Removing that one attribute
returns the launcher to the substrate default (oxide) without touching a stylesheet.

*Made with my soul - Swately <3*
