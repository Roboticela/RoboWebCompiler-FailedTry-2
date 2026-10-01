# RoboRenderer

The native runtime and rendering system for RoboHTMLCompiler.

RoboHTMLCompiler answers *what the UI is*: it reads the source tree, validates
it, and lowers every document into flat, immutable compiled data. RoboRenderer
answers *what that UI looks like and how it behaves*: it consumes that data,
lays it out natively, measures and breaks text, routes input, hit tests, builds a
display list and executes it against a graphics backend.

> **RoboRenderer is not a browser.** It contains no HTML parser, no CSS parser,
> no selector matching and no cascade, no JavaScript engine, and no DOM. Everything
> a browser resolves at run time — which element is what, which rules apply,
> which classes exist, what a `<button type="submit">` does — was resolved at
> compile time and is already final by the time the renderer sees it.
>
> What it *does* contain is the work a browser does that genuinely cannot move
> earlier: layout against a real window size and DPI, text measurement and line
> breaking, hit testing, focus and input, a display list, and rasterization.

---

## 1. Where it sits

```
Examples/<App>/index.html
        |
        |  RoboHTMLCompiler:  parse -> validate -> style references -> native IR
        v
    NativeUiModel                      the compiler's own IR
        |
        |  CompiledUiBuilder:  flatten, deduplicate, resolve
        v
    CompiledDocument                   flat tables, read-only, in the binary
        |
        +--> GeneratedRuntime/<App>CompiledUi.{h,cpp}     for an application
        |
        v
    RoboRenderer
        |
        +-- UiState           per window mutable state (the compiled data is never written)
        +-- LayoutEngine      geometry, line boxes, clipping
        +-- PaintEngine       typed display list
        +-- HitTester / InputRouter
        +-- ResourceManager   images, decoded once
        |
        v
    PaintCommand stream
        |
        v
    RenderBackend  ->  SoftwareBackend (reference)  /  a GPU backend
        |
        v
    framebuffer / swap chain
```

Two properties of that picture matter more than the rest:

* **The compiled document is immutable and shared.** Every mutable thing lives in
  `UiState`, which is per window. Two windows can render the same document at
  once, and hovering a button in one changes nothing the other can see.
* **The runtime is later replaceable in one direction only.** The backend is
  below the display list and knows nothing about HTML, layout or state, so a
  Direct3D, Vulkan, Metal or RoboOS rasterizer is a drop-in for the reference
  software backend and nothing above it has to change.

---

## 2. The compiled contract

`Include/RoboRenderer/CompiledUi.h` is the seam between the two projects, and it
is the header a backend-free application links against.

| Table | What it holds |
| --- | --- |
| `CompiledDocument` | counts, root, and pointers to every other table |
| `CompiledElement[]` | one dense record per element: category, flags, parent/first/last/next links, string indices for text, id, name, value, href, label target, list style, resource, width/height, numeric value and range, bindings, source position |
| `CompiledStyle[]` | resolved style values (empty in the HTML-only phase; see §4) |
| `CompiledResource[]` | resource kind and source string, deduplicated by the compiler |
| `CompiledBinding[]` | element, event kind, handler name |
| `CompiledIdEntry[]` | sorted `id -> element` index, so a lookup is a binary search |
| strings | one string table, every string referenced by index |

Design rules the tables follow, because the runtime's cost model depends on them:

* **Dense and indexed.** The root is element 0 and the tree is a first-child /
  next-sibling structure over an array. No pointer chasing, no hash of tag names,
  no string comparison on any per-frame path.
* **Deduplicated.** Identical resources and identical strings are stored once.
  The runtime's key for a resource is its index, so resource lookup is an array
  access.
* **Normalised.** Source tags are collapsed into categories at compile time:
  `div`, `section`, `article`, `nav`, … are all `Container`; `h1`–`h6` are all
  `Heading`, with the level kept separately. `CategoryFor(ElementKind)` maps a
  compiled kind to one of the `RenderCategory` values the runtime implements
  (Document, Container, Text, PreformattedText, Heading, LineBreak, Separator,
  Link, List, ListItem, DescriptionTerm, DescriptionDescription, Figure,
  FigureCaption, Image, Media, Canvas, Form, FieldSet, Legend, Label, Button,
  Input, Checkbox, Radio, Range, ColorInput, FileInput, TextArea, Select, Option,
  OptionGroup, Output, Progress, Meter, Table, TableCaption, TableColumn,
  TableSection, TableRow, TableCell, Disclosure, DisclosureSummary, Dialog,
  Frame, EmbeddedObject, Ruby, RubyText, CustomElement).
