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
| RCC010 | A real CSS feature this compiler does not implement: an at-rule, or one of the five properties whose specification is outside the reference |
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
`background-color`, `color`, `font-size`, `font-weight`, `font-family`,
`font-style`, `line-height`, `letter-spacing`, `word-spacing`, `text-align`,
`text-indent`, `text-transform`, `white-space`, `direction`, `overflow-x`/
`overflow-y`, `all`, and the `margin`, `padding`, `border-width`, `border-style`,
`border-color`, `border`, `border-<side>`, `border-radius`, `overflow`,
`background` and `font` shorthands.

The text and font properties are the ones that reach the runtime's own text
engine rather than only the style table. `font-family` and `font-style` select a
face (a real italic when the host has one, a synthesized oblique otherwise);
`line-height`, `letter-spacing`, `word-spacing`, `text-indent`, `white-space`,
`text-transform` and `direction` are laid out and painted by the engine, so the
text a document marks up wraps, spaces, leans and orders exactly as stated.

`all` is the one property that is about the other properties rather than about
the element: it takes a CSS-wide keyword and hands it to every property at once.
It is expanded in the cascade, before inheritance is applied, so
`all: initial` is indistinguishable from a rule that sets every property to its
initial value, and `all: inherit` copies the parent's computed value onto every
property including the ones that do not inherit. It completed `css-cascade-3`,
`css-cascade-4` and `css-cascade-5`, which are the first three reference modules
this compiler has retired.

Reported rather than pretended, each with the reason:

- **Runtime-state selectors** (`:hover`, `:focus`, `:active`, `:checked` on
  runtime state, `:open`): the runtime selects one style per element, and the
  state-variant table is not compiled yet. Structural pseudo-classes
  (`:first-child`, `:nth-child`, `:not`, attribute selectors) are resolved during
  compilation and work.
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

**Where `all` and `inherit` stop.** They copy what the *cascade* computed, and
the cascade does not see the renderer's per-element base style. That base style
is this compiler's user-agent sheet - it is what makes a `<div>` a block and a
`<button>` carry a border - but it lives in the runtime and is merged in after
compilation through the style table's specified bits. So `all: inherit` on a
`<div>` whose parent says nothing about `display` copies CSS's initial `inline`,
not the renderer's `block`, and a subsequent `padding` then behaves as padding on
an inline box. Declaring the parent's `display` makes the copy match what the
renderer would have used. The boundary is stated because it is observable, and
because the alternative - making the cascade invent user-agent declarations for
properties the stylesheet never mentions - would erase the difference between
"the author said this" and "the native default applies", which is the whole point
of the specified bits.

`Reference/CSS` was the source of truth for the list, and it has been consumed:
every property it defined is now either compiled or carried, which is what
"implemented" has to mean for the parts of CSS a static native UI compiler cannot
draw. `BuildCoverageReport()` prints which names are compiled, which are carried,
and which module each one came from.

## 9. The reference, and how it was consumed

`Reference/CSS` was the CSS Working Group's drafts: one directory per module,
Bikeshed source in `Overview.bs` for most of them and only the generated
`Overview.html` for sixteen. It was an implementation checklist, not something the
compiler reads. `Tools/CssSpecExtractor.cpp` mined it in one pass and wrote the
knowledge the compiler is built against:

| | |
| --- | --- |
| `CompilerKnowledge/Vocabulary/CssVocabulary.json` | every property, descriptor, at-rule, selector and function the reference defined, with the module and line it came from - the vocabulary of the reference that is left, so it is empty now |
| `CompilerKnowledge/Include/RoboCSSCompiler/GeneratedCssVocabulary.h` | the same names, compiled in and sorted for lookup: the properties the reference still has, every module it ever had, and the properties and at-rules its retired modules defined |
| `CompilerKnowledge/CssReferenceAudit.json` | per-module counts, the inventory of each retired module, what could not be mined, and whether each module was read before it went |

```sh
cmake --build build --target RoboExtractCssReferences
```

**The inventory.** 175 directories to begin with, of which **eight are not
specifications at all** — the repository's own configuration and asset
directories, its template module (`css-module`), an April Fools specification
(`css-egg-1`), and three directories that hold figures, an explainer that has
moved and working notes rather than a draft — and each is named with the reason
rather than mined. The other **167 are specifications, and all 167 were mined**:
151 from Bikeshed source, and sixteen from the generated draft, which states the
same vocabulary as tables of rows and is read as such (`HtmlTableFields`), so no
module was ever deleted without having been read. Between them they state
**1,108 property definitions naming 721 distinct properties**, 129 descriptors,
11 at-rules, 85 selectors and 24 functions.

Four names that appear in the same `propdef` markup a real definition uses but
are examples or placeholders are excluded by name, with the reason recorded
(`foo`, `shorthand-foo`, `display-or-not`, `background-tbd`).

**Two answers, and both are answers.** A property the reference defines is either
*compiled* — a registry row with an id, a category, a grammar and effect flags,
which the cascade resolves, applies and the style builder lowers into the native
style — or *carried*: the compiler knows its name, the module that defines it,
whether it inherits and its initial value, the cascade parses a declaration of it
and keeps the value on the computed style, and nothing is reported as a defect
and nothing is lowered.

`CSSPropertyRegistry::Find` answers for both, `FindRegistered` for the carried
table alone, and the two cannot overlap: a name in both would be one property with
two identifiers. A carried property is given an id at or above
`CSSPropertyId::DynamicBase`, which is what lets a value be stored against it
without a compiled slot to index; its value grammar is `Dynamic`, the one grammar
that judges nothing.

