// ---------------------------------------------------------------------------
// RoboHTMLCompiler - authored CSS lowered into native style data.
//
//     index.html + app.css
//            |
//            v
//     CSSElementNode tree          (the document, as a selector sees it)
//            |
//     RoboCSSCompiler              (parse, match, cascade, inherit)
//            |
//            v
//     CSSComputedStyle per element (typed, interned)
//            |
//            v
//     CompiledStyle[]              (native values, what the renderer consumes)
//
// This is the only place in the project where CSS and HTML meet, and it meets
// them through two representations neither language owns: the selector element
// view on the CSS side, and the native style table on the renderer side.
//
// The lowering is a *translation of units and structures*, never of meanings:
// a computed `1rem` becomes the logical length the runtime lays out with, a
// computed `currentColor` a resolved colour, and a computed `50%` stays a
// fraction because only the runtime knows the containing block. Nothing here
// parses a value at run time, and nothing downstream of it knows CSS.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboCSSCompiler/CssCascade.h"
#include "RoboHTMLCompiler/Ast.h"
#include "RoboHTMLCompiler/Diagnostics.h"
#include "RoboHTMLCompiler/Ir.h"
#include "RoboHTMLCompiler/StyleResolver.h"
#include "RoboRenderer/CompiledUi.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace RoboHTMLCompiler
{
    /// The kinds of native pixels per logical unit the compiler can resolve a
    /// relative length against.
    struct CssLengthContext
    {
        /// The font size of the root element, which `rem` and the initial
        /// `font-size` are relative to.
        float rootFontSize = 16.0f;

        /// The viewport the application targets. A media query is evaluated
        /// against it, and `vw`/`vh` lengths that the compiler cannot leave to the
        /// runtime are resolved against it.
        float viewportWidth = 1024.0f;
        float viewportHeight = 768.0f;
    };

    /// The compiled style table: what the renderer reads.
    struct CssStyleTable
    {
        /// Index 0 is unused, so an element's style index of 0 means "the native
        /// default", exactly as the compiled format expects.
        std::vector<RoboRenderer::CompiledStyle> styles;

        /// The `font-family` list of each style, in the same order as `styles`.
        ///
        /// A compiled style carries a font family as a document string index,
        /// and string interning belongs to the later build stage that owns the
        /// string table. Keeping the names here until then is what lets the two
        /// stages stay in their lanes: this table decides *what* family was
        /// asked for, the builder decides *where* it is stored.
        std::vector<std::string> fontFamilies;

        /// The `font-feature-settings` declaration of each style, in the same
        /// order as `styles`. Interning belongs to the later stage that owns the
        /// string table for the same reason `fontFamilies` lives here: this table
        /// decides *what* was asked for, the builder decides where it is stored.
        /// A style with no settings leaves its entry empty.
        std::vector<std::string> featureSettings;

        /// Where an element was written -> index into `styles`.
        ///
        /// The key is the source position, not an element identity, because it is
        /// the one thing the native lowering records for every element it
        /// creates (`NativeElement::source`) and the one thing the parser records
        /// for every element it reads. Two distinct elements cannot share a
        /// position, so the mapping is exact, and it survives text being split
        /// into runs.
        std::map<std::uint64_t, std::uint32_t> indexBySourcePosition;

        /// How many interned computed styles the cascade produced.
        std::size_t computedStyleCount = 0;
        /// How many elements carried a style of their own.
        std::size_t styledElementCount = 0;
        /// Declarations the cascade applied, and declarations it dropped.
        std::size_t appliedDeclarationCount = 0;
        std::size_t droppedDeclarationCount = 0;
        /// The style table's size in bytes, which is what a memory budget asks
        /// about.
        std::size_t byteSize = 0;

        /// The style index of an element written at this position, or 0.
        std::uint32_t StyleIndexAt(int line, int column) const
        {
            const auto found = indexBySourcePosition.find(PositionKey(line, column));
            return found != indexBySourcePosition.end() ? found->second : 0u;
        }

        const RoboRenderer::CompiledStyle* StyleAt(int line, int column) const
        {
            const std::uint32_t index = StyleIndexAt(line, column);
            return index != 0u && index < styles.size() ? &styles[index] : nullptr;
        }

        /// Packs a position into the map's key. The low half is the column so a
        /// document written on one line still orders.
        static std::uint64_t PositionKey(int line, int column)
        {
            return (static_cast<std::uint64_t>(line) << 32) |
                   static_cast<std::uint64_t>(static_cast<std::uint32_t>(column));
        }
    };

    /// Compiles the CSS of one document into the native style table.
    class CssStyleBuilder
    {
    public:
        explicit CssStyleBuilder(DiagnosticSink& diagnostics);

        /// Runs the cascade over `document` with every sheet in `sheets` and
        /// lowers the result. Returns false when a CSS error means the build
        /// cannot continue; unsupported features are reported as warnings and the
        /// rest of the document still compiles.
        bool Build(const MarkupDocument& document,
                   const std::vector<LoadedStyleSheet>& sheets,
                   const CssLengthContext& context,
                   CssStyleTable& out);

    private:
        /// Copies the CSS frontend's diagnostics into the compiler's, so one
        /// report carries both. A CSS problem is a compiler problem; the CSS
        /// code space (RCC) is preserved in the message, and the CSS severity
        /// decides whether the build fails.
        void Mirror(RoboCSSCompiler::CSSDiagnosticSink& cssDiagnostics, const std::string& fallbackFile);

        DiagnosticSink& diagnostics;
    };
}
