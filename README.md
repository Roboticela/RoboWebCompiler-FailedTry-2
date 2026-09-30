# RoboHTMLCompiler

A strict, registry-driven HTML / SVG / MathML → native C++ data-model compiler.

RoboHTMLCompiler reads a web application source tree and lowers it into three
parallel, inspectable targets:

* **Web target** – browser-compatible HTML plus one generated stylesheet and one
  generated behaviour script. The markup is re-parsed and re-validated by the
  compiler before it is accepted, so malformed output can never be reported as a
  successful build.
* **Native target** – strongly typed C++ that builds a `NativeElement` UI model,
  plus a machine-readable IR dump. The native runtime never parses HTML, CSS or
  JavaScript.
* **Runtime target** – the same model flattened into the compiled UI tables that
  `RoboRenderer` consumes: see [Docs/RoboRenderer.md](Docs/RoboRenderer.md).

The compiler is deliberately strict. It never repairs, reinterprets or partially
compiles source: a document either satisfies every rule or the build fails with
no output.

---

## 1. Separation of concerns

The single most important rule is that the three web languages keep their own
jobs:

| Concern   | Lives in                     | The markup may only                        |
| --------- | ---------------------------- | ------------------------------------------ |
| Structure | `*.html`                     | nest elements and reference resources      |
| Appearance| `*.css` (or Tailwind classes)| —                                          |
| Behaviour | `*.js`                       | —                                          |

Consequently the compiler rejects, with `file`, `line`, `column`, a stable error
code, a human-readable message, an explanation and a suggested fix:

* inline `style="…"` attributes,
* inline `<style>` elements,
* `on*` event-handler attributes,
* inline `<script>` content.

`class="…"` references, `data-*` metadata and external `<script src>` /
`<link rel="stylesheet">` references remain legal: they *point at* the other
languages instead of embedding them.

### Nothing is ignored

Source that the compiler does not implement is always an error. There is no
silent fallback, no "unknown, pass through" and no partial compilation:

| Source construct              | Result                                                                 |
| ----------------------------- | ---------------------------------------------------------------------- |
| Element the reference defines | Normalised into a native `ElementKind` and compiled                     |
| Element the reference removed | `RHC1004` obsolete element — registered so it can be reported precisely, never implemented |
| Attribute the reference removed | `RHC1005` obsolete attribute — same policy                            |
| Attribute that is not a handler | `RHC1005`/`MHTML003`/`MHTML005` depending on why it does not apply   |
| Anything starting with `on`   | `MHTML002` inline event-handler attribute, whether or not the reference still defines the handler |
| Element or attribute the compiler has never heard of | `MHTML004`/`MHTML005` unknown element or attribute |

The one deliberate exception is the custom element: a name that follows the
custom element naming rules (`my-component`) is not part of the reference
vocabulary, so it is accepted and lowered into a native custom category, and it
may carry its own non-standard attributes. Everything else is an error.

### Diagnostic codes

| Code                     | Meaning                                             |
| ------------------------ | --------------------------------------------------- |
| `MHTML001` (alias `RHC1007`) | inline CSS attribute                            |
| `MHTML002`               | inline event-handler attribute                      |
| `MHTML003`               | attribute not allowed on this element               |
| `MHTML004`               | unknown HTML element                                |
| `MHTML005`               | unknown attribute                                   |
| `MHTML006`               | unknown ARIA attribute                              |
| `RHC1001` / `RHC1002`    | inline `<style>` / inline `<script>` content        |
| `RHC1004` / `RHC1005`    | obsolete element / obsolete attribute               |
| `RHC1006`                | invalid custom-element name                         |
| `RHC1008`                | duplicate attribute                                 |
| `RHC1009` / `RHC1010`    | malformed markup / unexpected end tag               |
| `RHC1011` / `RHC1012`    | missing required attribute / invalid attribute value|
| `RHC1013` / `RHC1014`    | unknown `<input type>` / missing document element   |
| `MCSS001` / `MCSS002`    | unresolved style reference (warning) / malformed stylesheet |
| `MJS001` / `MJS002`      | unresolved script resource / malformed behaviour    |
| `MSVG001` / `MSVG002`    | invalid SVG attribute / unknown SVG element         |
| `MMATH001` / `MMATH002`  | invalid MathML attribute / unknown MathML element   |
| `RHC2001` / `RHC2002`    | missing source file / invalid generated output      |

