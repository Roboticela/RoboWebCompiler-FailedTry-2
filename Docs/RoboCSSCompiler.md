# RoboCSSCompiler

The CSS frontend. It reads the CSS a project ships, resolves it against the
document the compiler has already built, and publishes *native* style values to
RoboRenderer.

```text
    index.html + app.css
            |
            v
    RoboCSSCompiler                 parse, match, cascade, inherit
            |
            v
    one computed style per element  typed values, interned
            |
            v
    native style table              CompiledStyle[] - what layout and paint read
            |
            v
    RoboRenderer                    percentages, viewport units, layout, text, paint
```

The rule the whole design follows:

> RoboHTMLCompiler understands CSS. RoboRenderer understands compiled native
> style data.

CSS is a **source language**. It is parsed and resolved once, during the build.
No selector is matched, no declaration is read and no property is looked up by
name at run time.

## 1. Why the split is where it is

A browser re-does its style work whenever something invalidates it: it matches
selectors, collects declarations, sorts them, inherits, expands shorthands and
parses values - per element, per invalidation. For a compiled application none of
that depends on anything the build does not already know. Only two things do:

- **what the runtime size is** - a percentage or a viewport unit is not a length
  until layout knows the containing block or the window;
- **what the user is doing** - hover, focus, press, a disclosure being open.

So the compiler resolves everything else, and what it cannot resolve it stores
*semantically*: `50%` stays a fraction, `50vw` stays a fraction of the viewport,
`currentColor` is resolved to the element's colour, and `1rem` is resolved to the
root font size, because that is a compile-time fact.

## 2. Modules

| Module | What it is |
| --- | --- |
| `CssSyntax` | CSS Syntax Level 3 tokenizer: escapes, numeric forms with exponents, dimensions, strings, urls, comments, CDO/CDC |
| `CssParser` | Rules, at-rules with nested rules, declarations, and component values - token preserving, which is what makes `var()` and `calc()` possible |
| `CssProperty` | The property registry: id, name, category, inheritance, initial value, value grammar, and effect flags (`affectsLayout`, `affectsPaint`, `affectsText`, `affectsHitTesting`) |
| `CssValue` | Typed values. Units keep their family *and their context*; colours are native; `calc()` is a flat program |
| `CssValueParser` | One parser per grammar; colour functions; `calc`/`min`/`max`/`clamp`; `var()` substitution |
| `CssSelector` | Selector parser, `an+b`, attribute operators, structural and state pseudo-classes, specificity, and a matcher over a plain element view |
| `CssCascade` | Matching, cascade order, inheritance, CSS-wide keywords, shorthand expansion, style interning, coverage |
| `CssStyleBuilder` *(in RoboHTMLCompiler)* | The lowering: computed styles to `CompiledStyle[]`, and the CSS diagnostics to compiler diagnostics |

The element view (`CSSElementNode`) is the only interface between the two
languages: the compiler fills it in from its own AST, the selector matcher reads
it. CSS never parses HTML; HTML never parses CSS.

## 3. Typed values, not strings

`10px 20%` is a list of a dimension and a percentage. Nothing is a string after
parsing, and a value that needs a runtime context is *marked* as needing one:

| What the stylesheet wrote | What the compiler stores |
| --- | --- |
| `1rem`, `12px`, `0.5in` | an absolute length in logical units |
| `1em` | an absolute length, resolved against the element's own font size (its computed value) |
| `50%` | a fraction, flagged `ParentRelative` |
| `50vw` | a fraction of the viewport, flagged `ViewportRelative` |
| `calc(100px - 20px)` | folded to `80px` at compile time |
| `calc(100% - 20px)` | reported: a native length carries one dimension |
| `currentColor` | the element's resolved colour, or reported when the colour is the native default |
| `oklch(60% 0.2 30)` | sRGB, converted during compilation |
| `var(--accent)` | the custom property's tokens, substituted before the grammar sees them |

Units carry their family (length, angle, time, resolution) and their context
(absolute, font relative, root font relative, viewport relative, container
relative). `ex`, `ch`, `lh` and the container units are recognised and reported
as unusable in a native box *for the reason they are unusable*, not as a syntax
error.

## 4. The cascade

Per element, in source order:

1. **Custom properties** first - inherited as a whole, then the element's own.
   `var()` is substituted on the component values, which is where the
   specification puts it: a custom property's value is a token stream.
2. **Declarations**, sorted by origin and importance, then specificity, then
   source order. The last writer of a property wins.
3. **CSS-wide keywords**: `inherit`, `initial` (the registry's initial value,
   parsed by the same parser a declaration uses), `unset`, `revert`,
   `revert-layer` (equivalent to `unset` while the compiler has no `@layer`).
4. **Inheritance** for whatever the element did not set, over the registry's
   inherited property list.

