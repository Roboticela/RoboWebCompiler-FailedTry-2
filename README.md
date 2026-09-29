# RoboHTMLCompiler

A strict, registry-driven HTML / SVG / MathML → native C++ data-model compiler.

RoboHTMLCompiler reads a web application source tree and lowers it into two
parallel, inspectable targets:

* **Web target** – browser-compatible HTML plus one generated stylesheet and one
  generated behaviour script. The markup is re-parsed and re-validated by the
  compiler before it is accepted, so malformed output can never be reported as a
  successful build.
* **Native target** – strongly typed C++ that builds a `NativeElement` UI model,
  plus a machine-readable IR dump. The native runtime never parses HTML, CSS or
  JavaScript.

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
| `MCSS001` / `MCSS002`    | unresolved style reference / malformed stylesheet   |
| `MJS001` / `MJS002`      | unresolved script resource / malformed behaviour    |
| `MSVG001` / `MSVG002`    | invalid SVG attribute / unknown SVG element         |
| `MMATH001` / `MMATH002`  | invalid MathML attribute / unknown MathML element   |
| `RHC2001` / `RHC2002`    | missing source file / invalid generated output      |

---

## 2. Pipeline

```
Examples/<App>/                     Registry/*.json        Generated/Registry/*.json
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
        |        NativeTreeBuilder ──► Ir.h NativeElement tree ──► NativeTargetGenerator
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
Source/                     compiler implementation (lexer, parsers, validation,
                            CSS, JavaScript, native lowering, code generation, CLI)
Registry/                   hand-authored vocabulary (kinds, properties,
                            normalisation, routing, ARIA, SVG, MathML, semantics)
Generated/Registry/         JSON mined from the standards reference
Generated/Include/          generated strongly typed enums + registry tables
Generated/Source/           generated registry tables implementation
Generated/ImplementationCoverage.json  machine-readable coverage report
Tools/                      SpecExtractor, RegistryCompiler, JSON helper, build drivers
Tests/                      component and integration suites (custom framework)
Examples/BasicApp/          a working HTML + CSS + JS application
Reference/HTML/             the WHATWG HTML Standard checkout (engineering input)
```

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
and, when the tools are enabled, `build/bin/SpecExtractor.exe` and
`build/bin/RegistryCompiler.exe`.

---

## 5. Regenerating the registries

The registries are the compiler's knowledge, and they are *generated*, not
hand-maintained at run time:

```sh
# Reference/HTML/  ->  Generated/Registry/*.json + coverage report
cmake --build build --target RoboExtractReferences

# Registry/ + Generated/Registry/  ->  Generated/Include + Generated/Source
cmake --build build --target RoboCompileRegistries

# both, in order
cmake --build build --target RoboGenerateRegistries
```

`RegistryCompiler` validates every cross-reference while it runs: an unknown
native kind, an unknown native property, or a route for an unregistered
attribute is a hard error, so the registries cannot silently drift apart.

### Reference tracking policy

`Generated/ImplementationCoverage.json` records, for every file under
`Reference/HTML/`, its role and processing status. Today only
`Reference/HTML/source` (the preprocessed WHATWG HTML Standard) is a
`primary-vocabulary-source` and is marked `processed`; the remaining 131 files
are `auxiliary` and `not-compiler-input`. Reference files are **not deleted**
while they are still needed as an engineering input, and any file is only
eligible for removal once the coverage report marks it fully covered. The report
currently records zero `partial`, `pending` and `unsupported` entries for HTML,
SVG, MathML, ARIA and input types.

---

## 6. Using the compiler

```sh
RoboHTMLCompiler [options] [sourceDirectory]

  --output <directory>       Root output directory (default: build)
  --name <applicationName>   Application name (default: derived from the document)
  --target <web|native|all>  Output targets (default: all)
  --strictness <lax|strict|pedantic>
  --allow-obsolete           Accept obsolete elements and attributes
  --no-ir                    Do not emit the IR dump
  --quiet                    Do not print the success summary
  --list-registry            Print the compiled-in registry summary and exit
  --help                     Print this help and exit
```

### Generated output

```
<output>/GeneratedHTML/index.html          browser-compatible HTML
<output>/GeneratedHTML/styles.css          authored rules + expanded Tailwind utilities
<output>/GeneratedHTML/scripts.js          behaviour summary
<output>/GeneratedNative/<App>NativeModel.h    typed model declaration
<output>/GeneratedNative/<App>NativeModel.cpp  typed model construction
<output>/GeneratedIR/<App>.ir.json         machine-readable native IR
<output>/GeneratedReports/CompilationReport.json
<output>/GeneratedReports/Diagnostics.json
<output>/GeneratedReports/ImplementationCoverage.json
```

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

Suites: `Registry`, `Validation` (separation + attribute rules), `HTML`
(element coverage), `SVG`, `MathML`, `CSS`, `JavaScript`, `NativeModel` and
`Integration` (end-to-end compilation of the example and of temporary
applications). `ctest` also registers `RoboHTMLCompilerExample`, which compiles
`Examples/BasicApp`.

---

## 8. Conventions

* Files, classes and functions use `PascalCase`; variables use `camelCase`;
  constants use `PascalCase`.
* Registries are declarative JSON; C++ tables are generated from them and never
  edited by hand (`Generated/` is git-ignored and reproducible).
* The compiler only ever *writes* JSON (reports, IR dumps); it never parses it
  outside the tooling.