The runtime has its own code space, so a log never has to be disambiguated by
which component printed it:

| Code                  | Meaning                                                        |
| --------------------- | -------------------------------------------------------------- |
| `RRE001` / `RRE002`   | invalid element id / invalid child or sibling index            |
| `RRE003` / `RRE004`   | invalid resource id / category outside the HTML-only phase     |
| `RRE005` / `RRE006`   | unusable layout data / graphics backend failure                |
| `RRE007` / `RRE008`   | unknown event binding / invalid string index                   |
| `RRE009` / `RRE010`   | invalid style index / resource failed to load or decode        |
| `RRE011` / `RRE012`   | unusable compiled document / duplicate identifier              |
| `RRE013`              | element not reachable from the root                            |

---

## 2. Pipeline

```
Examples/<App>/                     Registry/*.json        CompilerKnowledge/Vocabulary/*.json
   index.html  app.css  app.js            \                     /
        |                                  \                   /
        v                                   v                 v
   HtmlLexer ──► HtmlParser / SvgParser / MathMlParser    RegistryCompiler
        |              |                                     |
        |              v                                     v
        |        Ast.h (source vocabulary, exact spellings)  GeneratedEnums.h
        |              |                                     GeneratedRegistries.{h,cpp}
        |              v
        |        HtmlValidator / SvgValidator / MathMlValidator
        |              |
        |              v
        |        NativeTreeBuilder ──► Ir.h NativeElement tree
        |              |                    |
        |              |                    +──► NativeTargetGenerator
        |              |                    |
        |              |                    +──► CompiledUiBuilder ──► GeneratedRuntime/
        |              |
        |              v
        |        StyleSheetCompiler + TailwindUtilities + JavaScriptModuleCompiler
        |              |
        v              v
        └──────► WebTargetGenerator (renders, re-parses and re-validates its output)
```

* `Include/RoboHTMLCompiler/Ast.h` – source AST. It preserves the original
  vocabulary; normalisation happens later.
* `Include/RoboHTMLCompiler/Registry.h` – the compiler's structured knowledge,
  compiled into the binary as strongly typed tables. No JSON is parsed at run
  time. Lookups are case-insensitive (the lexer lower-cases names) while the
  canonical spelling is retained for diagnostics and generated output.
* `Include/RoboHTMLCompiler/Ir.h` – the native IR. Elements carry a compact
  `ElementKind`, typed state, typed content and typed `NativeAttribute` records
  instead of an untyped attribute dictionary.
* `Include/RoboHTMLCompiler/CompiledUiBuilder.h` – the flattening step. It turns
  the native IR into the dense, deduplicated tables `RoboRenderer` reads, and it
  is the only place the two projects' data models meet.

### The runtime

`RoboRenderer` is the native runtime and rendering system: layout, text
measurement and line breaking, hit testing, input, a typed display list and a
graphics backend. It contains no HTML or CSS parsing, no selectors and no
JavaScript, because the compiler already answered every question those would ask.
It has its own reference document:

➡ **[Docs/RoboRenderer.md](Docs/RoboRenderer.md)** – the compiled contract, the
layout, text, input, paint and backend engines, the `RRE` diagnostics, the frame
loop, the tools, the visual tests and the measured numbers.

### One category, many tags

Source tags are normalised into a small `ElementKind` enum rather than one class
per tag. `h1`–`h6` all become `Heading` (with a heading level), `section`,
`article`, `nav`, `div`, … all become `Container`, `abbr`, `code`, `em`, `span`,
… all become `Text`, and so on. Attributes route into a closed
`NativeProperty` enum on the hot path. The mapping is declared in
`Registry/ElementNormalization.json` and `Registry/AttributeRouting.json`.

### SVG and MathML are first-class namespaces

They are parsed by their own frontends (`SvgParser`, `MathMlParser`), validated
against their own registries and lowered into their own native categories
(`SvgShape`, `SvgPath`, `SvgGradient`, `MathFraction`, `MathOperator`, …).
Namespaces are case-sensitive in the source language: the lexer lower-cases every
name, and the namespace frontends restore the canonical spelling from the
registry, so `viewBox`, `linearGradient` and `definitionURL` survive intact.
`<foreignObject>` and `<annotation-xml encoding="text/html">` are HTML
integration points that hand their subtree back to the HTML vocabulary.

---

## 3. Repository layout

