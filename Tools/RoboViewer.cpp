// ---------------------------------------------------------------------------
// RoboViewer - renders a document with RoboRenderer and writes the frame.
//
//     HTML file
//        |
//     RoboHTMLCompiler (in process: exactly the pipeline that emits the
//        |              generated C++ tables for an application)
//        v
//     Compiled UI data
//        |
//     RoboRenderer + SoftwareBackend
//        |
//     PNG
//
// It is the fastest way to see what the runtime does with a document, and it is
// what the visual regression tests were recorded with.
//
//     RoboViewer <file.html> [options]
//
//     --width N        logical width            (default 900)
//     --height N       logical height           (default 620)
//     --scale S        device scale             (default 1)
//     --output FILE    image to write           (default <document>.png)
//     --ppm            write PPM instead of PNG
//     --assets DIR     directory resources resolve against (default the
//                      document's own directory)
//     --hover N        hover element N before the frame
//     --click N        click element N before the frame
//     --type TEXT      type text into the focused control before the frame
//     --scroll Y       scroll the document by Y before the frame
//     --resize WxH     resize to a second size and render a second image
//     --debug          draw the debug overlay (ROBO_RENDERER_DEBUG builds)
//     --quiet          print only the output path
// ---------------------------------------------------------------------------
#include "RendererHarness.h"