Shorthands are expanded during the cascade, from the registry's own table:
`margin: 0 auto` becomes four longhands, `border: solid 2px red` becomes width,
style and colour for all four sides in any order, `font: bold 1rem/1.4 sans-serif`
becomes size, line height and weight.

Two elements with the same computed style share one entry
(`CSSComputedStyle::Key()` is the interning key), and the lowering interns again
over the native values, because `margin: 0` and `margin: 0px` are the same box.

### Static and runtime, in this implementation

| Compile time | Run time |
| --- | --- |
| property identification, value parsing, colour conversion | percentage resolution against the containing block |
| selector parsing and matching, specificity, source order | viewport unit resolution against the window |
| the cascade, inheritance, initial values | the containing block's size, and therefore every used length |
| shorthand expansion, `var()` substitution | font measurement, text wrapping, line boxes |
| `@media` and `@supports` evaluation against the target viewport | hover, focus, press, caret, scroll |
| interning, deduplication, the style table | |

## 5. At-rules

| At-rule | Treatment |
| --- | --- |
| `@media` | Parsed and **evaluated during compilation** against the application's target viewport (`width`, `height`, `min-`/`max-`, `orientation`). A feature that needs a runtime environment - `pointer`, `hover`, `prefers-color-scheme`, `resolution`, `dynamic-range` - is reported as undecidable rather than assumed. |
| `@supports` | Answered by running the *real* value parser: `@supports (display: grid)` is false because this compiler does not implement grid. Conditions are plain `(property: value)` conjunctions and disjunctions. |
| `@layer`, `@import`, `@font-face`, `@keyframes`, `@container`, `@namespace`, `@page` | Reported as unimplemented (RCC010). They belong to features - cascade layers, resource loading, animation, container queries - that the runtime does not have yet. |

## 6. Diagnostics

CSS problems are CSS problems, so they have their own code space, and the
compiler's report carries them through unchanged (`MCSS002: RCC002: ...` at the
stylesheet's own line and column).

| Code | Meaning |
| --- | --- |
| RCC001 | Unknown property |
| RCC002 | Invalid value for the property's grammar |
| RCC003 | Invalid selector |
| RCC004 | Invalid declaration |
| RCC005 | Invalid function (`rgb()`, `calc()` and the rest) |
| RCC006 | Invalid or unusable unit |
| RCC007 | Invalid or unknown at-rule |
| RCC008 | The value does not fit the property's grammar shape |
| RCC009 | A shorthand's values are contradictory |
| RCC010 | A feature the reference defines and this compiler does not implement yet |
| RCC011 | Malformed stylesheet syntax |
| RCC012 | Text between tokens that means nothing (an empty rule) |

An error fails the build. A warning is reported and the rest of the document
still compiles - which is the difference between "this value is not legal" and
"this value is legal and this runtime cannot honour it".

## 7. The runtime side

The compiler writes a `CompiledStyle[]` table. Each entry is **sparse**: a
`specified` bit mask names the fields the stylesheet actually decided, and the
native default fills everything else - which is exactly what a user-agent
stylesheet means in a browser. So:

- `margin-top: 8px` sets one edge and leaves the other three to the native
  default;
- a stylesheet that sets only a colour leaves a control's border, font and
  geometry untouched;
- an element whose selectors matched nothing carries style index 0 and is
  answered entirely by the native default for its category.

`StyleTableProvider` composes the two: the native default for the element's
category, adjusted by the element's own nature, with the authored fields laid
over it. That is the entire CSS involvement of RoboRenderer.

Relative lengths are resolved by the layout engine, at the only place that knows
the answer: `ResolveDeclaredLength` turns `ParentRelative` into a fraction of the
containing block and `ViewportRelative` into a fraction of the window, and it is
called for width, height, minimum width and minimum height.

## 8. What is implemented, and what is reported

Implemented end to end (compiled, lowered, applied, tested): `display`, `width`,
`height`, `min-width`, `min-height`, the four margins and paddings, the four
border widths, `border-style`, the four border colours, `border-radius`,
`background-color`, `color`, `font-size`, `font-weight`, `letter-spacing`,
`text-align`, `overflow-x`/`overflow-y`, and the `margin`, `padding`,
`border-width`, `border-style`, `border-color`, `border`, `border-<side>`,
`border-radius`, `overflow`, `background` and `font` shorthands.

Reported rather than pretended, each with the reason:

- **Runtime-state selectors** (`:hover`, `:focus`, `:active`, `:checked` on
  runtime state, `:open`): the runtime selects one style per element, and the
  state-variant table is not compiled yet. Structural pseudo-classes
  (`:first-child`, `:nth-child`, `:not`, attribute selectors) are resolved during
  compilation and work.
- **`line-height`**: the text engine derives a line's height from the font's
  metrics and the compiled style has no field that overrides it. Compiling the
  value into a field nothing reads would be support in name only.
- **`max-width`/`max-height`**: the native box model carries a minimum and no
  maximum.