* **Decisions are recorded, not re-derived.** `ButtonBehavior` is resolved from
  `type` at compile time, `listStyle` from the list kind and `type` attribute, the
  checked/selected/open/disabled flags from the document, the heading level from
  the tag. The runtime never reads an attribute because there are none left to
  read.

`RenderCategory::UnsupportedNamespace` is the one category the runtime does not
paint: SVG and MathML are recognised by the compiler and lowered, but they are
outside the HTML-only runtime phase. The element is recorded and its box takes
part in layout, but nothing is drawn for it — and that is reported as an `RRE004`
warning per occurrence, so it is never silently dropped.

---

## 3. Runtime state

`Include/RoboRenderer/RuntimeState.h`. One `UiState` per window, built from the
compiled document and reset on demand.

* `ElementState[]` — one small record per element: a 32-bit flag word plus scroll
  offsets. The flags cover hover, press, focus, focus-visible, the hovered and
  focused *subtrees*, checked, selected, open, disabled, a value-changed mark, and
  the two invalidation bits (layout-dirty, paint-dirty).
* `InputFieldState[]` — text editing state (value, caret, selection anchor, field
  scroll), stored **only for text controls**, with a sparse `element -> slot`
  index. A 100,000-element document with no text field stores nothing here.
* Numeric overrides — range, progress and meter values the runtime changed, kept
  in a small sparse list rather than an array indexed by element.
* Pointer and focus — the hovered, pressed and focused element, plus the
  document scroll offset.

`SeedFromCompiled()` is where runtime state is initialised from the compiled
data: checked/selected/open/disabled flags, and the initial `value` of a control
(which for a `<textarea>` is its compiled *content*, not its `value`).

Everything is invalidated through those two flags. Nothing walks the tree to
decide whether it needs to redraw, and nothing compares old and new geometry.

---

## 4. Style: there is no CSS at run time

`Include/RoboRenderer/Style.h`.

A renderer usually depends on CSS at this point. This one depends on resolved
values: `CompiledStyle` is the final value of every property the layout and paint
engines consume, and they consume *final values* rather than declarations. In the
HTML-only phase those values come from `DefaultStyleProvider`, which holds one
default style per `RenderCategory` — the formatting defaults of HTML expressed as
native values (a document is a white page with 8 units of padding; a heading is
bold and larger; a textarea is a 220×60 scroller; a checkbox is a 14×14 control;
and so on) — adjusted per element by the compile-time nature the compiler
recorded (inline vs block, hidden, disabled, direction).

When CSS is added later, the compiler publishes resolved values into
`CompiledStyle[]` and the layout and paint engines do not change: they already
read final values. That is the whole reason the style table exists in the
contract before anything fills it.

The same header exposes the questions layout asks about a category —
`IsBlockCategory`, `IsInlineCategory`, `IsReplacedCategory`, `IsTextCategory`,
`CategoryIntrinsicSize` — so the layout engine branches on native categories, not
on tag names.

---

## 5. Layout

`Include/RoboRenderer/Layout.h`, `Source/RoboRenderer/LayoutEngine.cpp`.

Layout is runtime work and it is *expected*: it depends on the window size, the
device scale, the measured text, the current value of a control and whether a
disclosure is open. What it never does is read HTML.

Geometry is produced in **absolute document coordinates**. Scrolling is a
translation that painting and hit testing apply on their way down the tree, which
is why scrolling never invalidates layout.

`LayoutResult` holds three parallel arrays and two scalars:

| Field | Meaning |
| --- | --- |
| `elements[]` | `ElementLayout`: border box, content box, clip rect, scroll extent, first line and line count, depth, flags |
| `lines[]` | `LineLayout`: one line-box fragment — owning element, establishing block, x/width/baseline, line top/height/width, font size and weight, byte range, and the placeholder/control-value/underline markers |
| `elementLines[]` | line-fragment indices grouped by element, so an element can name its own fragments even though a line mixes several elements |
| `documentWidth`, `documentHeight` | what the viewport scrolls |
| `laidOutCount`, `lineCount` | counts the metrics and the debug overlay report |

`LayoutFlagDisplayNone` is how the engine removes something from the flow
entirely; the engine also marks it so the paint engine and hit tester skip it
without asking why.

The behaviours worth naming, because each one is a decision:

* **Block flow.** A block's containing width is the available width; sibling
  boxes stack. Anonymous inline rows are laid out with the category default style
  rather than an element's.
* **Inline flow.** An inline run is broken into line fragments against the
  available width, at real break opportunities, and the fragment's block's first
  and last line boxes are grown around them. `ResolveInlineBoxes()` then unions an
  element's own fragments back into its border box — that is what makes a `<span>`
  a real, hit-testable, clickable box rather than a decoration.
* **Replaced elements.** An `<img>` takes its size from the declared attributes,
  then from the decoded resource's intrinsic size, then from the category default,
  and preserves the aspect ratio when only one dimension is known.
* **Tables.** Sections, rows and cells are laid out as their own boxes with
  spanning recorded at compile time (`columnSpan`, `rowSpan`), and the cell's
  resolved border is drawn per edge, so a cell paints only the sides it asks for.
* **`<details>`.** A closed disclosure collapses everything except its
  `<summary>`; opening it changes layout, and the runtime treats that as exactly
  that — a layout invalidation, not a special case in the painter.
* **The root fills the viewport** vertically, so a short document paints a page
  rather than a stripe.

---

## 6. Text

`Include/RoboRenderer/Text.h`, `Source/RoboRenderer/Text.cpp`,
`Source/RoboRenderer/FontFile.{h,cpp}`, `Source/RoboRenderer/FontTables.cpp`,
`Source/RoboRenderer/OpenTypeLayout.cpp`, `Source/RoboRenderer/FontVariations.cpp`,
`Source/RoboRenderer/FontStack.cpp`, `Include/RoboRenderer/Unicode.h`,
`Source/RoboRenderer/Unicode.cpp`, `Source/RoboRenderer/Generated/UnicodeData.{h,cpp}`,
`Source/RoboRenderer/Bidi.{h,cpp}`, `Source/RoboRenderer/StrokeFont.cpp`.

Text is measured, broken, shaped and painted from **real font files**. `SfntFont`
parses the sfnt container — `head`, `hhea`, `hmtx`, `maxp`, `cmap`, `glyf`/`loca`,
the `CFF `/`CFF2` Type 2 charstrings of an `OTTO` face, `OS/2`, `name`, `kern`,
`vhea`/`vmtx`/`VORG`, the layout tables `GSUB`/`GPOS`/`GDEF`,
the variation tables `fvar`/`avar`/`gvar`/`HVAR`/`MVAR`, and the colour tables
`COLR`/`CPAL`/`CBDT`/`CBLC`/`sbix`/`SVG ` — and resolves a code point to a glyph
and a glyph to filled contours (composite glyphs included). `FontRegistry` loads
those faces from disk and answers which face is primary for a weight and which
face in the fallback chain actually covers a code point.

**Shaping is real.** `OpenTypeLayout.cpp` implements `GSUB` (single, multiple,
alternate, ligature, contextual and chained-contextual, extension and
reverse-chaining), `GPOS` (single, pair, cursive, mark-to-base, mark-to-ligature,
mark-to-mark, contextual and chained-contextual, extension) and `GDEF` (glyph
classes, mark attachment classes, mark glyph sets). Only the features that are ON
by default are applied — `ccmp`, `locl`, `rlig`, `liga`, `clig`, `calt`, the
Arabic/Syriac joining forms and the Indic conjunct forms — so discretionary
features (`dlig`, `salt`, `smcp`, …) never silently change text. A run's script
system is chosen from the first strong character's Unicode script
(`OtLayout::ScriptTagForName`), and `ShapedGlyph` carries advances, offsets and
clusters in font design units.

**Inter is the default family.** It is loaded from `Fonts/Inter/` as a variable
font; `SetInstance` resolves `fvar` axes (applying `avar` remapping), interpolates
`gvar` deltas into the outlines and `HVAR` deltas into the advances. When a weight
the file does not carry is asked for, bold is synthesized — advances widen and the
rasterizer dilates the coverage — so a bold run measurably measures wider than the
same run in regular weight. Everything Inter does not cover falls through to a
curated chain of system faces (Segoe UI, Nirmala UI, Malgun Gothic, Microsoft
YaHei, Yu Gothic, Segoe UI Emoji, …), chosen by code-point coverage. An emoji code
point prefers a colour-capable face over an earlier monochrome symbol face. A host
with no font files at all falls back to the built-in **stroke face**: a compiled-in
vector glyph table, so a renderer still draws text rather than nothing.

