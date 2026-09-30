// ---------------------------------------------------------------------------
// RoboRenderer - the runtime.
//
//     CompiledDocument (immutable)
//              +
//     UiState (mutable)
//              |
//     RoboRenderer: frame loop
//              |
//        UpdateRuntimeState()
//        UpdateLayoutIfRequired()
//        UpdatePaintIfRequired()
//        Backend.BeginFrame(damage)
//        Execute every paint command
//        Backend.EndFrame()
//
// Every step is conditional on what actually changed. A hover moves one button's
// pixels; it does not relayout the page, rebuild the display list from scratch
// or repaint the window. The bookkeeping that makes that possible is a handful of
// dirty flags, not a diffing engine.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/Backend.h"
#include "RoboRenderer/CompiledUi.h"
#include "RoboRenderer/Input.h"
#include "RoboRenderer/Layout.h"
#include "RoboRenderer/Paint.h"
#include "RoboRenderer/ResourceManager.h"
#include "RoboRenderer/RuntimeState.h"
#include "RoboRenderer/Style.h"
#include "RoboRenderer/UiDiagnostics.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace RoboRenderer
{
    struct RendererOptions
    {
        /// Logical size and device scale of the window the document renders into.
        RenderTargetInfo target;

        /// Directory the compiled resources resolve against.
        std::string assetDirectory;

        /// Validates the compiled data before the first frame. A document that
        /// fails validation is refused rather than drawn incorrectly.
        bool validateOnAttach = true;

        /// Background the target is cleared to outside the painted document.
        Color clearColor{ 255, 255, 255, 255 };

        /// Loads every resource at initialization instead of on first use.
        bool preloadResources = true;

        /// How long the caret blinks, in seconds. Zero keeps it steady.
        double caretBlinkPeriod = 1.0;
    };

    /// Measured cost of one frame. Every field is produced by the run, never
    /// asserted about it: these are the numbers the benchmark report prints.
    struct FrameMetrics
    {
        std::uint64_t frameIndex = 0;

        double updateMs = 0.0;
        double layoutMs = 0.0;
        double paintMs = 0.0;
        double executeMs = 0.0;
        double totalMs = 0.0;

        /// Frames between this frame and the previous one.
        double frameIntervalMs = 0.0;

        std::uint32_t layoutPasses = 0;
        std::uint32_t paintPasses = 0;

        std::uint32_t elementCount = 0;
        std::uint32_t lineCount = 0;
        std::uint32_t commandCount = 0;
        std::uint32_t glyphCount = 0;
        std::uint32_t imageCount = 0;

        std::uint64_t drawnPixels = 0;

        bool layoutRan = false;
        bool paintRan = false;
        bool fullRepaint = false;

        /// The damaged region of this frame, in logical units.
        Rect damage;
    };

    /// Debug overlay. `ROBO_RENDERER_DEBUG` is 0 in a release build, where the
    /// overlay draws nothing and costs nothing.
    struct DebugRenderOptions
    {
        bool enabled = false;
        bool elementBounds = true;
        bool layoutBoxes = true;
        bool clipRegions = false;
        bool hitRegions = false;
        bool dirtyRegion = true;
        bool elementIds = false;
        bool paintCommands = false;
        bool statistics = true;
    };

    class RoboRenderer
    {
    public:
        explicit RoboRenderer(const CompiledDocument& document);

        /// Binds the renderer to a backend. Returns false when the backend cannot
        /// create the target, or when the compiled data fails validation.
        bool Initialize(RenderBackend& backend, const RendererOptions& options);

        void Shutdown();

        /// Applies a new window size or device scale. A scale change invalidates
        /// cached glyph bitmaps, because they are rasterized per device size.
        bool Resize(float logicalWidth, float logicalHeight, float deviceScale = -1.0f);

        /// Advances time-dependent state: the caret blink. Layout and painting
        /// happen in RenderFrame, not here.
        void Update(double deltaSeconds);

        /// Runs the frame loop. Returns false when the frame could not be drawn.
        bool RenderFrame();

        /// Routes an input event, then applies the invalidation it reported.
        EventResult SendEvent(const InputEvent& event);

        /// Marks everything dirty: the whole window is laid out and repainted.
        void InvalidateAll();

        // --- access -------------------------------------------------------
        const CompiledDocument& Document() const { return *document; }
        UiState& State() { return state; }
        const UiState& State() const { return state; }
        LayoutEngine& Layout() { return layout; }
        const LayoutEngine& Layout() const { return layout; }
        PaintEngine& Paint() { return paint; }
        const PaintEngine& Paint() const { return paint; }
        ResourceManager& Resources() { return resources; }
        const ResourceManager& Resources() const { return resources; }
        InputRouter& Input() { return inputRouter; }
        HitTester& Hits() { return hitTester; }
        RenderBackend& Backend() { return *backend; }
        StyleTableProvider& Styles() { return styles; }
        const RendererOptions& Options() const { return options; }
        const FrameMetrics& Metrics() const { return metrics; }
        const RendererDiagnostics& DiagnosticLog() const { return diagnostics; }
        DebugRenderOptions& Debug() { return debug; }
        const DebugRenderOptions& Debug() const { return debug; }

        bool IsInitialized() const { return initialized; }
        bool CaretVisible() const { return caretVisible; }

    private:
        /// Fills the display list with the debug overlay, after the UI itself.
        void BuildDebugCommands();

        const CompiledDocument* document;

        RendererOptions options;
        /// The native defaults with the compiled style table laid over them.
        StyleTableProvider styles;
        UiState state;
        ResourceManager resources;
        LayoutEngine layout;
        /// Rasterized glyphs, keyed by font size and weight. Owned here because a
        /// backend may keep its own atlas, while this one is the cache every
        /// backend reads through.
        GlyphCache glyphCache;
        PaintEngine paint;
        HitTester hitTester;
        InputRouter inputRouter;

        RenderBackend* backend = nullptr;
        RendererDiagnostics diagnostics;
        DebugRenderOptions debug;
        FrameMetrics metrics;

        double caretTimer = 0.0;
        std::chrono::steady_clock::time_point lastFrameTime;
        bool haveLastFrameTime = false;
        bool caretVisible = true;
        bool initialized = false;
        bool needsFullRepaint = true;
    };
}