- **Percentages and viewport units in margin, padding, border width and
  `font-size`**: the native box model holds resolved lengths, and the window can
  be resized after compilation. They *are* supported in `width`, `height` and
  `min-*`, where the layout engine resolves them.
- **`margin: auto`**: the native layout engine does not distribute free space.
- **Grid, Flexbox, transforms, transitions, animations, filters, gradients,
  background images, custom fonts, logical properties, container queries**: the
  reference defines them and this compiler does not implement them.
- **System colours** (`CanvasText` and friends): a native runtime has one theme,
  and the compiler does not invent a palette. `color`'s initial value is CSS
  2.1's black for the same reason.
- **Utility-class expansion**: the class-token mechanism (Tailwind-style
  utilities) resolves for the web target and is not lowered into native styles.

`Reference/CSS` is the source of truth for the list, and a reference item is
retired only when its feature is parsed, categorised, implemented, tested and
integrated. `BuildCoverageReport()` prints what is implemented, what is not, and
which reference property each unimplemented name belongs to.

## 9. Tests and benchmarks

- `Tests/CSS/CssCascadeTests.cpp` - the tokenizer, the parser, typed values,
  colour conversion, `calc()` folding, and the cascade: specificity, source
  order, `!important`, inheritance, the CSS-wide keywords, shorthand expansion,
  custom properties, structural pseudo-classes, `@media`, `@supports`,
  unsupported-property reporting, interning.
- `Tests/Integration/CssPipelineTests.cpp` - the whole pipeline, asserted on
  what a user can see: the compiled table's contents, box geometry, a percentage
  and a viewport unit resolving at runtime, a minimum width, a background colour
  in the pixels and in the paint command, specificity, invalid CSS failing the
  build, and unimplemented CSS being reported while the rest applies.
- `RoboBench` measures compile, initialize, first frame, layout, paint, execute,
  idle, interactive dispatch, resize, commands, glyphs, pixels and per-stage
  memory for five document shapes. Compile time is reported separately from frame
  time: a slower compiler is an acceptable price for a faster runtime, and the
  two numbers must never be read as one.

### Compile-time scaling

`RoboBench --css` measures the compiler against a generated stylesheet:
`--rules 100,1000,10000` sets the stylesheet's size, `--css-elements N` the
document's, and `--css-only` skips the frame scenarios so a scaling question can
be answered without waiting for a full sweep. Every number below is the CSS
portion of a build (the whole build minus the same document with no stylesheet),
best of three, from a Release clang 18 build. The MSVC build of the same sources
measures within about 10% of these numbers; the shape is the table's point, not
the third digit.

The cost is two products and they must be read separately:

- **declarations x parsing**, from the stylesheet. A stylesheet that is ten times
  larger costs about ten times more, at roughly 5 us per declaration.
- **elements x rules**, from matching. Every element is tested against every
  rule, so a document that is ten times larger costs about ten times more at a
  fixed stylesheet, at roughly 0.5 us per test.

| declarations | rules | elements | CSS (difference) |
| --- | --- | --- | --- |
| 100 | 14 | 10 | 1.32 ms |
| 998 | 140 | 10 | 6.66 ms |
| 9 975 | 1 400 | 10 | 51.25 ms |
| 99 750 | 14 000 | 10 | 515.91 ms |

| rules | elements | CSS (difference) |
| --- | --- | --- |
| 50 | 202 | 7.47 ms |
| 50 | 2 002 | 53.24 ms |
| 50 | 20 002 | 540.31 ms |

The benchmark earned its keep before it was believed. 1,000/2,000/4,000/8,000
rules first measured 79/222/705/2,496 ms: doubling the rules more than tripled
the time, which is not a constant per rule, and three real defects were behind
it. The same sweep now measures 37/73/144/294 ms - a clean doubling, and 8.5x
faster at the top.

1. `StyleSheetCompiler` found a rule's line number by counting newlines from the
   start of the sheet for every rule it reported, which is a scan of the sheet
   per rule. Line starts are now recorded once per sheet and answered with a
   binary search.
2. The same class searched for `/*` from the current position on every
   iteration. On a sheet with no comments, that search runs to the end of the
   file and finds nothing - every time. The last answer is now reused while it
   still lies ahead of the cursor.
3. `CSSParser` built each declaration's value text twice - once to ask whether
   the value was empty, once to store it - and stored it in a field nothing
   read. Emptiness is now answered by looking at the tokens, and the field is
   gone.

The tokenizer also reserved its token vector up front. A `CSSToken` carries its
location, decoded text and raw spelling, so a large sheet's token vector is tens
of megabytes, and growing it a doubling at a time was most of what tokenizing
cost: 162 ms of a 267 ms parse for 9000 rules became 74 ms.

What is left is linear and honest about where it goes: tokenizing the sheet is
about a third of it and turning tokens into component values another third, both
of which are proportional to the sheet. Neither is a per-frame cost, and the
runtime receives typed data either way.