**Colour glyphs.** `COLR` v0 layer lists are read directly; `COLR` v1 paint graphs
are flattened to the same layer list (transforms are dropped and gradients are
approximated by their first colour stop), which is enough for the colour emoji the
Windows and macOS faces ship. `CBDT`/`CBLC` embedded bitmaps are decoded with the
runtime's own PNG codec, choosing the strike closest to the device size. The
`GlyphCache` composites COLR layers in their `CPAL` colours into an RGBA bitmap and
hands it to the backend, which blends it instead of tinting coverage.

**Unicode.** `Unicode.{h,cpp}` (data generated by `Tools/Unicode/GenerateUnicodeData.py`,
Unicode 16.0.0) supplies general category, script, bidi class, combining class,
line-break class, East Asian width, emoji properties, normalization (NFC/NFD/NFKC/
NFKD), case mapping, grapheme/word/sentence segmentation, line-break
opportunities and the full bidi algorithm (`ResolveParagraph`). `Bidi.h` keeps the
smaller predicates the paint run reorderer asks: `IsRtlCodepoint`, `IsLtrCodepoint`,
`IsCjkCodepoint`, `IsClusterExtender`, `IsRegionalIndicator`.

**Direction.** A run's direction is decided by its first strong character; the run
is shaped in logical order and then a right-to-left run is reversed for display, so
the letters were joined in the order they were written. A CSS `direction` on the
element overrides that heuristic — an `rtl` paragraph paints its first character at
the right edge however Latin the text is.

**Italics.** `font-style: italic` selects an italic sibling of the chosen family
when the host installs one and otherwise shears the upright design synthetically
(`Glyph::slant`, applied by the rasterizer). A shear changes no advance, so
measurement is identical and only the painted outline leans. `TextRunStyle` carries
the family, slant and direction a run was measured and painted with, and
`FontRegistry::Face`/`FaceFor` take the slant, so layout and paint always resolve
the same face.

* `Measure(text, size, weight)` — the width of a UTF-8 string.
* `MeasurePrefix` — the width of the first *n* bytes, which is what caret
  positioning and hit testing inside a text field need.
* `LineHeight`, `Advance`, `TabAdvance` — a tab stop is four spaces.
* `ShapeRun(text, start, end, size, weight, glyphs)` — the single source of truth
  for advances: it groups the run by covering face, shapes each group with that
  face's `GSUB`/`GPOS`, and returns positioned glyphs. `Measure`, `MeasurePrefix`,
  `HitTestOffset` and the paint pass all walk these glyphs, so measurement and
  painting can never disagree about where a glyph lands.
* `Wrap(text, size, weight, availableWidth, preserveWhitespace, lines)` — breaks
  into lines at spaces, hyphens and explicit line breaks, and never inside a
  grapheme cluster: a combining mark, variation selector, skin-tone modifier or
  anything after a zero-width joiner stays with the base character it decorates.
  `preserveWhitespace` turns wrapping off, which is what `<pre>` needs. A width of
  zero means "unbounded": only explicit breaks apply.
* `ByteOffsetForCodepoint` / `CodepointIndexForByte` / `CodepointCount` /
  `DecodeUtf8` — the byte/code-point conversions that keep the runtime's indices
  unambiguous.

`GlyphCache` rasterizes a glyph once per (face, font size, weight) and hands the
same bitmap to every backend that asks. A glyph is addressed by code point, or by
its font glyph id when shaping produced a ligature or contextual form that has no
code point of its own. A filled outline is flattened to polygons and filled with
the non-zero winding rule on a 4x4 sampling grid; a colour glyph is composited in
its palette colours (or decoded from its embedded bitmap) into RGBA pixels; the
built-in stroke face is stroked instead. The cache is owned by the renderer rather
than the backend, because a cache is a correctness question (all backends must
rasterize identically), not a backend implementation detail. A device-scale change
makes every cached bitmap wrong rather than merely stale, so `Resize` clears the
cache.

---

## 7. Input and hit testing

`Include/RoboRenderer/Input.h`, `Source/RoboRenderer/Input.cpp`.

Hit testing maps a point to an element **index**, through rectangles and the
compiled tree. It never looks up an element by name, never walks a string and
never asks what a tag is.

* `HitTest(x, y)` — the deepest element whose border box contains the point and
  which is not clipped away; the last painted child wins, because it was painted
  last.
