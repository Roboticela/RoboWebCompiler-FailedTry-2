// ---------------------------------------------------------------------------
// RoboRenderer harness - compiling a document in process.
//
//     HTML source + CSS source
//         |
//     RoboHTMLCompiler: parse -> validate -> style references -> native IR
//         |
//     CssStyleBuilder: the CSS cascade, lowered into native styles
//         |
//     CompiledUiBuilder: flat compiled UI data
//         |
//     RoboRenderer consumes it, exactly as it consumes generated C++
//
// The viewer, the benchmark suite and the renderer tests all use this, so all
// three exercise the real compiler pipeline rather than hand written fixtures.
// Compiling in process is the same path the compiler takes when it emits the
// generated C++ for an application; the only difference is that the tables stay
// in memory instead of being written to a file.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/CompiledUiBuilder.h"
#include "RoboHTMLCompiler/Diagnostics.h"
#include "RoboHTMLCompiler/Ir.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace RoboTools
{
    struct HarnessOptions
    {
        std::string applicationName = "App";
        std::string documentPath = "App.html";

        /// Named companion sources, as file name -> contents. They are compiled
        /// the same way the compiler compiles the files beside a document.
        std::vector<std::pair<std::string, std::string>> styleSheets;
        std::vector<std::pair<std::string, std::string>> scripts;

        /// Application directory. Every stylesheet and script in it that the
        /// document names is loaded, which is how a caller compiles an example
        /// directory the way the compiler driver does.
        std::string sourceDirectory;

        /// The viewport the application targets. CSS media queries are evaluated
        /// against it and viewport units resolve against it, so a caller that
        /// renders at a different size asks for a different compilation- just as
        /// a browser would evaluate the query for the window it has.
        float viewportWidth = 1024.0f;
        float viewportHeight = 768.0f;

        /// Runs the HTML validator. The benchmarks turn it off when they only
        /// want to measure the lowering stages.
        bool validate = true;

        /// Records source file names in diagnostics.
        bool verbose = false;
    };

    /// A document compiled into the renderer's data format, with everything the
    /// compiled view points at kept alive.
    class CompiledApplication
    {
    public:
        /// Compiles a complete HTML document. Returns false and fills `error`
        /// with the compiler's formatted diagnostics when it cannot.
        bool Build(const std::string& html, const HarnessOptions& options, std::string& error);

        /// True once a document has been compiled successfully.
        bool IsValid() const { return built; }

        const RoboRenderer::CompiledDocument& Document() const { return storage.View(); }

        /// The native IR the compiled data was lowered from. The renderer never
        /// sees it; tools and tests use it to check that lowering kept what it
        /// should have.
        const RoboHTMLCompiler::NativeUiModel& Model() const { return model; }

        const RoboHTMLCompiler::NativeElement* Root() const { return model.root.get(); }

        const std::string& ApplicationName() const { return storage.ApplicationName(); }

        std::size_t ElementCount() const { return storage.Elements().size(); }

        const std::string& Diagnostics() const { return formattedDiagnostics; }

        /// The resolved styles the document carries: index 0 is "no authored
        /// style", and every other entry is one interned computed style.
        const RoboHTMLCompiler::CssStyleTable& Styles() const { return styleTable; }

        /// Compiler version the compiled data records.
        const char* CompilerVersion() const { return storage.View().compilerVersion; }

    private:
        RoboHTMLCompiler::DiagnosticSink sink;
        RoboHTMLCompiler::NativeUiModel model;
        RoboHTMLCompiler::CssStyleTable styleTable;
        RoboHTMLCompiler::CompiledUiStorage storage;
        std::string formattedDiagnostics;
        bool built = false;
    };

    /// Wraps a body fragment in a complete document, so a test can compile a
    /// snippet without repeating the envelope.
    std::string WrapFragment(const std::string& body);
}