```
CMakeLists.txt              top-level project, shared options and output layout
CMakePresets.json           debug / release / relwithdebinfo presets
Include/RoboHTMLCompiler/   public compiler headers
Include/RoboRenderer/       public runtime headers (the compiled contract and
                            every engine's API)
Source/                     compiler implementation (lexer, parsers, validation,
                            CSS, JavaScript, native lowering, code generation, CLI)
Source/RoboRenderer/        runtime implementation (layout, text, style, state,
                            input, paint, backend, resources, codecs, validation)
Docs/RoboRenderer.md        the runtime's reference document
Docs/RoboCSSCompiler.md     the CSS compiler's reference document: the module
                            split, the diagnostics, what is implemented and
                            what is reported, and compile-time scaling
Registry/                   hand-authored vocabulary (kinds, properties,
                            normalisation, routing, ARIA, SVG, MathML, semantics)
CompilerKnowledge/          the compiler's knowledge, checked in as real code
  Vocabulary/               JSON mined from the standards reference
  Include/                  strongly typed enums + registry tables (compiled in)
  Source/                   registry tables implementation
  ImplementationCoverage.json   machine-readable coverage report
  ReferenceAudit.json       the audit trail of the consumed reference
Tools/                      SpecExtractor, RegistryCompiler, JSON helper, drivers,
                            RoboViewer, RoboWatch, RoboBench, RendererHarness
Tests/                      component, integration and renderer suites
Tests/RoboRenderer/         runtime suites (contract, style, text, layout, paint,
                            backend, images, input, resources, visual)
Fonts/                      the typefaces the runtime loads (Inter by default)
Examples/BasicApp/          a single-document HTML + CSS + JS application
Examples/MultiPage/         three documents in one folder, one model each
Examples/TypographyShowcase/ every writing system, emoji, weights and line breaking
RoboBench.md                the current benchmark report, produced by RoboBench
```

`CompilerKnowledge/` is **not** build output. It is generated once by the
tooling from the standards reference, and from then on it is source: it is
compiled into the binary, checked in, and edited only by rerunning the tools.
`Reference/HTML` and `Reference/CSS` have both been fully consumed and are gone. A
reference is used as an implementation
checklist, mined into checked-in knowledge by a tool, and then deleted module by
module as the compiler accounts for it (see below).

---

## 4. Building

Requirements: CMake ≥ 3.20, a C++17 compiler and (optionally) Ninja.

### Windows

`Tools/Build.bat` locates Visual Studio (via `VSINSTALLDIR`, `vswhere`, then a
candidate scan), imports the native x64 environment and drives CMake + Ninja:

```bat
Tools\Build.bat Release
Tools\Build.bat Debug clean
```

### POSIX hosts (Linux / macOS / MinGW)

```sh
Tools/Build.sh Release
```

### CMake presets

```sh
cmake --preset release && cmake --build --preset release
ctest --preset release
```

The build produces `build/bin/RoboHTMLCompiler.exe`, `build/bin/RoboHTMLCompilerTests.exe`
and, when the tools are enabled, `build/bin/SpecExtractor.exe`,
`build/bin/RegistryCompiler.exe`, `build/bin/RoboViewer.exe` (render one document
to an image), `build/bin/RoboWatch.exe` (the live preview, Windows only) and
`build/bin/RoboBench.exe` (measure the runtime and write a report).

---

## 4a. Live previewing a document

`Tools/Watch.bat` opens the code editor on the left half of the screen and the
live preview on the right. The preview holds the HTML in one box and the
stylesheet in the other, compiles them with the same pipeline `RoboViewer` drives,
draws the frame, and writes it as `preview.png` beside the document on every
successful build.

```bat
Tools\Watch.bat
Tools\Watch.bat Examples\Watch\index.html
Tools\Watch.bat Examples\LandingPage\index.html Examples\LandingPage\app.css
```

It builds the preview first if it has not been built, and with no arguments it
previews `Examples/Watch/index.html`. The boxes compile when they are asked to:
`F5`, the Rebuild button, or `Ctrl+S`, which writes them back to the two files
first. Typing never starts a build and a build never runs on the window thread,
so a large document or a large paste cannot stall the window; a build in flight
shows a `building...` badge over the last frame and keeps taking keystrokes.
A build that fails puts the first line of the diagnostic in the status strip and
lays the whole thing, the text the console prints, over the panel in a read-only
view that scrolls and copies, so the reason is never squeezed into one line: the
status strip only ever has room for a summary. The last good frame stays behind
it, so a half-written rule never blanks the preview.

