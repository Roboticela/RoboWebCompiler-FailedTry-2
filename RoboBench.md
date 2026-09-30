# RoboRenderer benchmark report

Measured by `RoboBench`. Every table below is produced by running the runtime:
documents are compiled in process, then laid out, painted and executed by
`RoboRenderer` and the software backend. Times are the best of 5 runs.

Window: 1280x800 logical units at 1x.

Build: optimized (NDEBUG), debug overlay compiled out.

Documents: 1000 requested elements x static, text, images, interactive, mixed.

## Frame cost

| scenario | elements | compile | initialize | first frame | layout | paint | execute | idle frame | idle work | interactive dispatch | interactive frame | resize |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| static | 2002 | 48.27 ms | 2.22 ms | 5.01 ms | 0.736 ms | 0.377 ms | 3.90 ms | 0.000 ms | skipped | &mdash; | &mdash; | 7.70 ms |
| text | 1006 | 7.87 ms | 2.21 ms | 11.17 ms | 2.48 ms | 2.92 ms | 5.78 ms | 0.000 ms | skipped | &mdash; | &mdash; | 13.33 ms |
| images | 2006 | 18.86 ms | 3.89 ms | 4.51 ms | 0.666 ms | 0.268 ms | 3.58 ms | 0.000 ms | skipped | &mdash; | &mdash; | 8.51 ms |
| interactive | 1610 | 16.20 ms | 2.19 ms | 18.51 ms | 1.71 ms | 0.786 ms | 16.02 ms | 0.000 ms | skipped | 0.010 ms | 4.53 ms | 25.75 ms |
| mixed | 2674 | 15.22 ms | 3.29 ms | 10.43 ms | 2.16 ms | 0.961 ms | 7.31 ms | 0.000 ms | skipped | 0.022 ms | 3.41 ms | 13.92 ms |

## Frame content and memory

`elements` here is the count the frame laid out, not the count the document
compiled to: the first table reports the latter.

| scenario | elements laid out | commands | glyphs | lines | pixels drawn / target | allocations (first frame) | compiled | runtime state | layout | paint list | framebuffer |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| static | 1002 | 2001 | 6890 | 1000 | 2150492 / 1024000 | 4043 | 251.5 KiB | 23.5 KiB | 199.4 KiB | 213.6 KiB | 4.94 MiB |
| text | 1006 | 1507 | 87520 | 1506 | 2126108 / 1024000 | 4582 | 189.3 KiB | 11.8 KiB | 144.3 KiB | 1.08 MiB | 4.94 MiB |
| images | 2006 | 1504 | 3397 | 501 | 2071389 / 1024000 | 4552 | 243.9 KiB | 23.5 KiB | 170.4 KiB | 139.7 KiB | 4.94 MiB |
| interactive | 1610 | 7237 | 5408 | 1005 | 2842897 / 1024000 | 11942 | 219.6 KiB | 18.9 KiB | 172.1 KiB | 544.0 KiB | 4.94 MiB |
| mixed | 2006 | 4511 | 22826 | 1503 | 3431711 / 1024000 | 16948 | 320.2 KiB | 31.3 KiB | 276.1 KiB | 567.1 KiB | 4.94 MiB |

## CSS compilation

The compiler's side of the pipeline, measured on a document of 1000 elements in 250 classes
styled by a generated stylesheet. `compile` is the whole build - parse, cascade,
resolve, lower - and `CSS (difference)` is that build minus the same document
compiled with no stylesheet, so it is the work CSS added. Both are compile
time: nothing here is per frame.

| rules | declarations | elements | distinct styles | stylesheet | compile | without CSS | CSS (difference) | per declaration | compiled |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 100 | 713 | 2002 | 116 | 16.8 KiB | 73.71 ms | 15.56 ms | 58.15 ms | 81.55 us | 265.8 KiB |
| 1000 | 7125 | 2002 | 271 | 169.5 KiB | 169.13 ms | 14.74 ms | 154.40 ms | 21.67 us | 285.1 KiB |
| 10000 | 71250 | 2002 | 271 | 1.67 MiB | 1361.69 ms | 14.46 ms | 1347.23 ms | 18.91 us | 285.1 KiB |

The baseline behind `CSS (difference)` is the same document with its class tokens
removed and no stylesheet, so the column is CSS resolution and nothing else, with
the styled document's attribute text excluded from it.

The `distinct styles` column is the interning result: 1000 elements, styled by a handful of class tokens, and one entry per
distinct computed style in the compiled table. Compiling CSS is work an application pays
once; it is reported here so that a change which makes the runtime faster at the
compiler's expense can be seen, and judged on purpose.

## Notes

- `compile` is HTML parsing, CSS resolution, validation and lowering for the whole
  document. It is reported because it is work the application does once, not per
  frame; the CSS table separates that part of it out.
- `idle frame` is a frame in which nothing was invalidated: the runtime detects that
  and does not touch the backend at all.
- `interactive dispatch` is the cost of routing one pointer move through hit testing
  and runtime state; `interactive frame` is the average frame that follows it, which
  repaints only the region the change affected.
- `runtime state` is the per element state array: the renderer keeps a fixed
  record for every element, whether or not the frame draws it.
- `layout` is the geometry of one pass and `paint list` is the display list it
  produced. Both scale with what the document contains and both are rebuilt
  whenever layout or paint is invalidated, so a regression in either is a
  regression in a frame.
- `idle work` is read back from the runtime's frame metrics: `skipped` means the
  idle frame reported no layout pass, no paint pass and no pixels written.
- Text is measured on its own, on a ~500 character run at one width: 1.29 us to measure,
  5.00 us to measure and break into lines. Both are the average of a timed batch.