* `HitTestInteractive(x, y)` — the nearest interactive ancestor, which is what a
  click actually targets. It returns `NoElement` on a miss rather than falling
  back to the root: a miss is a miss, and reporting the document as hovered is a
  bug that looks like a feature.
* `ScrollableAt` — the innermost scrollable element under the point.
* `ElementBounds` — an element's border box in window coordinates, with the
  scroll translation of its ancestors applied.
* `LocalPosition` — a point in an element's content coordinates.

The clip rule is the subtle part: an element is tested against the clip its
*ancestors* impose, and it only tightens the clip it passes to its children when
it actually clips or scrolls. An element that clips its own overflow must not be
able to reject the point that is inside itself — otherwise a single
`overflow: clip` control swallows every hit in the window.

`InputRouter` is behaviour, not presentation. It owns the runtime actions the
runtime performs itself, with no document script involved:

* hover, press and click, including the hovered/focused *subtree* flags;
* focus movement, `NextFocusable`, and drawing the focus ring only when the focus
  came from the keyboard (`focusVisible`);
* activation: buttons (submit / reset / plain, from `ButtonBehavior`), checkboxes
  and radios, links, disclosures (`<summary>`), selects and options;
* text editing: `InsertText`, `DeleteBackward`, `DeleteForward`, `SelectAll`,
  `MoveCaret`, caret blink;
* scrolling, by wheel and by drag, in the element under the pointer or the
  document when there is none.

Every dispatch returns an `EventResult` that says what happened *and* what the
renderer must recompute: `layoutChanged`, `paintChanged`, `scrolled`,
`focusChanged`, `submitted`, `reset`, `activated`, `activatedHref`. The renderer
turns that into invalidation; the application can ignore all of it and still get
a correct UI, or act on the last four.

Keys the runtime models are a small closed set (`Enter`, `Escape`, `Tab`,
`Backspace`, `Delete`, arrows, `Home`, `End`, `PageUp`/`PageDown`, `Space`).
A key it does not know is still delivered, with `Key::None`, so an application
can act on it.

---

## 8. Paint

`Include/RoboRenderer/Paint.h`, `Source/RoboRenderer/PaintEngine.cpp`.

The paint engine turns geometry plus runtime state into a **flat, typed display
list**: a `PaintList` of `PaintCommand`s plus a parallel glyph array that
`DrawText` commands index into by (`firstGlyph`, `glyphCount`). Ten command types:

```
FillRect  FillRoundedRect  StrokeRect  StrokeRoundedRect  DrawImage  DrawText
PushClip  PopClip  PushTransform  PopTransform
```

A command carries everything a backend needs and nothing else: bounds in logical
units, colour, resource index, radius, per-edge widths, font size and weight, and
the element it came from (for the debug overlay and for tests). `PushTransform`
exists for scrolled subtrees and DPI, never for HTML semantics.

Control chrome — button surfaces, the checkbox face and its check mark, the radio
dot, the range groove, fill and thumb, the select box and its label, progress and
meter bands, table headers, the caret — is drawn here from runtime state, as
native appearance rather than metadata the document carries. Hover and pressed
appearance is a shading factor applied to the resolved colour.

**Damage tracking.** `PaintEngine::Build` computes the region to repaint from the
elements that were explicitly invalidated, and unions their subtree rectangles.
It is deliberately *not* derived from "every element whose paint-dirty flag is
set": an ancestor carries a `HoveredSubtree` flag too, so a hover would otherwise
expand its damage to the whole window through the root. An element's own
invalidation is the union of what that element and its own subtree can change.

A frame is a **full** repaint when layout ran (every rectangle the display list
held is stale), when the paint engine was asked to rebuild everything, or when the
renderer was explicitly told to. Otherwise only the damaged region is cleared and
redrawn — everything outside it keeps the pixels the previous frame left, which
is what makes a hover cheap.

`PaintStats` reports command count, glyph count, image count and the damage area
for the debug overlay and the benchmark.

---

## 9. The graphics backend

`Include/RoboRenderer/Backend.h`.

`RenderBackend` is a dozen virtual calls and nothing else: `Initialize`,
`Shutdown`, `Resize`, `BeginFrame(damage, FrameContext)`, `Execute(command)`,
`EndFrame`, plus `Name`, `LastError`, `SetClearColor`, `DrawnPixelCount` and
`ExecutedCommandCount`. Everything that decides *what* to draw sits above it, so
a backend is a rasterizer and no more.