```bat
Tools\Watch.bat Examples\Watch\index.html --auto
```

With `--auto` (or the Auto rebuild box) the window keeps up on its own: a change
in a box, or a file saved on disk by the editor, is reloaded and rebuilt after a
short pause. That is the loop for `Examples/Watch/index.html` and for small
documents; the explicit build is the default because a build per keystroke is the
wrong trade for a document large enough to notice.

`RoboWatch --once` is the same compile and render without a window: it writes the
PNG, prints the result and exits, which is how the preview can be used from a
script.

```sh
build-clang/bin/RoboWatch.exe Examples/Watch/index.html --once
```

---

## 4b. Compiling a folder of documents

`Tools/Run.bat` and `Tools/Run.sh` compile every HTML document in a folder into
its own strongly typed C++ native model. They build the compiler first if it has
not been built yet, and the only required argument is the input folder; the
output folder defaults to `build/compiled/<inputFolderName>`.

```bat
Tools\Run.bat Examples\MultiPage
Tools\Run.bat Examples\MultiPage build\compiled\MultiPage Release
```

```sh
Tools/Run.sh Examples/MultiPage
Tools/Run.sh Examples/MultiPage build/compiled/MultiPage --target native
```

One folder per document, one native model per document:

```
<output>/
  BatchReport.json                    per-document outcome, counts and diagnostics
  Index/
    GeneratedHTML/index.html          browser output keeps the document's name
    GeneratedHTML/styles.css
    GeneratedHTML/scripts.js
    GeneratedNative/IndexNativeModel.h
    GeneratedNative/IndexNativeModel.cpp
    GeneratedIR/Index.ir.json
    GeneratedReports/{CompilationReport,Diagnostics}.json
  Login/
    GeneratedNative/LoginNativeModel.{h,cpp}
    ...
```

The subdirectory name and the generated file names come from the document's
file name: `task-list.html` becomes `TaskList/` and `TaskListNativeModel.{h,cpp}`.

A document that fails to compile does not stop the others. Its diagnostics are
recorded in `BatchReport.json` and printed, the compiler exits non-zero, and the
remaining documents still produce their full output.

Each document is linked only with the behaviour modules it actually references,
so one page's script is never checked against another page's markup. Every
stylesheet in the folder is treated as the document's rule pool, exactly as in a
single-document compile.

---

## 5. Regenerating the registries

The registries are the compiler's knowledge, and they are *generated*, not
hand-maintained at run time:

```sh
# reference tree (if any)  ->  CompilerKnowledge/Vocabulary/*.json + coverage report
cmake --build build --target RoboExtractReferences

# Registry/ + CompilerKnowledge/Vocabulary/  ->  CompilerKnowledge/Include + CompilerKnowledge/Source
cmake --build build --target RoboCompileRegistries

# both, in order
cmake --build build --target RoboGenerateRegistries
```

With the reference consumed, `RoboExtractReferences` reports that and changes
nothing; `RoboCompileRegistries` remains the authoritative step.

`RegistryCompiler` validates every cross-reference while it runs: an unknown
native kind, an unknown native property, or a route for an unregistered
attribute is a hard error, so the registries cannot silently drift apart.

### Auditing and consuming the reference

The reference directory was an implementation checklist, not a runtime
dependency: it was read document by document, the vocabulary each document
defined was implemented, and the document was deleted once nothing in it was
left unaccounted for. **The checklist is finished and the directory has been
removed.** The commands below are kept because the same loop is how a new or
updated reference would be taken in.

```sh
# Report every reference document whose vocabulary is not accounted for yet
cmake --build build --target RoboAuditReferences

# Delete the revision snapshots that are fully covered
cmake --build build --target RoboConsumeReferences
```

Equivalently, straight through the tool:

```sh
SpecExtractor Reference/HTML CompilerKnowledge/Vocabulary CompilerKnowledge/ImplementationCoverage.json \
    --sweep --hand-registry Registry --audit-report CompilerKnowledge/ReferenceAudit.json
```

Add `--consume` to delete the documents that pass the audit.

