// ---------------------------------------------------------------------------
// RoboBench - the RoboRenderer benchmark suite.
//
//     Documents of 10, 100, 1,000, 10,000 and 100,000 elements, generated for
//     five shapes of UI: structure, text, images, interactive controls, mixed.
//
//     For each one it measures what a native UI framework is actually judged on:
//
//       compile             HTML -> compiled UI data (reported, not claimed)
//       initialization      attach the compiled data, load resources
//       first frame         layout + paint + execute
//       idle frame          a frame in which nothing changed
//       interactive frame   hover, click, type: dispatch + relayout + repaint
//       resize frame        a new window size, relayout and repaint
//       text measurement    measure/line-break cost on its own
//       memory              every table the runtime keeps, in bytes
//       allocations         counted through the global allocator
//
// Numbers are measured, never asserted. The report is written so that a claim
// about performance can be checked by re-running this tool.
//
//     RoboBench [--sizes 10,100,1000,10000,100000] [--scenarios static,text,images,interactive,mixed]
//               [--iterations N] [--width N] [--height N] [--assets DIR] [--output FILE]
//               [--no-report] [--help]
//
//     The image scenarios read tiles, so a run writes them to its asset directory
//     (`RoboBenchAssets` unless --assets says otherwise) before it starts.
// ---------------------------------------------------------------------------
#include "RendererHarness.h"

#include "RoboRenderer/Backend.h"
#include "RoboRenderer/Renderer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Allocation counting
//
// The global allocator is instrumented in this tool only, so "allocations per
// frame" is a measurement rather than an estimate.
// ---------------------------------------------------------------------------
namespace
{
    std::atomic<unsigned long long> AllocationCount{ 0 };

    bool CountingAllocations = false;
}

void* operator new(std::size_t size)
{
    if (CountingAllocations)
    {
        ++AllocationCount;
    }

    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr)
    {
        throw std::bad_alloc();
    }

    return memory;
}

void* operator new[](std::size_t size)
{
    if (CountingAllocations)
    {
        ++AllocationCount;
    }

    void* memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr)
    {
        throw std::bad_alloc();
    }

    return memory;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    if (CountingAllocations)
    {
        ++AllocationCount;
    }

    return std::malloc(size == 0 ? 1 : size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    if (CountingAllocations)
    {
        ++AllocationCount;
    }

    return std::malloc(size == 0 ? 1 : size);
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, const std::nothrow_t&) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, const std::nothrow_t&) noexcept
{
    std::free(memory);
}

namespace
{
    // The runtime's class carries the name of the system it implements, so the
    // benchmarks alias it rather than pulling the whole namespace in and
    // colliding with it.
    using Runtime = RoboRenderer::RoboRenderer;

    using RoboRenderer::CategoryFor;
    using RoboRenderer::CompiledBinding;
    using RoboRenderer::CompiledDocument;
    using RoboRenderer::CompiledElement;
    using RoboRenderer::CompiledIdEntry;
    using RoboRenderer::CompiledResource;
    using RoboRenderer::CompiledStyle;
    using RoboRenderer::ElementAt;
    using RoboRenderer::ElementFlagInteractive;
    using RoboRenderer::ElementIndex;
    using RoboRenderer::ElementLayout;
    using RoboRenderer::ElementState;
    using RoboRenderer::EventType;
    using RoboRenderer::FontWeightKind;
    using RoboRenderer::FrameMetrics;
    using RoboRenderer::Image;
    using RoboRenderer::InputEvent;
    using RoboRenderer::LayoutResult;
    using RoboRenderer::LineLayout;
    using RoboRenderer::MakeCheckerboardImage;
    using RoboRenderer::NextInDocumentOrder;
    using RoboRenderer::NoElement;
    using RoboRenderer::PaintCommand;
    using RoboRenderer::PaintGlyph;
    using RoboRenderer::Rect;
    using RoboRenderer::RendererOptions;
    using RoboRenderer::SoftwareBackend;
    using RoboRenderer::StringAt;
    using RoboRenderer::TextLine;
    using RoboRenderer::WritePngFile;

    struct BenchOptions
    {
        std::vector<std::uint32_t> sizes{ 10, 100, 1000, 10000, 100000 };
        std::vector<std::string> scenarios{ "static", "text", "images", "interactive", "mixed" };
        std::uint32_t iterations = 5;
        float width = 1280.0f;
        float height = 800.0f;
        std::string assetDirectory;
        std::string output = "RoboBench.md";
        bool writeReport = true;

        // The CSS sweep measures the compiler, not the runtime: a stylesheet of
        // `rules` rules over a document of `cssElements` elements. The two are
        // kept apart on purpose - a slower compile is a price, a slower frame is
        // a defect, and adding the numbers together would hide both.
        bool runCssBenchmark = false;
        std::vector<std::uint32_t> ruleCounts{ 100, 1000, 10000 };
        std::uint32_t cssElements = 1000;
        std::uint32_t cssDistinctClasses = 250;
    };

    struct CssBenchResult
    {
        std::uint32_t rules = 0;
        std::uint32_t declarations = 0;
        std::uint32_t elements = 0;
        std::uint32_t distinctClasses = 0;
        std::uint32_t compiledElements = 0;
        std::uint32_t styles = 0;

        std::size_t styleSheetBytes = 0;
        std::size_t compiledBytes = 0;

        double compileMs = 0.0;
        double plainCompileMs = 0.0;
        double cssMs = 0.0;

        bool valid = false;
        std::string failure;
    };

    struct BenchResult
    {
        std::string scenario;
        std::string shape;
        std::uint32_t requestedElements = 0;
        std::uint32_t compiledElements = 0;