#include "RoboRenderer/Renderer.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    // The runtime's class carries the name of the system it implements, so the
    // tools alias it rather than pulling the whole namespace in and colliding
    // with it.
    using Runtime = RoboRenderer::RoboRenderer;

    using RoboRenderer::CategoryFor;
    using RoboRenderer::Color;
    using RoboRenderer::CompiledDocument;
    using RoboRenderer::CompiledElement;
    using RoboRenderer::ElementAt;
    using RoboRenderer::ElementFlagInteractive;
    using RoboRenderer::ElementIndex;
    using RoboRenderer::EventType;
    using RoboRenderer::FrameMetrics;
    using RoboRenderer::InputEvent;
    using RoboRenderer::NextInDocumentOrder;
    using RoboRenderer::NoElement;
    using RoboRenderer::Rect;
    using RoboRenderer::RenderCategory;
    using RoboRenderer::RendererOptions;
    using RoboRenderer::SoftwareBackend;
    using RoboRenderer::WritePngFile;
    using RoboRenderer::WritePpmFile;

    struct ViewerOptions
    {
        std::string input;
        std::string output;
        std::string assetDirectory;
        float width = 900.0f;
        float height = 620.0f;
        float deviceScale = 1.0f;
        bool writePpm = false;
        bool debug = false;
        bool quiet = false;
        bool ascii = false;
        bool probe = false;
        std::uint32_t probeX = 0;
        std::uint32_t probeY = 0;
        bool hasResize = false;
        float resizeWidth = 0.0f;
        float resizeHeight = 0.0f;
        bool hasHover = false;
        bool hasClick = false;
        bool hasScroll = false;
        std::uint32_t hoverIndex = 0;
        std::uint32_t clickIndex = 0;
        float scrollY = 0.0f;
        std::string typeText;
    };

    void PrintUsage()
    {
        std::cout << "RoboViewer <file.html> [--width N] [--height N] [--scale S] [--output FILE]\n"
                     "           [--ppm] [--assets DIR] [--hover N] [--click N] [--type TEXT]\n"
                     "           [--scroll Y] [--resize WxH] [--debug] [--quiet]\n";
    }

    bool ReadFile(const std::string& path, std::string& contents)
    {
        std::ifstream stream(path, std::ios::binary);

        if (!stream)
        {
            return false;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        contents = buffer.str();
        return true;
    }

    std::string DirectoryOf(const std::string& path)
    {
        const std::size_t separator = path.find_last_of("/\\");
        return separator == std::string::npos ? std::string(".") : path.substr(0, separator);
    }

    std::string ReplaceExtension(const std::string& path, const char* extension)
    {
        const std::size_t separator = path.find_last_of("/\\");
        const std::size_t dot = path.find_last_of('.');

        if (dot == std::string::npos || (separator != std::string::npos && dot < separator))
        {
            return path + extension;
        }

        return path.substr(0, dot) + extension;
    }

    /// Finds the Nth element of a category in document order, which is how the
    /// pointer options name a target without the caller knowing indices.
    ElementIndex FindNth(const CompiledDocument& document, RenderCategory category, std::uint32_t ordinal)
    {
        std::uint32_t seen = 0;

        for (ElementIndex index = document.root; index != NoElement;)
        {
            const CompiledElement* element = ElementAt(document, index);

            if (element != nullptr && CategoryFor(element->kind) == category)
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

    /// Any interactive element, so --click works without naming a category.
    ElementIndex FindNthInteractive(const CompiledDocument& document, std::uint32_t ordinal)
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

    void CenterOf(Runtime& renderer, ElementIndex index, float& x, float& y)
    {
        Rect bounds;

        if (renderer.Hits().ElementBounds(index, bounds))
        {
            x = bounds.x + bounds.width * 0.5f;
            y = bounds.y + bounds.height * 0.5f;
            return;
        }

        x = 0.0f;
        y = 0.0f;
    }

    bool ParseNumber(const char* text, float& value)
    {
        if (text == nullptr)
        {
            return false;
        }

        char* end = nullptr;
        const double parsed = std::strtod(text, &end);
        value = static_cast<float>(parsed);
        return end != nullptr && *end == '\0';
    }

    bool ParseSize(const char* text, float& width, float& height)
    {
        if (text == nullptr)
        {
            return false;
        }

        const std::string value(text);
        const std::size_t separator = value.find_first_of("xX*");

        if (separator == std::string::npos)
        {
            return false;
        }

        float parsedWidth = 0.0f;
        float parsedHeight = 0.0f;

        if (!ParseNumber(value.substr(0, separator).c_str(), parsedWidth) ||
            !ParseNumber(value.substr(separator + 1).c_str(), parsedHeight))
        {
            return false;
        }

        width = parsedWidth;
        height = parsedHeight;
        return true;
    }

    bool ParseArguments(int argumentCount, char** argumentValues, ViewerOptions& options)
    {
        for (int index = 1; index < argumentCount; ++index)
        {
            const std::string argument = argumentValues[index];

            const auto next = [&](float& value) {
                return index + 1 < argumentCount && ParseNumber(argumentValues[++index], value);
            };

            if (argument == "--width")
            {
                if (!next(options.width)) return false;
            }
            else if (argument == "--height")
            {
                if (!next(options.height)) return false;
            }
            else if (argument == "--scale")
            {
                if (!next(options.deviceScale)) return false;
            }
            else if (argument == "--scroll")
            {
                if (!next(options.scrollY)) return false;
                options.hasScroll = true;
            }
            else if (argument == "--resize")
            {
                if (index + 1 >= argumentCount) return false;
                if (!ParseSize(argumentValues[++index], options.resizeWidth, options.resizeHeight)) return false;
                options.hasResize = true;
            }
            else if (argument == "--output" || argument == "--assets" || argument == "--type")
            {
                if (index + 1 >= argumentCount) return false;
                const std::string value = argumentValues[++index];

                if (argument == "--output") options.output = value;
                else if (argument == "--assets") options.assetDirectory = value;
                else options.typeText = value;
            }
            else if (argument == "--hover" || argument == "--click")
            {
                if (index + 1 >= argumentCount) return false;

                float value = 0.0f;
                if (!ParseNumber(argumentValues[++index], value)) return false;

                if (argument == "--hover")
                {
                    options.hoverIndex = static_cast<std::uint32_t>(value);
                    options.hasHover = true;
                }
                else
                {
                    options.clickIndex = static_cast<std::uint32_t>(value);
                    options.hasClick = true;
                }
            }
            else if (argument == "--ppm")
            {
                options.writePpm = true;
            }
            else if (argument == "--debug")
            {
                options.debug = true;
            }
            else if (argument == "--quiet")
            {
                options.quiet = true;
            }
            else if (argument == "--ascii")
            {
                options.ascii = true;
            }
            else if (argument == "--probe")
            {
                if (index + 1 >= argumentCount) return false;

                float probeX = 0.0f;
                float probeY = 0.0f;

                if (!ParseSize(argumentValues[++index], probeX, probeY)) return false;

                options.probeX = static_cast<std::uint32_t>(probeX);
                options.probeY = static_cast<std::uint32_t>(probeY);
                options.probe = true;
            }
            else if (argument == "--quiet-options")
            {
                options.quiet = true;
            }
            else if (!argument.empty() && argument[0] == '-')
            {
                std::cerr << "unknown option: " << argument << "\n";
                return false;
            }
            else if (options.input.empty())
            {
                options.input = argument;
            }
            else
            {
                return false;
            }
        }

        return !options.input.empty();
    }

    /// Prints the frame as text, which is how the layout can be checked from a
    /// terminal: a bright cell is a `` character, a dark one a '.'. It is a
    /// coarse picture, and it is enough to see whether the page is laid out at
    /// all, where the boxes are and whether text landed inside them.
    void PrintAscii(const SoftwareBackend& backend, std::uint32_t columns, std::uint32_t rows)
    {
        if (backend.DeviceWidth() == 0 || backend.DeviceHeight() == 0)
        {
            return;
        }

        static const char* ramp = " .:-=+*#%@";

        for (std::uint32_t row = 0; row < rows; ++row)
        {
            std::string line;
            line.reserve(columns);

            const std::uint32_t y0 = row * backend.DeviceHeight() / rows;
            const std::uint32_t y1 = std::max(y0 + 1, (row + 1) * backend.DeviceHeight() / rows);

            for (std::uint32_t column = 0; column < columns; ++column)
            {
                const std::uint32_t x0 = column * backend.DeviceWidth() / columns;
                const std::uint32_t x1 = std::max(x0 + 1, (column + 1) * backend.DeviceWidth() / columns);

                double luminance = 0.0;
                std::uint32_t samples = 0;

                for (std::uint32_t y = y0; y < y1 && y < backend.DeviceHeight(); ++y)
                {
                    for (std::uint32_t x = x0; x < x1 && x < backend.DeviceWidth(); ++x)
                    {
                        const Color pixel = backend.ReadPixel(x, y);
                        luminance += (0.2126 * pixel.red + 0.7152 * pixel.green + 0.0722 * pixel.blue) / 255.0;
                        ++samples;
                    }
                }

                const double ink = samples == 0 ? 1.0 : 1.0 - luminance / samples;
                const std::size_t step = static_cast<std::size_t>(ink * 9.999);
                line.push_back(ramp[step > 9 ? 9 : step]);
            }

            std::cout << line << "\n";
        }
    }

    /// Writes the current framebuffer to a file, choosing PNG or PPM.
    bool WriteFrame(const SoftwareBackend& backend, const std::string& path, bool writePpm)
    {
        if (writePpm)
        {
            return WritePpmFile(path.c_str(), backend.Framebuffer());
        }

        return WritePngFile(path.c_str(), backend.Framebuffer());
    }

    /// Runs every requested interaction, in the order a user would perform it.
    void ApplyInteractions(Runtime& renderer, const CompiledDocument& document, const ViewerOptions& options)
    {
        if (options.hasScroll)
        {
            InputEvent event;
            event.type = EventType::Wheel;
            event.x = options.width * 0.5f;
            event.y = options.height * 0.5f;
            event.deltaY = options.scrollY;
            renderer.SendEvent(event);
        }

        if (options.hasHover)
        {
            const ElementIndex target = FindNthInteractive(document, options.hoverIndex);
            if (target != NoElement)
            {
                float x = 0.0f;
                float y = 0.0f;
                CenterOf(renderer, target, x, y);

                InputEvent event;
                event.type = EventType::PointerMove;
                event.x = x;
                event.y = y;
                renderer.SendEvent(event);
            }
        }

        if (options.hasClick)
        {
            const ElementIndex target = FindNthInteractive(document, options.clickIndex);
            if (target != NoElement)
            {
                float x = 0.0f;
                float y = 0.0f;
                CenterOf(renderer, target, x, y);

                InputEvent down;
                down.type = EventType::PointerDown;
                down.x = x;
                down.y = y;
                renderer.SendEvent(down);

                InputEvent up;
                up.type = EventType::PointerUp;
                up.x = x;
                up.y = y;
                renderer.SendEvent(up);
            }
        }

        if (!options.typeText.empty())
        {
            // Focus the first text control, the way clicking into it would.
            const ElementIndex field = FindNth(document, RenderCategory::Input, 0) != NoElement
                                           ? FindNth(document, RenderCategory::Input, 0)
                                           : FindNth(document, RenderCategory::TextArea, 0);

            if (field != NoElement)
            {
                float x = 0.0f;
                float y = 0.0f;
                CenterOf(renderer, field, x, y);

                InputEvent down;
                down.type = EventType::PointerDown;
                down.x = x;
                down.y = y;
                renderer.SendEvent(down);

                InputEvent up;
                up.type = EventType::PointerUp;
                up.x = x;
                up.y = y;
                renderer.SendEvent(up);

                InputEvent text;
                text.type = EventType::TextInput;
                text.text = options.typeText;
                renderer.SendEvent(text);
            }
        }
    }

    void PrintMetrics(const FrameMetrics& metrics)
    {
        std::cout << "  frame .......... " << metrics.frameIndex << "\n"
                  << "  layout ......... " << metrics.layoutMs << " ms"
                  << (metrics.layoutRan ? " (pass)" : " (skipped)") << "\n"
                  << "  paint .......... " << metrics.paintMs << " ms"
                  << (metrics.paintRan ? " (pass)" : " (skipped)") << "\n"
                  << "  execute ........ " << metrics.executeMs << " ms\n"
                  << "  total .......... " << metrics.totalMs << " ms\n"
                  << "  elements ....... " << metrics.elementCount << "\n"
                  << "  lines .......... " << metrics.lineCount << "\n"
                  << "  commands ....... " << metrics.commandCount << " (" << metrics.glyphCount << " glyphs)\n"
                  << "  full repaint ... " << (metrics.fullRepaint ? "yes" : "no, damage "
                                                                           + std::to_string(
                                                                                 static_cast<int>(metrics.damage.width)) +
                                                                       "x" +
                                                                       std::to_string(
                                                                           static_cast<int>(metrics.damage.height)))
                  << "\n";
    }
}

int main(int argumentCount, char** argumentValues)
{
    ViewerOptions options;

    if (!ParseArguments(argumentCount, argumentValues, options))
    {
        PrintUsage();
        return 2;
    }

    std::string html;

    if (!ReadFile(options.input, html))
    {
        std::cerr << "cannot read " << options.input << "\n";
        return 1;
    }

    if (options.assetDirectory.empty())
    {
        options.assetDirectory = DirectoryOf(options.input);
    }

    if (options.output.empty())
    {
        options.output = options.writePpm ? ReplaceExtension(options.input, ".ppm")
                                          : ReplaceExtension(options.input, ".png");
    }

    // --- compile ---------------------------------------------------------
    RoboTools::CompiledApplication application;
    RoboTools::HarnessOptions harness;
    harness.applicationName = "Viewer";
    harness.documentPath = options.input;
    harness.sourceDirectory = options.assetDirectory;

    std::string error;

    if (!application.Build(html, harness, error))
    {
        std::cerr << "compilation failed\n" << error << "\n";
        return 1;
    }

    // --- render ----------------------------------------------------------
    SoftwareBackend backend;
    Runtime renderer(application.Document());

    RendererOptions rendererOptions;
    rendererOptions.target.logicalWidth = options.width;
    rendererOptions.target.logicalHeight = options.height;
    rendererOptions.target.deviceScale = options.deviceScale;
    rendererOptions.assetDirectory = options.assetDirectory;

    if (!renderer.Initialize(backend, rendererOptions))
    {
        std::cerr << "the renderer refused the document\n";
        std::cerr << renderer.DiagnosticLog().Format();
        return 1;
    }

    if (options.debug)
    {
        renderer.Debug().enabled = true;
        renderer.InvalidateAll();
    }

    ApplyInteractions(renderer, application.Document(), options);

    if (!renderer.RenderFrame())
    {
        std::cerr << "the frame could not be drawn\n";
        std::cerr << renderer.DiagnosticLog().Format();
        return 1;
    }

    if (!WriteFrame(backend, options.output, options.writePpm))
    {
        std::cerr << "cannot write " << options.output << "\n";
        return 1;
    }

    if (options.probe)
    {
        const Color pixel = backend.ReadPixel(options.probeX, options.probeY);
        std::cout << "probe " << options.probeX << "," << options.probeY << " = rgba("
                  << static_cast<int>(pixel.red) << ", " << static_cast<int>(pixel.green) << ", "
                  << static_cast<int>(pixel.blue) << ", " << static_cast<int>(pixel.alpha) << ")\n";
    }

    if (options.ascii)
    {
        PrintAscii(backend, 100, 40);
    }

    if (!options.quiet)
    {
        const FrameMetrics& metrics = renderer.Metrics();

        std::cout << "RoboViewer\n"
                  << "==========\n"
                  << "  document ....... " << options.input << "\n"
                  << "  elements ....... " << application.ElementCount() << " compiled\n"
                  << "  resources ...... " << renderer.Resources().ResourceStats().loadedCount << " loaded, "
                  << renderer.Resources().ResourceStats().failedCount << " failed\n"
                  << "  window ......... " << options.width << "x" << options.height << " at "
                  << options.deviceScale << "x\n"
                  << "  output ......... " << options.output << "\n\n"
                  << "metrics\n"
                  << "-------\n";

        PrintMetrics(metrics);

        if (renderer.DiagnosticLog().Entries().empty())
        {
            std::cout << "\nno renderer diagnostics\n";
        }
        else
        {
            std::cout << "\nrenderer diagnostics\n-------------------\n" << renderer.DiagnosticLog().Format();
        }

        std::cout << options.output << "\n";
    }
    else
    {
        std::cout << options.output << "\n";
    }

    // A second frame at another size is how a resize is verified end to end.
    if (options.hasResize)
    {
        if (!renderer.Resize(options.resizeWidth, options.resizeHeight))
        {
            std::cerr << "resize refused\n";
            return 1;
        }

        if (!renderer.RenderFrame())
        {
            std::cerr << "the frame after the resize could not be drawn\n";
            return 1;
        }

        const std::string resized = ReplaceExtension(options.output, options.writePpm ? ".resized.ppm"
                                                                                      : ".resized.png");

        if (!WriteFrame(backend, resized, options.writePpm))
        {
            std::cerr << "cannot write " << resized << "\n";
            return 1;
        }

        if (!options.quiet)
        {
            std::cout << "\nafter resize to " << options.resizeWidth << "x" << options.resizeHeight << "\n";
            PrintMetrics(renderer.Metrics());
        }

        std::cout << resized << "\n";
    }

    return 0;
}