`SoftwareBackend` is the **reference implementation, and a real one**: anti-aliased
rectangles from exact per-pixel coverage, rounded rectangles from a signed
distance, per-edge borders, bilinear image scaling, coverage-blended text, a clip
stack and a translation stack, all into an RGBA8 framebuffer. It exists to be
correct, to be testable pixel for pixel, and to be the definition a hardware
backend is checked against — the visual regression tests compare *its* output.

One fast path is worth knowing about, because the numbers depend on it. The
interior of an axis-aligned opaque rectangle is *written* rather than blended: a
fully covered pixel of an opaque colour takes the same value either way, and a
per-pixel blend for the whole target is what a full repaint would otherwise cost.
That single change took a 1280×800 full frame from ~25 ms to ~2.8 ms on the
development machine, which is why the benchmark's `execute` column is now a
modest share of a frame instead of the dominant one.

---

## 10. Resources

`Include/RoboRenderer/ResourceManager.h`, `Source/RoboRenderer/Image.cpp`.

* The compiler deduplicates identical sources, so the manager's key is the
  resource index: an array, not a map, and no string comparison after startup.
* `LoadAll()` decodes everything once, at initialization, on the thread that owns
  the document; `Load(index)` does it lazily.
* A resource that fails to decode resolves to a **placeholder**, not to `nullptr`,
  so painting never branches on load state. The failure is reported as `RRE010`
  with the source, and counted in `ResourceStats().failedCount`.
* Sources are resolved against an application-named base directory, because the
  compiler records them exactly as the document wrote them.
* `Image` carries the decoders and writers the runtime needs with no third-party
  dependency: PNG (including the colour types and filters), BMP and PPM read, and
  PNG/PPM write — the write side is what the tools and the visual tests use.

---

## 11. Validation and diagnostics

`Include/RoboRenderer/UiDiagnostics.h`, `Source/RoboRenderer/CompiledUiValidation.cpp`.

Nothing the runtime cannot act on is ever silently ignored. `ValidateCompiledDocument`
runs **once, at attach time** — the runtime does no validity checking per frame.
It checks that every index is in range, every tree link is reciprocal, the root is
reachable, nothing is orphaned, the identifier table is sorted and unique, and
every category is either supported or reported. A document that fails is refused
rather than drawn incorrectly; warnings do not stop it.

Renderer diagnostics use their own code space, deliberately separate from the
compiler's `RHC` / `MHTML` / `MCSS` / `MJS` / `MSVG` / `MMATH` codes:

| Code | Meaning | Severity |
| --- | --- | --- |
| `RRE001` | invalid element id — out of range, or a non-root element with no parent | error |
| `RRE002` | invalid child/sibling index | error |
| `RRE003` | invalid resource id | error |
| `RRE004` | category outside the HTML-only runtime phase (SVG / MathML) | warning |
| `RRE005` | geometry or a numeric field is not usable layout data | warning |
| `RRE006` | the graphics backend reported a failure | error |
| `RRE007` | a binding names an event the runtime does not model | warning |
| `RRE008` | invalid string index | error |
| `RRE009` | invalid style index | error |
| `RRE010` | a resource could not be loaded or decoded | warning |
| `RRE011` | the compiled document itself is unusable | error |
| `RRE012` | two elements share one identifier, or the table is unsorted | error |
| `RRE013` | an element is not reachable from the root | error |

`RendererDiagnostics` collects entries with a hard limit (`MaxEntries = 64`) and
counts the rest as suppressed, so diagnosing a broken 100,000-element tree cannot
itself become the bottleneck. `ToString` gives every code, severity, event type,
key, paint command type and render category a stable name, so logs never depend on
an enum's numeric value.

---

## 12. The frame loop

`Include/RoboRenderer/Renderer.h`, `Source/RoboRenderer/Renderer.cpp`.

```
RenderFrame()
  UpdateRuntimeState()        caret blink, time-dependent state          (Update)
  if layout is needed         ForceLayout | UpdateLayout                 -> layoutMs
  if paint is dirty           PaintEngine::Build                         -> paintMs
  if neither ran              return: the frame cost nothing
  if debug is enabled         append the debug overlay to the same list
  Backend::BeginFrame(damage)
  for each command            Backend::Execute                           -> executeMs
  Backend::EndFrame()
```

Every step is conditional on what actually changed. A hover moves one button's
pixels; it does not relayout the page, rebuild the display list from scratch or
repaint the window. The bookkeeping that makes that possible is a handful of dirty
flags, not a diffing engine — and a frame in which nothing changed returns before
touching the backend at all.

