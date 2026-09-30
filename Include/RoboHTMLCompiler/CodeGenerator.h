// ---------------------------------------------------------------------------
// RoboHTMLCompiler - code generation.
//
// Both targets originate from the same validated source model; the compiler
// never builds two unrelated interpretations of the source.
//
//     Web target:    build/GeneratedHTML/index.html, styles.css, scripts.js
//     Native target: build/GeneratedNative/*, build/GeneratedIR/*
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/Ast.h"
#include "RoboHTMLCompiler/CssStyleBuilder.h"
#include "RoboHTMLCompiler/Diagnostics.h"
#include "RoboHTMLCompiler/Ir.h"
#include "RoboHTMLCompiler/JavaScriptModule.h"
#include "RoboHTMLCompiler/StyleResolver.h"

#include <filesystem>
#include <string>
#include <vector>

namespace RoboHTMLCompiler
{
    struct WebTargetResult
    {
        std::filesystem::path documentPath;
        std::filesystem::path styleSheetPath;
        std::filesystem::path scriptPath;
        bool generated = false;
    };

    struct NativeTargetResult
    {
        std::filesystem::path headerPath;
        std::filesystem::path sourcePath;
        std::filesystem::path irDumpPath;
        bool generated = false;
    };

    struct RuntimeTargetResult
    {
        std::filesystem::path headerPath;
        std::filesystem::path sourcePath;
        std::size_t elementCount = 0;
        std::size_t resourceCount = 0;
        std::size_t stringCount = 0;
        std::size_t bindingCount = 0;
        bool generated = false;
    };

    /// Emits browser compatible HTML, CSS and JavaScript from the validated
    /// model, then re-parses and re-validates its own output before declaring
    /// success.
    class WebTargetGenerator
    {
    public:
        WebTargetGenerator(DiagnosticSink& diagnostics, const Registry& registry);

        bool Generate(const MarkupDocument& document,
                      const StyleSheetCompiler& styleSheetCompiler,
                      const JavaScriptModuleCompiler& behaviourModule,
                      const std::filesystem::path& outputDirectory,
                      const std::string& styleSheetFileName,
                      const std::string& scriptFileName,
                      WebTargetResult& result);

        /// Renders the document as browser compatible HTML.
        static std::string RenderDocument(const MarkupDocument& document,
                                          const std::string& styleSheetFileName,
                                          const std::string& scriptFileName);

    private:
        bool ValidateGeneratedDocument(const std::string& html, const std::filesystem::path& path);

        DiagnosticSink& diagnostics;
        const Registry& registry;
    };

    /// Emits a strongly typed C++ translation unit describing the native model
    /// alongside a machine-readable IR dump.
    class NativeTargetGenerator
    {
    public:
        NativeTargetGenerator(DiagnosticSink& diagnostics, const Registry& registry);

        bool Generate(const NativeUiModel& model,
                      const std::string& applicationName,
                      const std::filesystem::path& outputDirectory,
                      NativeTargetResult& result);

        static std::string RenderIrDump(const NativeUiModel& model, const std::string& applicationName);

    private:
        DiagnosticSink& diagnostics;
        const Registry& registry;
    };

    /// Emits the compiled UI as ordinary C++ data: `static constexpr` tables of
    /// the flat representation declared in RoboRenderer/CompiledUi.h.
    ///
    /// No renderer function is generated. The generated translation unit only
    /// describes the application; the reusable layout, text, paint and input
    /// algorithms live in RoboRenderer, where they are compiled once instead of
    /// once per element.
    class RuntimeTargetGenerator
    {
    public:
        RuntimeTargetGenerator(DiagnosticSink& diagnostics, const Registry& registry);

        /// `authoredStyles` is the CSS the document resolved to; it becomes the
        /// generated style table. Optional, so a caller that only wants the
        /// structure can omit it.
        bool Generate(const NativeUiModel& model,
                      const std::string& applicationName,
                      const std::filesystem::path& outputDirectory,
                      RuntimeTargetResult& result,
                      const CssStyleTable* authoredStyles = nullptr);

    private:
        DiagnosticSink& diagnostics;
        const Registry& registry;
    };
}
