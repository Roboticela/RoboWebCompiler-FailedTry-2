// ---------------------------------------------------------------------------
// Shared helpers for the RoboRenderer test suites.
//
//     body fragment -> RoboHTMLCompiler pipeline -> compiled UI data
//                                                        |
//                                       RoboRenderer + SoftwareBackend -> pixels
//
// Every suite builds its documents through the real compiler, so a renderer test
// that fails points either at the runtime or at the lowering, and never at a
// hand written fixture that drifted away from both.
// ---------------------------------------------------------------------------
#pragma once

#include "../TestFramework.h"

#include "RendererHarness.h"

#include "RoboRenderer/Renderer.h"

#include <string>

namespace RoboTest
{
    /// The runtime class is named after the system it implements, so the suites
    /// refer to it through this alias rather than through RoboRenderer::RoboRenderer.
    using Runtime = RoboRenderer::RoboRenderer;

    /// Compiles a body fragment into the compiled UI data the renderer consumes.
    inline bool CompileBody(const std::string& body,
                            RoboTools::CompiledApplication& application,
                            std::string& error)
    {
        RoboTools::HarnessOptions options;
        options.applicationName = "RendererTest";
        options.documentPath = "RendererTest.html";
        return application.Build(RoboTools::WrapFragment(body), options, error);
    }

    /// Compiles a fragment with companion sources.
    inline bool CompileBodyWithSources(const std::string& body,
                                       RoboTools::CompiledApplication& application,
                                       const std::vector<std::pair<std::string, std::string>>& styleSheets,
                                       std::string& error)
    {
        RoboTools::HarnessOptions options;
        options.applicationName = "RendererTest";
        options.documentPath = "RendererTest.html";
        options.styleSheets = styleSheets;
        return application.Build(RoboTools::WrapFragment(body), options, error);
    }

    /// A renderer bound to the software backend, which is the reference
    /// implementation of every paint command.
    struct TestRenderer
    {
        explicit TestRenderer(const RoboRenderer::CompiledDocument& document,
                              float width = 800.0f,
                              float height = 600.0f,
                              float deviceScale = 1.0f,
                              const std::string& assetDirectory = std::string())
            : renderer(document)
        {
            RoboRenderer::RendererOptions options;
            options.target.logicalWidth = width;
            options.target.logicalHeight = height;
            options.target.deviceScale = deviceScale;
            options.assetDirectory = assetDirectory;

            initialized = renderer.Initialize(backend, options);
        }

        bool Frame()
        {
            return renderer.RenderFrame();
        }

        /// Renders one frame and returns whether the frame produced pixels.
        bool Draw()
        {
            renderer.InvalidateAll();
            return renderer.RenderFrame();
        }

        RoboRenderer::Color Pixel(std::uint32_t x, std::uint32_t y) const
        {
            return backend.ReadPixel(x, y);
        }

        std::uint32_t Width() const { return backend.DeviceWidth(); }
        std::uint32_t Height() const { return backend.DeviceHeight(); }

        /// Sends a pointer event at the center of an element.
        RoboRenderer::EventResult ClickElement(RoboRenderer::ElementIndex index)
        {
            RoboRenderer::Rect bounds;
            renderer.Hits().ElementBounds(index, bounds);

            const float x = bounds.x + bounds.width * 0.5f;
            const float y = bounds.y + bounds.height * 0.5f;

            RoboRenderer::InputEvent down;
            down.type = RoboRenderer::EventType::PointerDown;
            down.x = x;
            down.y = y;
            RoboRenderer::EventResult result = renderer.SendEvent(down);

            RoboRenderer::InputEvent up;
            up.type = RoboRenderer::EventType::PointerUp;
            up.x = x;
            up.y = y;

            const RoboRenderer::EventResult upResult = renderer.SendEvent(up);
            result.layoutChanged = result.layoutChanged || upResult.layoutChanged;
            result.paintChanged = result.paintChanged || upResult.paintChanged;
            result.activated = upResult.activated;
            result.submitted = upResult.submitted;
            result.reset = upResult.reset;
            result.activatedHref = upResult.activatedHref;
            return result;
        }

        /// Moves the pointer to the center of an element.
        RoboRenderer::EventResult HoverElement(RoboRenderer::ElementIndex index)
        {
            RoboRenderer::Rect bounds;
            renderer.Hits().ElementBounds(index, bounds);

            RoboRenderer::InputEvent move;
            move.type = RoboRenderer::EventType::PointerMove;
            move.x = bounds.x + bounds.width * 0.5f;
            move.y = bounds.y + bounds.height * 0.5f;
            return renderer.SendEvent(move);
        }