`FrameMetrics` is produced by the run, never asserted about it, and is exactly
what the benchmark report prints: frame index and interval, `updateMs`,
`layoutMs`, `paintMs`, `executeMs`, `totalMs`, layout and paint pass counts,
element/line/command/glyph/image counts, pixels drawn, the `layoutRan`,
`paintRan` and `fullRepaint` flags, and the damaged region.

`SendEvent` routes the event, then applies the invalidation the result reported:
a scroll of the document repaints the window, an element-targeted change
invalidates the target and the element that gained or lost focus, and an event
that named neither leaves nothing to work with and falls back to a full
invalidate rather than silently repainting nothing.

---

## 13. Debug rendering

`DebugRenderOptions` draws an overlay that appends to the *same* command stream
the UI used, so a backend still executes exactly one list in exactly one order,
and a debug frame is a normal frame plus more commands.

Layers: element bounds, layout boxes, clip regions, hit-test regions, the dirty
region, element identifiers, paint command outlines and a statistics panel.

The whole feature is compiled out of a release build. `ROBO_RENDERER_DEBUG` is `1`
only when `CMAKE_BUILD_TYPE` is `Debug`; at `0` the overlay draws nothing and its
per-frame bookkeeping disappears with it. The same document, the same tool, the
same command, with and without `--debug`. Each cell is the run without the flag,
then the run with it:

| Build | `ROBO_RENDERER_DEBUG` | Commands | Frame |
| --- | --- | --- | --- |
| Release | 0 | 44 / 44 | 3.8 ms / 3.3 ms |
| Debug | 1 | 44 / 67 | 9.3 ms / 10.7 ms |

In the release build the overlay changes neither the command count nor the frame:
asking for it is asking for nothing, which is the point. In the debug build it
adds 23 commands and about 1.4 ms to that frame, so the number a debug frame
reports is a debug frame's number — and even in a debug build, a frame that needs
no work still costs nothing.

---

## 14. Tools

`Tools/` — built when `RoboTools` is enabled.

| Tool | What it does |
| --- | --- |
| `RoboViewer` | compiles one document in process and renders it to PNG (or PPM), then prints the frame metrics, the renderer diagnostics and an ASCII preview of the framebuffer |
| `RoboBench` | generates documents from 10 to 100,000 elements in five shapes, measures them, and writes a Markdown report |
| `RendererHarness` | the shared "compile a document in process" helper both tools and the renderer tests use |

The harness is the reason the tools and the tests exercise the *real* pipeline
rather than hand-written fixtures: `RoboTools::CompiledApplication::Build(html,
options, error)` runs exactly the compiler path that emits the generated C++ for
an application — parse, validate, resolve style references, build the native IR,
flatten it into compiled UI — with the tables staying in memory instead of being
written to a file.

`HarnessOptions` names the application, the document path, the companion
stylesheets and scripts, and the source directory. The source directory matters:
the compiler treats an application directory as the unit of compilation, so HTML
that references a class token is compiled the way the compiler driver would,
against every stylesheet beside the document.

```sh
# one document to an image
RoboViewer Examples/BasicApp/index.html --width 900 --height 620 --output app.png

# a full measurement pass, written as RoboBench.md
RoboBench --output RoboBench.md

# a measurement pass at one size and one shape
RoboBench --sizes 1000 --scenarios interactive --width 1920 --height 1080
```

`RoboBench` writes its image tiles into its asset directory
(`RoboBenchAssets` unless `--assets` says otherwise) and measures what a native
UI framework is actually judged on: compile, initialization, first frame, idle
frame, interactive dispatch and the frame after it, resize, text measurement and
line breaking, allocations through an instrumented global allocator, and the size
of every table the runtime keeps.

---

## 15. Tests

`Tests/RoboRenderer/` — six test files plus a shared support header, ten suites,
and a generated-UI suite driven by the compiler:

| Suite | What it covers |
| --- | --- |
| `RuntimeContract` | the compiler→renderer seam: the categories compiled data resolves to, the diagnostics produced when it cannot be trusted |
| `Style` / `Text` | category defaults, text measurement, wrapping, bold, multi-byte text |
| `Typography` | font files and the fallback chain, bidi direction, Arabic joining, cluster-aware breaking, every script rendering into its line box |
| `Layout` / `Paint` | geometry, line boxes, clipping, table layout, the display list, damage tracking |
| `Backend` / `Images` | the software rasterizer pixel for pixel, the image codecs |
| `Input` | hit testing, hover, focus, activation, text editing, scrolling |
| `Resources` | decoding, deduplication, placeholders, failure reporting |
| `Visual` | rendered frames compared against recorded reference PNGs |

