// ---------------------------------------------------------------------------
// RoboRenderer - graphics backend.
//
//     PaintCommand stream  ->  RenderBackend  ->  GPU / display
//
// The interface is deliberately this small: a backend implements ten command
// types and owns a target. Everything that decides *what* to draw - categories,
// layout, state, text, resources - sits above it, so a Direct3D, Vulkan, Metal
// or RoboOS backend is a rasterizer and nothing more.
//
// The software backend below is the reference: it is what the visual regression
// tests compare against, so a GPU backend has a definition of correct to match.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/Image.h"
#include "RoboRenderer/Paint.h"
#include "RoboRenderer/ResourceManager.h"
#include "RoboRenderer/Text.h"

#include <cstdint>
#include <vector>

namespace RoboRenderer
{
    /// Everything a backend needs beyond one paint command.
    struct FrameContext
    {
        const PaintList* commands = nullptr;
        const ResourceManager* resources = nullptr;
        const FontRegistry* fonts = nullptr;
        GlyphCache* glyphs = nullptr;
    };

    class RenderBackend
    {
    public:
        virtual ~RenderBackend() = default;

        virtual const char* Name() const = 0;

        /// Creates the target the renderer will draw into.
        virtual bool Initialize(const RenderTargetInfo& target) = 0;
        virtual void Shutdown() = 0;

        /// Reallocates the target. A backend that cannot honour the size returns
        /// false and keeps its previous target, so the renderer can report it
        /// rather than draw into a half-sized surface.
        virtual bool Resize(const RenderTargetInfo& target) = 0;

        /// Begins a frame whose damaged region is `damage`, in logical units.
        virtual bool BeginFrame(const Rect& damage, const FrameContext& context) = 0;

        /// Executes one command. Called once per command, in stream order.
        virtual void Execute(const PaintCommand& command) = 0;

        /// Presents the frame.
        virtual bool EndFrame() = 0;

        /// Human readable reason the last call failed. Never null.
        virtual const char* LastError() const = 0;

        /// Background of the target before any command runs, which is what makes
        /// an incremental repaint possible: nothing outside `damage` is touched.
        virtual void SetClearColor(const Color& color) = 0;

        /// Pixels written during the last frame, for the benchmark report.
        virtual std::uint64_t DrawnPixelCount() const = 0;

        /// Commands executed during the last frame.
        virtual std::uint32_t ExecutedCommandCount() const = 0;
    };

    /// An RGBA8 software rasterizer.
    ///
    /// It is a real backend, not a stub: anti-aliased rectangles, rounded
    /// rectangles, per-edge borders, bilinear image scaling, coverage blended
    /// text and a clip stack. It exists to be correct, to be testable pixel for
    /// pixel, and to be the definition a hardware backend is checked against.
    class SoftwareBackend : public RenderBackend
    {
    public:
        SoftwareBackend();

        const char* Name() const override { return "Software"; }

        bool Initialize(const RenderTargetInfo& target) override;
        void Shutdown() override;
        bool Resize(const RenderTargetInfo& target) override;
        bool BeginFrame(const Rect& damage, const FrameContext& context) override;
        void Execute(const PaintCommand& command) override;
        bool EndFrame() override;

        const char* LastError() const override { return lastError; }
        void SetClearColor(const Color& color) override { clearColor = color; }
        std::uint64_t DrawnPixelCount() const override { return drawnPixels; }
        std::uint32_t ExecutedCommandCount() const override { return executedCommands; }

        // --- inspection, for tools and tests ------------------------------
        const Image& Framebuffer() const { return framebuffer; }
        Image& MutableFramebuffer() { return framebuffer; }

        std::uint32_t DeviceWidth() const { return framebuffer.width; }
        std::uint32_t DeviceHeight() const { return framebuffer.height; }

        Color ReadPixel(std::uint32_t x, std::uint32_t y) const;

        /// Fills the whole target with a colour and resets the clip, outside of a
        /// frame. Used by the tools that render a single image.
        void Clear(const Color& color);

        /// Frames presented since the backend was initialized.
        std::uint32_t FrameNumber() const;

    private:
        /// One device space rectangle, clipped against the clip stack.
        struct ClipRegion
        {
            std::int32_t left = 0;
            std::int32_t top = 0;
            std::int32_t right = 0;
            std::int32_t bottom = 0;

            bool IsEmpty() const { return right <= left || bottom <= top; }
        };

        ClipRegion CurrentClip() const;
        ClipRegion ToDevice(const Rect& logical) const;
        std::int32_t DeviceX(float logicalX) const;
        std::int32_t DeviceY(float logicalY) const;
        float LogicalX(float deviceX) const;
        float LogicalY(float deviceY) const;

        /// Current translation accumulated by PushTransform, in logical units.
        float TransformX() const;
        float TransformY() const;

        /// The region being repainted: the damaged region, or the whole target
        /// when the renderer asked for a full frame.
        Rect DamageOrWholeTarget() const;

        void BlendPixel(std::int32_t x, std::int32_t y, const Color& color, float coverage);
        /// Fills a device space rectangle with exact per-pixel coverage, which is
        /// what makes a fractional coordinate anti-alias instead of snapping.
        void FillRectPrecise(float left, float top, float right, float bottom, const Color& color);
        void FillRectangle(const Rect& logicalBounds, const Color& color);
        void FillRoundedRectangle(const Rect& logicalBounds, float radius, const Color& color);
        void StrokeRectangle(const Rect& logicalBounds, const Edges& edges, const Color& color);
        void StrokeRoundedRectangle(const Rect& logicalBounds, float radius, const Edges& edges, const Color& color);
        void DrawImage(const Rect& logicalBounds, ResourceIndex resource);
        void DrawText(const PaintCommand& command);

        /// Coverage of a rounded rectangle at a device pixel centre.
        static float RoundedCoverage(float centerX,
                                     float centerY,
                                     float left,
                                     float top,
                                     float right,
                                     float bottom,
                                     float radius);

        Image framebuffer;
        std::vector<ClipRegion> clipStack;
        std::vector<std::pair<float, float>> transformStack;

        FrameContext frameContext;
        RenderTargetInfo target;

        Rect damageRegion;
        Color clearColor{ 255, 255, 255, 255 };

        std::uint64_t drawnPixels = 0;
        std::uint32_t executedCommands = 0;
        std::uint32_t frameNumber = 0;
        bool initialized = false;
        bool frameOpen = false;
        const char* lastError = "no error";
    };
}