The **51 compiled names** are the 38 longhand definitions and 14 shorthands the
renderer can actually draw — the box, border, background, colour, font, text and
overflow families. The **669 carried names** are everything else, and carrying is
not dropping: `opacity: 0.5` and `animation-name: fade` are valid CSS, so they are
accepted, kept, inherited where the specification says they inherit, and printed
back by the coverage report, instead of being refused. A compiler that models a
dozen of the 700 grammars in CSS has no business telling an author their CSS is
wrong.

**What refusal is left for.** A name that is not a property at all is still an
error (RCC001), and so is one of the five real properties whose specification
lives outside the consumed reference (`touch-action`, `font-stretch`, the
`math-*` trio), which is reported with the reason support is missing (RCC010).
That difference used to come from a hand-written list of 218 names, and 465 of the
reference's properties were reported as if they were typos.

**Carried values are kept as written.** `transition: opacity 1s ease-in 0.5s`
becomes a list of the four tokens in order, because the order is part of the
value and a later stage that learns the shorthand has to be able to read it. The
parser refuses exactly one thing: an empty value, which is not a value of any
property. `all: initial` resets the compiled properties and does not materialise
an initial value for each of the 669 carried ones — nothing lowers them, and
putting them on every element would bloat every interned style for no effect, so a
carried property is present on a computed style when a declaration put it there
and absent otherwise.

**Every module was retired.** `css-cascade-3`, `css-cascade-4` and
`css-cascade-5` went first, when implementing `all` was all that accounting for
them took. The rest went once registration made every name they defined
accounted for, so `referenceState` is `consumed` rather than `pending`, and the
unit of retirement was always the module: a module's directory is removed only
when nothing it stated is left unaccounted for.

**Retiring.** Consumption happens module by module, and the gate is a test:

```sh
cmake --build build --target RoboHTMLCompilerTests     # CssReference checks the accounting
# configure with the modules to retire, then build the target:
cmake -S . -B build -DRoboCssRetireModules=css-box-3,css-box-4
cmake --build build --target RoboRetireCssReferences
cmake -S . -B build -DRoboCssRetireModules=            # clear it again
```

`Tests/CSS/CssReferenceCoverageTests.cpp` asserts that every name the reference
ever defined is compiled or carried, that a carried name is never one the
compiler also compiles, that a carried id is its row's position in the carried
range, that the table is sorted and unique for the binary search the lookup
performs, that every name of a retired module stays accounted for once its
directory is gone, and that every spec citation in the registry — including the
ones on carried properties — names a module the reference actually had. Once that
is green for a module's contents, retiring it records it in
`CssReferenceAudit.json` and removes its directory.

Deletion is the one operation in this pipeline that can destroy knowledge, so it
is the one that needs machinery behind it. The extractor reads what it is about
to delete before it deletes it, and the detail it reads is what makes deletion
safe rather than merely tidy: a retired module's name stays in the generated
module table (a property that cites `css-cascade-3` would otherwise cite a module
the reference no longer has), its properties move to
`RetiredReferenceProperties` with the module, the inheritance and the initial
value each one stated, and its at-rules join the at-rule table. The coverage
tests then require every one of those names to *stay compiled or carried*
— not merely remembered — which is the difference between retiring a module and
quietly forgetting one. The audit carries the same inventory forward through
successive runs, so the record survives the vocabulary being regenerated, and it
records whether each module was mined before it went: a module deleted having
been read is not the same as one deleted without being looked at, and the audit
is the only place that difference can survive.

What the audit keeps as counts rather than names is the descriptors, selectors
and functions. The 129 descriptor names, 85 selectors and 24 functions are
recorded per module but were not carried into the header, which is the boundary
of what this extraction preserves. They are not lost — `w3c/csswg-drafts` is one
clone away — but they are not compiled in either, and naming the boundary is
better than implying they are inside it.

**The reference is consumed.** Every specification module was retired, so
`referenceState` is `consumed`, `modulesRemaining` is 0, and `Reference/CSS` is
gone, exactly as `Reference/HTML` was. The directory was never the compiler's
dependency: with it removed, `CssSpecExtractor` reports that there is no
reference left, leaves the vocabulary, the header and the audit untouched, and
the compiler builds against what it learned.

**What is honestly not there.** Carrying is an answer about a value, not an
implementation. This compiler is a static native UI compiler: it has no animation
clock, no scrolling, no print or paged output, no second writing direction and no
runtime that could observe a device preference. A large part of the reference
describes exactly those things (`animation-*`, `transition-*`, `scroll-*`,
`@page`, `writing-mode` and the logical aliases, `container-*`, `anchor-*`,
`view-transition-*`, `bookmark-*`, `-webkit-*`). A declaration of one is compiled
into the computed style and draws nothing: the author's CSS is preserved and the
runtime's limits are not reported as the author's mistakes.

What it does compile is the families the renderer can draw. The next real work
is moving names from the carried column to the compiled one — `opacity`,
`visibility`, `object-fit`, the table model, the list and text-decorations
families — and `BuildCoverageReport()` is where that split is printed rather than
claimed.

## 10. Tests and benchmarks

- `Tests/CSS/CssCascadeTests.cpp` - the tokenizer, the parser, typed values,
  colour conversion, `calc()` folding, and the cascade: specificity, source
  order, `!important`, inheritance, the CSS-wide keywords, shorthand expansion,
  custom properties, structural pseudo-classes, `@media`, `@supports`, carried
  properties (accepted, kept, inherited, never lowered), interning.
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