        RoboRenderer::SoftwareBackend backend;
        Runtime renderer;
        bool initialized = false;
    };

    /// Finds the Nth element of a category, in document order.
    inline RoboRenderer::ElementIndex FindNth(const RoboRenderer::CompiledDocument& document,
                                              RoboRenderer::RenderCategory category,
                                              std::uint32_t ordinal = 0)
    {
        std::uint32_t seen = 0;

        for (RoboRenderer::ElementIndex index = document.root; index != RoboRenderer::NoElement;)
        {
            const RoboRenderer::CompiledElement* element = RoboRenderer::ElementAt(document, index);

            if (element != nullptr && RoboRenderer::CategoryFor(element->kind) == category)
            {
                if (seen == ordinal)
                {
                    return index;
                }

                ++seen;
            }

            index = RoboRenderer::NextInDocumentOrder(document, index);
        }

        return RoboRenderer::NoElement;
    }

    inline RoboRenderer::ElementIndex FindNthWith(const RoboRenderer::CompiledDocument& document,
                                                  RoboRenderer::RenderCategory category,
                                                  RoboRenderer::ElementFlags flag,
                                                  std::uint32_t ordinal = 0)
    {
        std::uint32_t seen = 0;

        for (RoboRenderer::ElementIndex index = document.root; index != RoboRenderer::NoElement;)
        {
            const RoboRenderer::CompiledElement* element = RoboRenderer::ElementAt(document, index);

            if (element != nullptr && RoboRenderer::CategoryFor(element->kind) == category &&
                element->HasFlag(flag))
            {
                if (seen == ordinal)
                {
                    return index;
                }

                ++seen;
            }

            index = RoboRenderer::NextInDocumentOrder(document, index);
        }

        return RoboRenderer::NoElement;
    }

    /// Number of commands of one type in the current display list.
    inline std::size_t CountCommands(const Runtime& renderer, RoboRenderer::PaintCommandType type)
    {
        std::size_t count = 0;

        for (const RoboRenderer::PaintCommand& command : renderer.Paint().List().Commands())
        {
            if (command.type == type)
            {
                ++count;
            }
        }

        return count;
    }

    /// Finds the first command of a type, or nullptr.
    inline const RoboRenderer::PaintCommand* FindCommand(const Runtime& renderer,
                                                         RoboRenderer::PaintCommandType type,
                                                         RoboRenderer::ElementIndex element = RoboRenderer::NoElement)
    {
        for (const RoboRenderer::PaintCommand& command : renderer.Paint().List().Commands())
        {
            if (command.type != type)
            {
                continue;
            }

            if (element != RoboRenderer::NoElement && command.element != element)
            {
                continue;
            }

            return &command;
        }

        return nullptr;
    }

    /// True when the area contains at least `minimum` pixels that are not the
    /// background: how a test asks "did anything land here".
    inline std::size_t CountInkPixels(const RoboRenderer::SoftwareBackend& backend,
                                      const RoboRenderer::Rect& area,
                                      int threshold = 240)
    {
        std::size_t count = 0;

        const std::uint32_t lastX = static_cast<std::uint32_t>(
            area.Right() < 0.0f ? 0.0f : std::min(area.Right(), static_cast<float>(backend.DeviceWidth())));
        const std::uint32_t lastY = static_cast<std::uint32_t>(
            area.Bottom() < 0.0f ? 0.0f : std::min(area.Bottom(), static_cast<float>(backend.DeviceHeight())));
        const std::uint32_t firstX = static_cast<std::uint32_t>(area.x < 0.0f ? 0.0f : area.x);
        const std::uint32_t firstY = static_cast<std::uint32_t>(area.y < 0.0f ? 0.0f : area.y);

        for (std::uint32_t y = firstY; y < lastY; ++y)
        {
            for (std::uint32_t x = firstX; x < lastX; ++x)
            {
                const RoboRenderer::Color pixel = backend.ReadPixel(x, y);

                if (pixel.red < threshold || pixel.green < threshold || pixel.blue < threshold)
                {
                    ++count;
                }
            }
        }

        return count;
    }

    inline bool SameColor(const RoboRenderer::Color& left, const RoboRenderer::Color& right, int tolerance = 2)
    {
        const int difference = std::abs(static_cast<int>(left.red) - static_cast<int>(right.red)) +
                               std::abs(static_cast<int>(left.green) - static_cast<int>(right.green)) +
                               std::abs(static_cast<int>(left.blue) - static_cast<int>(right.blue));
        return difference <= tolerance * 3;
    }
}