**Policy.** A document is consumed only when every element and attribute it
defines is either *implemented* by the compiler or *registered as obsolete* and
rejected by it. Nothing is deleted on a guess, and obsolete vocabulary is never
implemented — see the guarantee table below. `CompilerKnowledge/ReferenceAudit.json`
is the durable audit trail: `documents` is the current state and
`consumedHistory` records every document that was consumed, with the coverage
evidence that justified it.

**Consumption log.** The reference is gone. `Reference/HTML/` held 132 files:

* `source`, the preprocessed WHATWG HTML Standard — mined into
  `CompilerKnowledge/Vocabulary/` (142 elements, 115 element attributes, 28 global
  attributes, 71 event handlers, 29 obsolete elements, 105 obsolete
  attributes).
* 17 Wattsi review drafts under `review-drafts/`, one per half-year from
  2018-07 to 2026-07. Each was audited against the compiled-in vocabulary and
  consumed; all 17 are recorded in `CompilerKnowledge/ReferenceAudit.json` under
  `consumedHistory`.
* 114 auxiliary files (the specification's own images, fonts, demo pages,
  dev scripts, CI configuration and licence). Thirteen of the demo HTML pages
  were scanned for tag and attribute vocabulary and introduced nothing the
  compiler did not already implement.

Consumption is recorded in `CompilerKnowledge/ImplementationCoverage.json`:
`referenceState` is `consumed`, `references` is empty, and `retiredReferences`
lists all 115 files that were still present when the folder was removed. The
mined vocabulary sections (`html`, `attributes`, `globalAttributes`,
`behaviour`) survive that refresh and still report zero `partial`, `pending`
and `unsupported` entries, as do SVG, MathML, ARIA and input types.

From here `CompilerKnowledge/Vocabulary/` is authoritative. `RoboExtractReferences` is a
no-op that reports the reference as consumed instead of failing, so the full
`RoboGenerateRegistries` pipeline keeps working. Dropping a new reference
tree back at `Reference/HTML/` and rerunning `RoboGenerateRegistries` picks it
up again; `RoboAuditReferences` will report whatever it defines that the
compiler does not account for yet.

### The CSS reference

`Reference/CSS` is the CSS Working Group's drafts, and the same loop applied with
a second extractor, because those drafts are Bikeshed source rather than wattsi
source:

```sh
# Mine the reference into the vocabulary, the generated header and the audit
cmake --build build --target RoboExtractCssReferences

# Retire the modules the compiler now accounts for
cmake --build build --target RoboRetireCssReferences -DRoboCssRetireModules=css-box-3
```

| file | what it is |
| --- | --- |
| `CompilerKnowledge/Vocabulary/CssVocabulary.json` | the inventory: every property, descriptor, at-rule, selector and function the reference defines, with its module and line - the vocabulary of the reference that is left, so it is empty now |
| `CompilerKnowledge/Include/RoboCSSCompiler/GeneratedCssVocabulary.h` | the names, compiled in and sorted, plus every module the reference ever had and what its retired modules defined; this is what makes a carried property a property the compiler knows instead of a hand-written list of 218 names |
| `CompilerKnowledge/CssReferenceAudit.json` | per-module counts, the inventory of every retired module, what could not be mined, and whether each module was read before it went |

**Consumption log.** The reference is gone. `Reference/CSS` held 175
directories:

* 167 specifications, every one of them mined: 151 from `Overview.bs`, and
  sixteen from the generated `Overview.html` because they ship no source, which
  is read as tables of `Key: value` rows. Between them they state 1,108 property
  definitions naming 721 distinct properties, 129 descriptors, 11 at-rules, 85
  selectors and 24 functions. All 167 modules were retired, and
  `CompilerKnowledge/CssReferenceAudit.json` carries the inventory of each.
* 8 directories that are not specifications - the drafts repository's own
  configuration, asset and tooling directories, its template module, an April
  Fools specification, and three that hold figures, a moved explainer and
  working notes - each named with the reason rather than mined.
* Four example names written in `propdef` markup that are not properties (`foo`,
  `shorthand-foo`, `display-or-not`, `background-tbd`), excluded by name with
  the reason recorded.

**Two answers.** A property is either *compiled* - the CSS front end compiles 51
names, the box, border, background, colour, font, text and overflow families the
renderer can draw - or *carried*: the compiler knows the name, the module that
defines it, whether it inherits and its initial value, and the cascade parses a
declaration of it and keeps the value on the computed style. Carrying is what a
static native UI compiler can honestly do with `animation-*`, `scroll-*` or
`writing-mode`: the author's CSS is preserved, nothing is drawn for it, and a
valid declaration is not reported as a defect. The two tables are disjoint and
together they are total over the reference, which is what let the folder be
removed: nothing the drafts defined is unaccounted for, and nothing valid is any
longer refused.