        double compileMs = 0.0;
        double initializeMs = 0.0;

        double firstLayoutMs = 0.0;
        double firstPaintMs = 0.0;
        double firstExecuteMs = 0.0;
        double firstFrameMs = 0.0;

        double idleFrameMs = 0.0;
        bool idleSkippedWork = false;
        std::uint64_t idleDrawnPixels = 0;

        std::uint32_t interactiveProbes = 0;
        double interactiveDispatchMs = 0.0;
        double interactiveFrameMs = 0.0;
        double resizeMs = 0.0;

        double textMeasureUs = 0.0;
        double textWrapUs = 0.0;

        std::uint64_t allocationsFirstFrame = 0;
        std::uint64_t allocationsInteractiveFrame = 0;

        std::size_t compiledBytes = 0;
        std::size_t runtimeBytes = 0;
        std::size_t layoutBytes = 0;
        std::size_t paintBytes = 0;
        std::size_t framebufferBytes = 0;

        std::uint32_t commands = 0;
        std::uint32_t glyphs = 0;
        std::uint32_t lines = 0;
        std::uint32_t laidOutElements = 0;
        std::uint64_t drawnPixels = 0;
        std::uint64_t pixelsInTarget = 0;

        bool valid = false;
        std::string failure;
    };

    double Milliseconds(const std::chrono::steady_clock::time_point& start,
                        const std::chrono::steady_clock::time_point& end)
    {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    /// Runs a callable `iterations` times and returns the best (lowest) time:
    /// the minimum is the least noisy estimator of the work itself.
    template <typename Function>
    double BestMs(std::uint32_t iterations, Function&& function)
    {
        double best = 0.0;

        for (std::uint32_t iteration = 0; iteration < iterations; ++iteration)
        {
            const auto start = std::chrono::steady_clock::now();
            function();
            const auto end = std::chrono::steady_clock::now();

            const double elapsed = Milliseconds(start, end);
            if (iteration == 0 || elapsed < best)
            {
                best = elapsed;
            }
        }

        return best;
    }

    /// The stylesheet the generated documents reference.
    ///
    /// HTML never carries CSS: a class token is a reference that has to resolve,
    /// so the benchmark supplies the stylesheet beside the document the same way
    /// an application directory does. Keeping the classes means the benchmark
    /// measures documents whose appearance comes from a stylesheet, which is what
    /// a real application compiles.
    const char* BenchStyleSheet()
    {
        return "/* Companion stylesheet for the benchmark documents. */\n"
               ".cell {\n"
               "  display: block;\n"
               "  padding: 2px;\n"
               "  border-width: 1px;\n"
               "  border-style: solid;\n"
               "  border-color: #e2e8f0;\n"
               "}\n"
               "\n"
               ".grid {\n"
               "  display: block;\n"
               "  padding: 4px;\n"
               "}\n"
               "\n"
               ".page {\n"
               "  display: block;\n"
               "  padding: 8px;\n"
               "  background-color: #f8fafc;\n"
               "}\n";
    }

    std::string Repeat(const std::string& value, std::size_t count)
    {
        std::string result;
        result.reserve(value.size() * count);

        for (std::size_t index = 0; index < count; ++index)
        {
            result += value;
        }

        return result;
    }

    /// The document for one scenario. Element counts are approximate: the
    /// generator produces `count` elements of the scenario's characteristic kind,
    /// which is what the benchmark is about.
    std::string GenerateDocument(const std::string& scenario, std::uint32_t count)
    {
        std::ostringstream html;
        html << "<!DOCTYPE html>\n<html lang=\"en\"><head><title>Bench</title></head><body>\n";

        if (scenario == "static")
        {
            html << "<div id=\"root\">\n";

            for (std::uint32_t index = 0; index < count; ++index)
            {
                html << "<div class=\"cell\"><span>Cell " << index << "</span></div>\n";
            }

            html << "</div>\n";
        }
        else if (scenario == "text")
        {
            html << "<article>\n";

            const std::string sentence =
                "The runtime measures and breaks text at run time using compiled native data, ";

            for (std::uint32_t index = 0; index < count / 4 + 1; ++index)
            {
                html << "<p>" << Repeat(sentence, 3) << "paragraph " << index << ".</p>\n";
                html << "<h2>Heading " << index << "</h2>\n";
                html << "<li>Item " << index << "</li>\n";
                html << "<p>" << Repeat(sentence, 2) << "</p>\n";
            }

            html << "</article>\n";
        }
        else if (scenario == "images")
        {
            html << "<div class=\"grid\">\n";

            for (std::uint32_t index = 0; index < count / 2 + 1; ++index)
            {
                html << "<img src=\"tile" << (index % 8) << ".png\" alt=\"tile\" width=\"48\" height=\"36\">\n";
                html << "<figure><img src=\"tile" << (index % 8)
                     << ".png\" alt=\"tile\" width=\"64\" height=\"48\"><figcaption>Tile " << index
                     << "</figcaption></figure>\n";
            }

            html << "</div>\n";
        }
        else if (scenario == "interactive")
        {
            html << "<form id=\"bench\">\n";

            for (std::uint32_t index = 0; index < count / 5 + 1; ++index)
            {
                html << "<button type=\"button\" id=\"b" << index << "\">Button " << index << "</button>\n";
                html << "<input type=\"text\" id=\"t" << index << "\" placeholder=\"Field " << index
                     << "\" value=\"value " << index << "\">\n";
                html << "<input type=\"checkbox\" id=\"c" << index << "\" checked>\n";
                html << "<input type=\"range\" id=\"r" << index << "\" min=\"0\" max=\"100\" value=\"40\">\n";
                html << "<select id=\"s" << index << "\"><option>One</option><option selected>Two</option>"
                        "<option>Three</option></select>\n";
            }

            html << "</form>\n";
        }
        else
        {
            html << "<div class=\"page\">\n";

            for (std::uint32_t index = 0; index < count / 6 + 1; ++index)
            {
                html << "<section>\n<h2>Section " << index << "</h2>\n";
                html << "<p>" << Repeat("Mixed content for the benchmark suite. ", 3) << "</p>\n";
                html << "<button type=\"button\">Action</button>\n";
                html << "<input type=\"text\" value=\"field\">\n";
                html << "<img src=\"tile" << (index % 8) << ".png\" alt=\"tile\" width=\"40\" height=\"30\">\n";
                html << "<table><caption>Table " << index << "</caption><thead><tr><th>Key</th><th>Value</th>"
                        "</tr></thead><tbody><tr><td>a</td><td>b</td></tr></tbody></table>\n";
                html << "</section>\n";
            }

            html << "</div>\n";
        }

        html << "</body></html>\n";
        return html.str();
    }

    // Defined below the sweep, which reports the size of the table it produces.
    std::size_t CompiledBytes(const CompiledDocument& document);

    /// The document the CSS sweep styles: `count` divs, each carrying one of
    /// `classCount` class tokens, so interning has something to collapse.
    std::string GenerateCssDocument(std::uint32_t count, std::uint32_t classCount)
    {
        std::ostringstream html;
        html << "<!DOCTYPE html>\n<html lang=\"en\"><head><title>CssBench</title></head><body>\n"
             << "<div id=\"root\">\n";

        for (std::uint32_t index = 0; index < count; ++index)
        {
            // A class count of zero produces the baseline document: the same
            // structure with nothing to style, which is what the stylesheet's
            // cost is measured against.
            if (classCount == 0)
            {
                html << "<div><span>Item " << index << "</span></div>\n";
            }
            else
            {
                html << "<div class=\"c" << (index % classCount) << "\"><span>Item " << index
                     << "</span></div>\n";
            }
        }

        html << "</div>\n</body></html>\n";
        return html.str();
    }

    /// A stylesheet of `rules` rules. Only the first `classCount` classes are
    /// used by the document; the rest still have to be parsed and still have to
    /// be compared against every element, which is the work being measured.
    std::string GenerateStyleSheet(std::uint32_t rules)
    {
        std::ostringstream css;

        for (std::uint32_t index = 0; index < rules; ++index)
        {
            css << ".c" << index << " {\n"
                << "  padding: " << (1 + index % 7) << "px;\n"
                << "  border-width: 1px;\n"
                << "  border-style: solid;\n"
                << "  border-color: rgb(" << (index % 255) << ", 90, 140);\n"
                << "  background-color: #" << std::hex << (index % 16) << 'f' << (index % 8) << (index % 4)
                << std::dec << ";\n"
                << "  font-weight: " << (index % 3 == 0 ? "bold" : "normal") << ";\n"
                << "  width: " << (40 + index % 60) << "px;\n"
                << "}\n";

            // Structural and descendant selectors, so the sweep exercises the
            // matcher rather than only the declaration parser.
            if (index % 8 == 0)
            {
                css << "#root > .c" << index << " span {\n  color: #" << std::hex << (index % 16) << (index % 9)
                    << (index % 6) << std::dec << ";\n}\n";
            }
        }

        return css.str();
    }

    std::uint32_t CountDeclarations(const std::string& styleSheet)
    {
        std::uint32_t count = 0;

        for (char character : styleSheet)
        {
            if (character == ';')
            {
                ++count;
            }
        }

        return count;
    }

    /// Compiles a document with a stylesheet `iterations` times and returns the
    /// best time, so the CSS number and the HTML number are measured the same
    /// way and can be subtracted.
    double CompileOnce(const std::string& html, const std::string& styleSheet, const BenchOptions& options,
                       std::string& error, std::uint32_t* elementCount)
    {
        double best = 0.0;

        for (std::uint32_t iteration = 0; iteration < options.iterations; ++iteration)
        {
            RoboTools::CompiledApplication application;
            RoboTools::HarnessOptions harness;
            harness.applicationName = "CssBench";
            harness.styleSheets.emplace_back("cssbench.css", styleSheet);

            const auto start = std::chrono::steady_clock::now();
            const bool built = application.Build(html, harness, error);
            const double elapsed = Milliseconds(start, std::chrono::steady_clock::now());

            if (!built)
            {
                return -1.0;
            }

            if (iteration == 0 || elapsed < best)
            {
                best = elapsed;
            }

            if (elementCount != nullptr && iteration == 0)
            {
                *elementCount = application.ElementCount();
            }
        }

        return best;
    }

    /// One point of the CSS sweep.
    CssBenchResult RunCssScenario(std::uint32_t rules, const BenchOptions& options)
    {
        CssBenchResult result;
        result.rules = rules;
        result.elements = options.cssElements;

        // Every class the document uses has to be defined by the stylesheet: a
        // class token that resolves to nothing is a compile error, by design.
        result.distinctClasses = std::min(options.cssDistinctClasses, rules);
        if (result.distinctClasses == 0)
        {
            result.distinctClasses = 1;
        }

        const std::string html = GenerateCssDocument(options.cssElements, result.distinctClasses);
        const std::string styleSheet = GenerateStyleSheet(rules);

        result.styleSheetBytes = styleSheet.size();
        result.declarations = CountDeclarations(styleSheet);

        std::string error;
        result.compileMs = CompileOnce(html, styleSheet, options, error, &result.compiledElements);

        if (result.compileMs < 0.0)
        {
            result.failure = "compilation failed: " + error.substr(0, 400);
            return result;
        }

        // The baseline is the same document with no class token to resolve and
        // no stylesheet to read: what is left is the HTML work, and the
        // difference is what CSS added.
        const std::string plainHtml = GenerateCssDocument(options.cssElements, 0);
        result.plainCompileMs = CompileOnce(plainHtml, std::string(), options, error, nullptr);

        if (result.plainCompileMs < 0.0)
        {
            result.failure = "the unstyled document failed to compile: " + error.substr(0, 400);
            return result;
        }

        result.cssMs = result.compileMs - result.plainCompileMs;

        // The interned style count is read back from the compiled table: this is
        // the number of distinct computed styles the document ended up with, not
        // the number of elements that were styled.
        {
            RoboTools::CompiledApplication application;
            RoboTools::HarnessOptions harness;
            harness.applicationName = "CssBench";
            harness.styleSheets.emplace_back("cssbench.css", styleSheet);

            if (application.Build(html, harness, error))
            {
                result.styles = application.Document().styleCount;
                result.compiledBytes = CompiledBytes(application.Document());
            }
            else
            {
                result.failure = "compilation failed: " + error.substr(0, 400);
                return result;
            }
        }

        result.valid = true;
        return result;
    }

    /// Writes the tiles the image scenarios reference. Eight distinct files, so
    /// resource deduplication has something to deduplicate.
    bool WriteTiles(const std::string& directory)
    {
        for (std::uint32_t index = 0; index < 8; ++index)
        {
            Image tile = MakeCheckerboardImage(48 + index * 4, 36 + index * 4, 6 + index);

            for (std::uint32_t y = 0; y < tile.height; ++y)
            {
                for (std::uint32_t x = 0; x < tile.width; ++x)
                {
                    std::uint8_t* pixel = tile.MutablePixel(x, y);
                    pixel[0] = static_cast<std::uint8_t>((pixel[0] + index * 17) % 256);
                    pixel[1] = static_cast<std::uint8_t>((pixel[1] + index * 29) % 256);
                }
            }

            std::ostringstream path;
            path << directory << "/tile" << index << ".png";

            if (!WritePngFile(path.str().c_str(), tile))
            {
                return false;
            }
        }

        return true;
    }

    std::size_t CompiledBytes(const CompiledDocument& document)
    {
        std::size_t bytes = sizeof(CompiledDocument);
        bytes += static_cast<std::size_t>(document.elementCount) * sizeof(CompiledElement);
        bytes += static_cast<std::size_t>(document.resourceCount) * sizeof(CompiledResource);
        bytes += static_cast<std::size_t>(document.bindingCount) * sizeof(CompiledBinding);
        bytes += static_cast<std::size_t>(document.identifierCount) * sizeof(CompiledIdEntry);
        bytes += static_cast<std::size_t>(document.styleCount) * sizeof(CompiledStyle);
        bytes += static_cast<std::size_t>(document.stringCount) * sizeof(const char*);

        for (std::uint32_t index = 0; index < document.stringCount; ++index)
        {
            bytes += std::strlen(StringAt(document, index)) + 1;
        }

        return bytes;
    }

    ElementIndex NthInteractive(const CompiledDocument& document, std::uint32_t ordinal)
    {
        std::uint32_t seen = 0;

        for (ElementIndex index = document.root; index != NoElement;)
        {
            const CompiledElement* element = ElementAt(document, index);

            if (element != nullptr && element->HasFlag(ElementFlagInteractive))
            {
                if (seen == ordinal)
                {
                    return index;
                }

                ++seen;
            }

            index = NextInDocumentOrder(document, index);
        }

        return NoElement;
    }

    BenchResult RunScenario(const std::string& scenario, std::uint32_t elementCount, const BenchOptions& options)
    {
        BenchResult result;
        result.scenario = scenario;
        result.requestedElements = elementCount;

        const std::string html = GenerateDocument(scenario, elementCount);

        RoboTools::CompiledApplication application;
        RoboTools::HarnessOptions harness;
        harness.applicationName = "Bench";
        harness.styleSheets.emplace_back("bench.css", BenchStyleSheet());

        const auto compileStart = std::chrono::steady_clock::now();
        std::string error;

        if (!application.Build(html, harness, error))
        {
            result.failure = "compilation failed: " + error.substr(0, 400);
            return result;
        }

        result.compileMs = Milliseconds(compileStart, std::chrono::steady_clock::now());
        result.compiledElements = application.ElementCount();

        SoftwareBackend backend;
        Runtime renderer(application.Document());

        RendererOptions rendererOptions;
        rendererOptions.target.logicalWidth = options.width;
        rendererOptions.target.logicalHeight = options.height;
        rendererOptions.target.deviceScale = 1.0f;
        rendererOptions.assetDirectory = options.assetDirectory;

        const auto initStart = std::chrono::steady_clock::now();

        if (!renderer.Initialize(backend, rendererOptions))
        {
            result.failure = "the renderer refused the compiled data";
            return result;
        }

        result.initializeMs = Milliseconds(initStart, std::chrono::steady_clock::now());

        // --- first frame ---------------------------------------------------
        CountingAllocations = true;
        AllocationCount = 0;

        if (!renderer.RenderFrame())
        {
            CountingAllocations = false;
            result.failure = "the first frame could not be drawn";
            return result;
        }

        CountingAllocations = false;
        result.allocationsFirstFrame = AllocationCount.load();

        const FrameMetrics& first = renderer.Metrics();
        result.firstLayoutMs = first.layoutMs;
        result.firstPaintMs = first.paintMs;
        result.firstExecuteMs = first.executeMs;
        result.firstFrameMs = first.totalMs;
        result.commands = first.commandCount;
        result.glyphs = first.glyphCount;
        result.lines = first.lineCount;
        result.laidOutElements = first.elementCount;
        result.drawnPixels = first.drawnPixels;
        result.pixelsInTarget = static_cast<std::uint64_t>(backend.DeviceWidth()) * backend.DeviceHeight();

        // --- idle frame ----------------------------------------------------
        //
        // The claim is that an unchanged frame costs nothing, so the frame that
        // follows is measured and then inspected: the metrics say whether the
        // runtime did any work, which makes the claim checkable rather than
        // asserted.
        result.idleFrameMs = BestMs(options.iterations, [&]() { renderer.RenderFrame(); });
        result.idleSkippedWork = !renderer.Metrics().layoutRan && !renderer.Metrics().paintRan &&
                                 renderer.Metrics().drawnPixels == 0;
        result.idleDrawnPixels = renderer.Metrics().drawnPixels;

        // --- interactive ---------------------------------------------------
        const CompiledDocument& document = application.Document();
        const ElementIndex firstInteractive = NthInteractive(document, 0);
        const std::uint32_t interactiveProbeCount = 32;
        double dispatchTotal = 0.0;
        double frameTotal = 0.0;
        std::uint32_t probes = 0;

        for (std::uint32_t probe = 0; probe < interactiveProbeCount; ++probe)
        {
            const ElementIndex target = NthInteractive(document, probe * 97u);
            if (target == NoElement)
            {
                continue;
            }

            Rect bounds;
            if (!renderer.Hits().ElementBounds(target, bounds))
            {
                continue;
            }

            InputEvent move;
            move.type = EventType::PointerMove;
            move.x = bounds.x + bounds.width * 0.5f;
            move.y = bounds.y + bounds.height * 0.5f;

            const auto dispatchStart = std::chrono::steady_clock::now();
            renderer.SendEvent(move);
            const auto dispatchEnd = std::chrono::steady_clock::now();

            dispatchTotal += Milliseconds(dispatchStart, dispatchEnd);

            CountingAllocations = true;
            AllocationCount = 0;

            const auto frameStart = std::chrono::steady_clock::now();
            renderer.RenderFrame();
            const auto frameEnd = std::chrono::steady_clock::now();

            CountingAllocations = false;

            frameTotal += Milliseconds(frameStart, frameEnd);
            result.allocationsInteractiveFrame =
                std::max(result.allocationsInteractiveFrame, AllocationCount.load());
            ++probes;

            // A click cycle on the first interactive element, so the benchmark
            // covers activation and the relayout it can trigger.
            if (probe == 0 && firstInteractive != NoElement)
            {
                Rect firstBounds;
                if (renderer.Hits().ElementBounds(firstInteractive, firstBounds))
                {
                    InputEvent down;
                    down.type = EventType::PointerDown;
                    down.x = firstBounds.x + firstBounds.width * 0.5f;
                    down.y = firstBounds.y + firstBounds.height * 0.5f;
                    renderer.SendEvent(down);

                    InputEvent up = down;
                    up.type = EventType::PointerUp;
                    renderer.SendEvent(up);
                    renderer.RenderFrame();
                }
            }
        }

        result.interactiveProbes = probes;

        if (probes > 0)
        {
            result.interactiveDispatchMs = dispatchTotal / probes;
            result.interactiveFrameMs = frameTotal / probes;
        }

        // --- resize --------------------------------------------------------
        result.resizeMs = BestMs(1, [&]() {
            renderer.Resize(options.width + 160.0f, options.height + 100.0f);
            renderer.RenderFrame();
        });

        // --- text measurement ----------------------------------------------
        //
        // One call is a few hundred nanoseconds, which is below the resolution a
        // per call timing loop can report, so a batch is timed and divided.
        const std::string longText =
            Repeat("Measurement and line breaking are runtime work, performed natively. ", 8);
        const std::uint32_t measureBatch = 200;
        std::vector<TextLine> wrappedLines;

        result.textMeasureUs = BestMs(3, [&]() {
                                   for (std::uint32_t index = 0; index < measureBatch; ++index)
                                   {
                                       volatile float width = renderer.Layout().Text().Measure(
                                           longText.c_str(), 16.0f, FontWeightKind::Normal);
                                       (void)width;
                                   }
                               }) *
                               1000.0 / measureBatch;

        result.textWrapUs = BestMs(3, [&]() {
                                for (std::uint32_t index = 0; index < measureBatch; ++index)
                                {
                                    wrappedLines.clear();
                                    renderer.Layout().Text().Wrap(longText.c_str(), 16.0f, FontWeightKind::Normal,
                                                                  640.0f, false, wrappedLines);
                                }
                            }) *
                            1000.0 / measureBatch;

        // --- memory --------------------------------------------------------
        const LayoutResult& layoutResult = renderer.Layout().Result();

        result.compiledBytes = CompiledBytes(document);
        result.runtimeBytes = renderer.State().ElementCount() * sizeof(ElementState);
        result.layoutBytes = layoutResult.elements.size() * sizeof(ElementLayout) +
                             layoutResult.lines.size() * sizeof(LineLayout) +
                             layoutResult.elementLines.size() * sizeof(std::uint32_t);
        result.paintBytes = renderer.Paint().List().CommandCount() * sizeof(PaintCommand) +
                            renderer.Paint().List().GlyphCount() * sizeof(PaintGlyph);
        result.framebufferBytes = static_cast<std::size_t>(backend.DeviceWidth()) * backend.DeviceHeight() * 4;

        result.valid = true;
        return result;
    }

    std::string FormatBytes(std::size_t bytes)
    {
        std::ostringstream stream;

        if (bytes >= 1024u * 1024u)
        {
            stream.precision(2);
            stream << std::fixed << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MiB";
        }
        else if (bytes >= 1024u)
        {
            stream.precision(1);
            stream << std::fixed << (static_cast<double>(bytes) / 1024.0) << " KiB";
        }
        else
        {
            stream << bytes << " B";
        }

        return stream.str();
    }

    std::string FormatDouble(double value)
    {
        std::ostringstream stream;
        stream.precision(value < 1.0 ? 3 : 2);
        stream << std::fixed << value;
        return stream.str();
    }

    /// What the numbers were measured on. A benchmark report that does not say so
    /// is a report nobody can compare anything against: an unoptimized build has
    /// no NDEBUG and a debug build still has the renderer's overlay compiled in.
    std::string BuildDescription()
    {
#if defined(NDEBUG)
        const char* optimization = "optimized (NDEBUG)";
#else
        const char* optimization = "unoptimized (no NDEBUG)";
#endif

#if ROBO_RENDERER_DEBUG
        const char* overlay = "debug overlay compiled in";
#else
        const char* overlay = "debug overlay compiled out";
#endif

        return std::string(optimization) + ", " + overlay;
    }

    /// True when the numbers are not the numbers a shipped build would produce.
    bool IsMeasurementBuildUnrepresentative()
    {
#if defined(NDEBUG)
        return false;
#else
        return true;
#endif
    }

    std::string WriteReport(const std::vector<BenchResult>& results, const std::vector<CssBenchResult>& cssResults,
                            const BenchOptions& options)
    {
        std::ostringstream report;

        report << "# RoboRenderer benchmark report\n\n"
               << "Measured by `RoboBench`. Every table below is produced by running the runtime:\n"
               << "documents are compiled in process, then laid out, painted and executed by\n"
               << "`RoboRenderer` and the software backend. Times are the best of "
               << options.iterations << " runs.\n\n"
               << "Window: " << static_cast<int>(options.width) << "x" << static_cast<int>(options.height)
               << " logical units at 1x.\n\n"
               << "Build: " << BuildDescription() << ".\n\n"
               << "Documents: ";

        for (std::size_t index = 0; index < options.sizes.size(); ++index)
        {
            report << (index == 0 ? "" : ", ") << options.sizes[index];
        }

        report << " requested elements x ";

        for (std::size_t index = 0; index < options.scenarios.size(); ++index)
        {
            report << (index == 0 ? "" : ", ") << options.scenarios[index];
        }

        report << ".\n";

        if (IsMeasurementBuildUnrepresentative())
        {
            report << "\n> **These timings are from an unoptimized build and are not comparable to a\n"
                      "> release report.** Re-run `RoboBench` from a release build before quoting them.\n";
        }

        report << "\n";

        report << "## Frame cost\n\n"
               << "| scenario | elements | compile | initialize | first frame | layout | paint | execute | "
                  "idle frame | idle work | interactive dispatch | interactive frame | resize |\n"
               << "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n";

        for (const BenchResult& result : results)
        {
            if (!result.valid)
            {
                report << "| " << result.scenario << " | " << result.requestedElements
                       << " | failed | | | | | | | | | | |\n";
                continue;
            }

            // A document with no interactive element has no interactive frame to
            // report: an em dash says "not applicable", where 0.000 would read as
            // "instant".
            const std::string dispatch = result.interactiveProbes == 0
                                             ? "&mdash;"
                                             : FormatDouble(result.interactiveDispatchMs) + " ms";
            const std::string interactive = result.interactiveProbes == 0
                                                ? "&mdash;"
                                                : FormatDouble(result.interactiveFrameMs) + " ms";

            report << "| " << result.scenario << " | " << result.compiledElements << " | "
                   << FormatDouble(result.compileMs) << " ms | " << FormatDouble(result.initializeMs) << " ms | "
                   << FormatDouble(result.firstFrameMs) << " ms | " << FormatDouble(result.firstLayoutMs)
                   << " ms | " << FormatDouble(result.firstPaintMs) << " ms | "
                   << FormatDouble(result.firstExecuteMs) << " ms | " << FormatDouble(result.idleFrameMs)
                   << " ms | " << (result.idleSkippedWork ? "skipped" : "relayout + repaint") << " | " << dispatch
                   << " | " << interactive << " | " << FormatDouble(result.resizeMs) << " ms |\n";
        }

        report << "\n## Frame content and memory\n\n"
               << "`elements` here is the count the frame laid out, not the count the document\n"
               << "compiled to: the first table reports the latter.\n\n"
               << "| scenario | elements laid out | commands | glyphs | lines | pixels drawn / target | "
                  "allocations (first frame) | compiled | runtime state | layout | paint list | framebuffer |\n"
               << "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n";

        for (const BenchResult& result : results)
        {
            if (!result.valid)
            {
                continue;
            }

            report << "| " << result.scenario << " | " << result.laidOutElements << " | " << result.commands
                   << " | " << result.glyphs << " | " << result.lines << " | " << result.drawnPixels << " / "
                   << result.pixelsInTarget << " | " << result.allocationsFirstFrame << " | "
                   << FormatBytes(result.compiledBytes) << " | " << FormatBytes(result.runtimeBytes) << " | "
                   << FormatBytes(result.layoutBytes) << " | " << FormatBytes(result.paintBytes) << " | "
                   << FormatBytes(result.framebufferBytes) << " |\n";
        }

        if (!cssResults.empty())
        {
            report << "\n## CSS compilation\n\n"
                   << "The compiler's side of the pipeline, measured on a document of "
                   << options.cssElements << " elements in " << options.cssDistinctClasses
                   << " classes\nstyled by a generated stylesheet. `compile` is the whole build - parse, cascade,\n"
                      "resolve, lower - and `CSS (difference)` is that build minus the same document\n"
                      "compiled with no stylesheet, so it is the work CSS added. Both are compile\n"
                      "time: nothing here is per frame.\n\n"
                   << "| rules | declarations | elements | distinct styles | stylesheet | compile | "
                      "without CSS | CSS (difference) | per declaration | compiled |\n"
                   << "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n";

            for (const CssBenchResult& result : cssResults)
            {
                if (!result.valid)
                {
                    report << "| " << result.rules << " | " << result.declarations << " | " << result.elements
                           << " | failed | | | | | | |\n";
                    continue;
                }

                const double perDeclarationUs =
                    result.declarations == 0 ? 0.0 : result.cssMs * 1000.0 / result.declarations;

                report << "| " << result.rules << " | " << result.declarations << " | " << result.compiledElements
                       << " | " << result.styles << " | " << FormatBytes(result.styleSheetBytes) << " | "
                       << FormatDouble(result.compileMs) << " ms | " << FormatDouble(result.plainCompileMs)
                       << " ms | " << FormatDouble(result.cssMs) << " ms | " << FormatDouble(perDeclarationUs)
                       << " us | " << FormatBytes(result.compiledBytes) << " |\n";
            }

            report << "\nThe baseline behind `CSS (difference)` is the same document with its class tokens\n"
                      "removed and no stylesheet, so the column is CSS resolution and nothing else, with\n"
                      "the styled document's attribute text excluded from it.\n\n"
                   << "The `distinct styles` column is the interning result: "
                   << options.cssElements
                   << " elements, styled by a handful of class tokens, and one entry per\ndistinct computed style in the compiled table. Compiling CSS is work an application pays\n"
                      "once; it is reported here so that a change which makes the runtime faster at the\n"
                      "compiler's expense can be seen, and judged on purpose.\n";
        }

        report << "\n## Notes\n\n"
               << "- `compile` is HTML parsing, CSS resolution, validation and lowering for the whole\n"
                  "  document. It is reported because it is work the application does once, not per\n"
                  "  frame; the CSS table separates that part of it out.\n"
               << "- `idle frame` is a frame in which nothing was invalidated: the runtime detects that\n"
                  "  and does not touch the backend at all.\n"
               << "- `interactive dispatch` is the cost of routing one pointer move through hit testing\n"
                  "  and runtime state; `interactive frame` is the average frame that follows it, which\n"
                  "  repaints only the region the change affected.\n"
               << "- `runtime state` is the per element state array: the renderer keeps a fixed\n"
                  "  record for every element, whether or not the frame draws it.\n"
               << "- `layout` is the geometry of one pass and `paint list` is the display list it\n"
                  "  produced. Both scale with what the document contains and both are rebuilt\n"
                  "  whenever layout or paint is invalidated, so a regression in either is a\n"
                  "  regression in a frame.\n"
               << "- `idle work` is read back from the runtime's frame metrics: `skipped` means the\n"
                  "  idle frame reported no layout pass, no paint pass and no pixels written.\n"
               << "- Text is measured on its own, on a ~500 character run at one width: "
               << FormatDouble(results.empty() ? 0.0 : results.front().textMeasureUs) << " us to measure,\n"
               << "  " << FormatDouble(results.empty() ? 0.0 : results.front().textWrapUs)
               << " us to measure and break into lines. Both are the average of a timed batch.\n";

        return report.str();
    }
}

void PrintUsage()
{
    std::cout << "RoboBench [--sizes 10,100,1000,10000,100000]\n"
                 "          [--scenarios static,text,images,interactive,mixed]\n"
                 "          [--iterations N] [--width N] [--height N]\n"
                 "          [--assets DIR] [--output FILE] [--no-report] [--help]\n"
                 "          [--css] [--css-only] [--rules 100,1000,10000] [--css-elements N]\n";
}

int main(int argumentCount, char** argumentValues)
{
    BenchOptions options;

    for (int index = 1; index < argumentCount; ++index)
    {
        const std::string argument = argumentValues[index];

        const auto value = [&]() -> const char* {
            return index + 1 < argumentCount ? argumentValues[++index] : nullptr;
        };

        if (argument == "--sizes")
        {
            const char* list = value();
            if (list == nullptr) return 2;

            options.sizes.clear();
            std::string text(list);
            std::size_t start = 0;

            while (start <= text.size())
            {
                const std::size_t separator = text.find(',', start);
                const std::string item = text.substr(start, separator == std::string::npos ? std::string::npos
                                                                                          : separator - start);
                if (!item.empty())
                {
                    options.sizes.push_back(static_cast<std::uint32_t>(std::strtoul(item.c_str(), nullptr, 10)));
                }

                if (separator == std::string::npos)
                {
                    break;
                }

                start = separator + 1;
            }
        }
        else if (argument == "--scenarios")
        {
            const char* list = value();
            if (list == nullptr) return 2;

            options.scenarios.clear();
            std::string text(list);
            std::size_t start = 0;

            while (start <= text.size())
            {
                const std::size_t separator = text.find(',', start);
                const std::string item = text.substr(start, separator == std::string::npos ? std::string::npos
                                                                                          : separator - start);
                if (!item.empty())
                {
                    options.scenarios.push_back(item);
                }

                if (separator == std::string::npos)
                {
                    break;
                }

                start = separator + 1;
            }
        }
        else if (argument == "--iterations")
        {
            const char* number = value();
            if (number == nullptr) return 2;
            options.iterations = static_cast<std::uint32_t>(std::max(1L, std::strtol(number, nullptr, 10)));
        }
        else if (argument == "--width")
        {
            const char* number = value();
            if (number == nullptr) return 2;
            options.width = static_cast<float>(std::strtod(number, nullptr));
        }
        else if (argument == "--height")
        {
            const char* number = value();
            if (number == nullptr) return 2;
            options.height = static_cast<float>(std::strtod(number, nullptr));
        }
        else if (argument == "--assets")
        {
            const char* directory = value();
            if (directory == nullptr) return 2;
            options.assetDirectory = directory;
        }
        else if (argument == "--output")
        {
            const char* path = value();
            if (path == nullptr) return 2;
            options.output = path;
        }
        else if (argument == "--no-report")
        {
            options.writeReport = false;
        }
        else if (argument == "--css")
        {
            options.runCssBenchmark = true;
        }
        else if (argument == "--css-only")
        {
            // The CSS sweep on its own, so it can be run without paying for the
            // document sweep (which reaches 100,000 elements).
            options.runCssBenchmark = true;
            options.sizes.clear();
            options.scenarios.clear();
        }
        else if (argument == "--rules")
        {
            const char* list = value();
            if (list == nullptr) return 2;

            options.ruleCounts.clear();
            options.runCssBenchmark = true;
            std::string text(list);
            std::size_t start = 0;

            while (start <= text.size())
            {
                const std::size_t separator = text.find(',', start);
                const std::string item = text.substr(start, separator == std::string::npos ? std::string::npos
                                                                                          : separator - start);
                if (!item.empty())
                {
                    options.ruleCounts.push_back(
                        static_cast<std::uint32_t>(std::strtoul(item.c_str(), nullptr, 10)));
                }

                if (separator == std::string::npos)
                {
                    break;
                }

                start = separator + 1;
            }
        }
        else if (argument == "--css-elements")
        {
            const char* number = value();
            if (number == nullptr) return 2;
            options.cssElements =
                static_cast<std::uint32_t>(std::max(1L, std::strtol(number, nullptr, 10)));
            options.runCssBenchmark = true;
        }
        else if (argument == "--help" || argument == "-h")
        {
            PrintUsage();
            return 0;
        }
        else
        {
            std::cerr << "unknown option: " << argument << "\n";
            PrintUsage();
            return 2;
        }
    }

    // The image scenarios read tiles, so the benchmark writes them. They go to
    // their own directory rather than the working directory, because the report
    // is the deliverable and the tiles are an artifact of producing it.
    if (options.assetDirectory.empty())
    {
        options.assetDirectory = "RoboBenchAssets";
    }

    std::error_code directoryError;
    std::filesystem::create_directories(options.assetDirectory, directoryError);

    if (!WriteTiles(options.assetDirectory))
    {
        std::cerr << "cannot write benchmark tiles to " << options.assetDirectory << "\n";
        return 1;
    }

    std::cout << "RoboBench\n=========\nwindow " << static_cast<int>(options.width) << "x"
              << static_cast<int>(options.height) << ", best of " << options.iterations << " runs, "
              << options.sizes.size() << " sizes x " << options.scenarios.size() << " scenarios\nbuild "
              << BuildDescription() << "\n\n";

    std::vector<BenchResult> results;
    std::vector<CssBenchResult> cssResults;

    // The CSS sweep runs first: it is the cheap one, so a long document sweep
    // does not hide its output behind minutes of rendering.
    for (std::uint32_t rules : options.ruleCounts)
    {
        if (!options.runCssBenchmark)
        {
            break;
        }

        const CssBenchResult result = RunCssScenario(rules, options);
        cssResults.push_back(result);

        if (!result.valid)
        {
            std::cout << "  css " << rules << " rules ... failed: " << result.failure << "\n";
            continue;
        }

        std::cout << "  css " << result.rules << " rules, " << result.declarations << " declarations, "
                  << result.compiledElements << " elements: compile " << FormatDouble(result.compileMs)
                  << " ms (without CSS " << FormatDouble(result.plainCompileMs) << " ms, CSS "
                  << FormatDouble(result.cssMs) << " ms), " << result.styles << " distinct styles\n";
    }

    for (const std::string& scenario : options.scenarios)
    {
        for (std::uint32_t size : options.sizes)
        {
            const BenchResult result = RunScenario(scenario, size, options);
            results.push_back(result);

            if (!result.valid)
            {
                std::cout << "  " << scenario << " " << size << " ... failed: " << result.failure << "\n";
                continue;
            }

            std::cout << "  " << scenario << " " << result.compiledElements << " elements: first frame "
                      << FormatDouble(result.firstFrameMs) << " ms (layout " << FormatDouble(result.firstLayoutMs)
                      << ", paint " << FormatDouble(result.firstPaintMs) << ", execute "
                      << FormatDouble(result.firstExecuteMs) << "), idle " << FormatDouble(result.idleFrameMs)
                      << " ms (" << (result.idleSkippedWork ? "work skipped" : "reworked") << "), interactive ";

            if (result.interactiveProbes == 0)
            {
                std::cout << "n/a";
            }
            else
            {
                std::cout << FormatDouble(result.interactiveFrameMs) << " ms";
            }

            std::cout << ", " << result.commands << " commands, " << result.allocationsFirstFrame
                      << " allocations\n";
        }
    }

    if (options.writeReport)
    {
        const std::string report = WriteReport(results, cssResults, options);
        std::ofstream stream(options.output, std::ios::binary | std::ios::trunc);

        if (!stream)
        {
            std::cerr << "cannot write the report to " << options.output << "\n";
            return 1;
        }

        stream << report;
        std::cout << "\nreport written to " << options.output << "\n";
    }

    return 0;
}