`Tests/Reference/RoboRenderer/` holds the recorded reference frames. The first
run records them (and reports that it did, rather than passing); later runs
compare. The directory is gitignored on purpose — a reference frame is a local
recording, not source — and a `.gitkeep` marks the path so it survives in a
working tree.

The suite runs with the rest of the project:

```sh
build/bin/RoboHTMLCompilerTests.exe             # everything
build/bin/RoboHTMLCompilerTests.exe Visual      # filter by suite name
build/bin/RoboHTMLCompilerTests.exe Visual.ReferenceFramesMatch
```

`Tests/CMakeLists.txt` also compiles the example into a generated-UI target and
defines `ROBO_HAVE_GENERATED_UI=1`, so the renderer is tested against *generated*
compiled data, not only against data the harness built in memory.

---

## 16. Measured performance

`RoboBench.md` at the repository root is the current report; it is produced by
running `RoboBench`, not by asserting anything. On the development machine, at
1280×800 and 1×, best of 5 runs, the shape of the numbers is:

* A **frame that changes nothing costs nothing** — the runtime detects that no
  layout or paint pass is needed and returns before touching the backend. The
  report reads that back from the frame metrics rather than claiming it.
* The **first frame of a 200,000-element document** is dominated by layout and
  paint rather than by the backend: the display list is where the work is, and
  executing it is a third of the frame at worst.
* **Interactive dispatch** — routing one pointer move through hit testing and
  runtime state — stays under 0.1 ms up to 16,000 elements and under 0.5 ms at
  160,000, and the frame that follows repaints only the region the change affected.
* **Text** measures at about 1.8 µs for a ~500 character run and about 5.3 µs to
  measure and break it into lines.
* **Memory** is reported for every structure separately — compiled data, per
  element runtime state, the layout result and the display list — so a regression
  in any one of them is visible rather than averaged away.

---

## 17. Building

RoboRenderer is a normal CMake target in the same project:

```
Include/RoboRenderer/     public headers (the contract, and every engine's API)
Source/RoboRenderer/      layout, text, fonts, style, state, input, paint, backend,
                          resources, image codecs, validation, diagnostics
Tests/RoboRenderer/       the suites above
Fonts/                    the bundled Inter face the runtime defaults to
Tools/                    RoboViewer, RoboBench, RendererHarness
Docs/RoboRenderer.md      this document
```

```sh
cmake --preset release && cmake --build --preset release
build/bin/RoboHTMLCompilerTests.exe
```

`ROBO_RENDERER_DEBUG` is derived from the build type, not set by hand:
`CMAKE_BUILD_TYPE=Debug` compiles the debug overlay in, anything else compiles it
out. `RoboWarningsAsErrors` (when enabled) applies to this target like any other,
at `/W4 /permissive-` on MSVC and `-Wall -Wextra -Wpedantic` elsewhere.

An application uses the generated target:

```sh
RoboHTMLCompiler --target runtime --output build Examples/BasicApp
```

which writes `GeneratedRuntime/<App>CompiledUi.{h,cpp}` next to the other
targets, and exposes

```cpp
namespace BasicAppGenerated
{
    const RoboRenderer::CompiledDocument& CompiledUi();
    constexpr std::uint32_t CompiledElementCount = 43;
    // ...
}
```

An application then does what `RoboViewer` does: construct a renderer over that
document, initialize it with a backend and a target size, route input events, and
call `RenderFrame()`.

---

## 18. Conventions

* Files, classes and functions are `PascalCase`; variables are `camelCase`;
  constants are `PascalCase`.
* Every public header states the pipeline it sits in and the decisions it embodies
  in a comment block at the top. The comments are part of the interface here, so
  keep them true when the code moves: a stale count or a renamed stage in a header
  comment is still a bug, just one a compiler will not catch.
* The runtime never writes to compiled data. If something must change per window,
  it belongs in `UiState`.
* The runtime never compares strings on a per-frame path. If something must be
  looked up, the compiler indexes it.
* New behaviour goes in the engine that owns the question: geometry in layout,
  pixels in paint, state transitions in input, and *nothing* about HTML anywhere.
  If a change needs the runtime to know what a tag or a class was, the compiler
  should have recorded the decision instead.
