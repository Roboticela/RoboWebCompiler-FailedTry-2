# Fonts

The faces the runtime loads when it resolves text.

```
Fonts/
  Inter/
    InterVariable.ttf   the default family
    LICENSE.txt         SIL Open Font License 1.1
```

## Inter is the default

`InterVariable.ttf` is the Inter variable font, vendored from
<https://github.com/rsms/inter> (`docs/font-files/InterVariable.ttf`) and
licensed under the SIL Open Font License 1.1, which is included beside it. Inter
is the runtime's default family: `FontRegistry` selects it as the primary face
for both regular and bold text whenever it is present. Because the variable font
carries one default instance (Regular), bold is synthesized — advances widen and
the rasterizer dilates the coverage — so a second file is not required for the
weight to read as a weight.

## Everything else comes from the host

Inter covers Latin, Greek and Cyrillic; it does not cover CJK, Indic, Arabic or
emoji. The registry therefore walks a **fallback chain** per code point: it loads
a curated set of system faces (Segoe UI, Nirmala UI, Malgun Gothic, Microsoft
YaHei, Yu Gothic, Segoe UI Emoji, and so on, by file name per platform) and picks
the first one that maps the code point. On a host with no font files at all the
registry falls back to the built-in stroke face, so a renderer still draws text.

## Search path

1. `ROBO_FONT_DIRS` — a `;`-separated list of directories, each scanned for
   `.ttf`, `.otf` and `.ttc` files.
2. `Fonts/` beside the working directory, or in any parent of it.
3. The platform font directory (`%WINDIR%\Fonts`, `/usr/share/fonts`, …): the
   curated file names first, then a shallow scan if none of them exist.

Parsed faces are cached process-wide, so a collection that costs a second to read
is read once rather than once per renderer.