`Tests/CSS/CssReferenceCoverageTests.cpp` is the gate: every reference name is
compiled or carried, a carried name is never one the compiler also compiles,
every retired name stays accounted for once its directory is gone,
every spec citation names a real module, and the tables are sorted for the
lookups that search them. `referenceState` in the audit is `consumed` and
`modulesRemaining` is 0. Dropping the drafts back at `Reference/CSS` and
rerunning the pipeline picks them up again, and the descriptor, selector and
function names, which the audit keeps as counts, come back with them.

---

## 6. Using the compiler

```sh
RoboHTMLCompiler [options] [sourceDirectory]

  --output <directory>       Root output directory (default: build)
  --name <applicationName>   Application name (default: derived from the document)
  --all                      Compile every HTML document in the directory, each into
                             its own <output>/<DocumentName> subdirectory
  --target <web|native|runtime|all>
                             Output targets (default: all)
  --strictness <lax|strict|pedantic>
  --allow-obsolete           Accept obsolete elements and attributes
  --no-ir                    Do not emit the IR dump
  --quiet                    Do not print the success summary
  --list-registry            Print the compiled-in registry summary and exit
  --help                     Print this help and exit
```

`--all` is what `Tools/Run.bat` / `Tools/Run.sh` use; without it the compiler
compiles a single entry document (`<ApplicationName>.html`, then `index.html`,
then any `*.html`).

### Generated output

```
<output>/GeneratedHTML/<document>.html      browser-compatible HTML
<output>/GeneratedHTML/styles.css           authored rules + expanded Tailwind utilities
<output>/GeneratedHTML/scripts.js           behaviour summary
<output>/GeneratedNative/<App>NativeModel.h     typed model declaration
<output>/GeneratedNative/<App>NativeModel.cpp   typed model construction
<output>/GeneratedRuntime/<App>CompiledUi.h     compiled UI for RoboRenderer
<output>/GeneratedRuntime/<App>CompiledUi.cpp
<output>/GeneratedIR/<App>.ir.json          machine-readable native IR
<output>/GeneratedReports/CompilationReport.json
<output>/GeneratedReports/Diagnostics.json
<output>/GeneratedReports/ImplementationCoverage.json
```

In `--all` mode every document gets the same tree under
`<output>/<DocumentName>/`, plus `<output>/BatchReport.json`.

The example application is compiled end to end by the
`RoboCompileExample` target, which is what produces `build/GeneratedHTML/index.html`.

---

## 7. Tests

The suite uses a dependency-free framework and is organised by frontend so a
failure points at the component that regressed:

```
build/bin/RoboHTMLCompilerTests.exe            # or ctest --preset release
build/bin/RoboHTMLCompilerTests.exe Svg        # filter by suite
```

Compiler suites: `Registry`, `Validation` (separation + attribute rules),
`HTML` (element coverage), `SVG`, `MathML`, `CSS`, `JavaScript`, `NativeModel`
and `Integration` (end-to-end compilation of the example and of temporary
applications). `ctest` also registers `RoboHTMLCompilerExample`, which compiles
`Examples/BasicApp`.

Runtime suites: `RuntimeContract` (the compiler→renderer seam), `Style`, `Text`,
`Typography` (font loading, per-code-point fallback, bidi, Arabic joining,
cluster-aware breaking), `Layout`, `Paint`, `Backend`, `Images`, `Input`,
`Resources` and `Visual` (rendered frames compared against recorded references).
The generated-UI suite runs the renderer against compiled data the compiler
emitted, not only against data a test built in memory.

The first `Visual` run *records* its reference frames under
`Tests/Reference/RoboRenderer/` and reports that it did; later runs compare.
That directory is gitignored, because a reference frame is a local recording.

---

## 8. Conventions

* Files, classes and functions use `PascalCase`; variables use `camelCase`;
  constants use `PascalCase`.
* Registries are declarative JSON; C++ tables are generated from them and never
  edited by hand; `CompilerKnowledge/` holds everything the tooling derives from
  them, and it is checked in because it is the compiler's real knowledge.
* The compiler only ever *writes* JSON (reports, IR dumps); it never parses it
  outside the tooling.
