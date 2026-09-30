#include "RoboRenderer/Renderer.h"

#include <algorithm>
#include <chrono>

namespace RoboRenderer
{
    namespace
    {
        double MillisecondsSince(const std::chrono::steady_clock::time_point& start)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
    }

    RoboRenderer::RoboRenderer(const CompiledDocument& compiledDocument)
        : document(&compiledDocument)
        , state(compiledDocument)
        , resources(compiledDocument)
        , layout(compiledDocument, styles)
        , glyphCache(layout.Fonts())
        , paint(compiledDocument, layout, state, styles)
        , hitTester(compiledDocument, layout, state)
        , inputRouter(compiledDocument, layout, state, hitTester)
    {
    }

    bool RoboRenderer::Initialize(RenderBackend& newBackend, const RendererOptions& newOptions)
    {
        backend = &newBackend;
        options = newOptions;

        if (options.target.logicalWidth <= 0.0f || options.target.logicalHeight <= 0.0f)
        {
            options.target.logicalWidth = options.target.logicalWidth > 0.0f ? options.target.logicalWidth : 800.0f;
            options.target.logicalHeight = options.target.logicalHeight > 0.0f ? options.target.logicalHeight : 600.0f;
        }

        if (options.target.deviceScale <= 0.0f)
        {
            options.target.deviceScale = 1.0f;
        }

        // Invalid compiled data is refused, not patched over: the runtime reports
        // RRE codes and the application decides what to do about it.
        if (options.validateOnAttach && !ValidateCompiledDocument(*document, diagnostics))
        {
            return false;
        }

        resources.SetBaseDirectory(options.assetDirectory);

        if (options.preloadResources)
        {
            resources.LoadAll();

            // A resource the runtime cannot rasterize is reported, not fatal: the
            // element paints its placeholder and the frame is still correct for
            // everything else. It stays a warning so that HasErrors keeps meaning
            // "the renderer could not do its job" rather than "this document has a
            // broken reference".
            for (const ResourceDiagnostic& resourceDiagnostic : resources.Diagnostics())
            {
                diagnostics.Warning(RendererDiagnosticCode::ResourceLoadFailure,
                                    NoElement,
                                    "resource could not be loaded: " + resourceDiagnostic.source,
                                    resourceDiagnostic.message);
            }
        }

        layout.SetResourceMetrics(resources.Metrics(), resources.MetricCount());

        backend->SetClearColor(options.clearColor);

        if (!backend->Initialize(options.target))
        {
            diagnostics.Error(RendererDiagnosticCode::RenderingBackendFailure,
                              NoElement,
                              "the graphics backend could not create a render target",
                              backend->LastError());
            return false;
        }

        layout.SetViewport(options.target);

        state.Reset();
        layout.InvalidateAll();
        paint.InvalidateAll();

        needsFullRepaint = true;
        initialized = true;
        caretVisible = true;
        caretTimer = 0.0;

        lastFrameTime = std::chrono::steady_clock::now();
        haveLastFrameTime = true;

        return true;
    }

    void RoboRenderer::Shutdown()
    {
        if (backend != nullptr)
        {
            backend->Shutdown();
        }

        initialized = false;
    }

    bool RoboRenderer::Resize(float logicalWidth, float logicalHeight, float deviceScale)
    {
        RenderTargetInfo target = options.target;
        target.logicalWidth = logicalWidth;
        target.logicalHeight = logicalHeight;

        if (deviceScale > 0.0f)
        {
            if (deviceScale != target.deviceScale)
            {
                // Glyph bitmaps are rasterized per device size, so a scale change
                // makes every cached one wrong rather than merely stale.
                glyphCache.Clear();
            }

            target.deviceScale = deviceScale;
        }

        options.target = target;

        if (backend != nullptr && !backend->Resize(target))
        {
            diagnostics.Error(RendererDiagnosticCode::RenderingBackendFailure,
                              NoElement,
                              "the graphics backend refused the new render target",
                              backend->LastError());
            return false;
        }

        layout.SetViewport(target);
        layout.InvalidateAll();
        paint.InvalidateAll();

        needsFullRepaint = true;
        return true;
    }

    void RoboRenderer::InvalidateAll()
    {
        layout.InvalidateAll();
        paint.InvalidateAll();
        needsFullRepaint = true;
    }

    void RoboRenderer::Update(double deltaSeconds)
    {
        if (options.caretBlinkPeriod <= 0.0 || deltaSeconds <= 0.0)
        {
            return;
        }

        caretTimer += deltaSeconds;

        if (caretTimer < options.caretBlinkPeriod)
        {
            return;
        }

        caretTimer = 0.0;
        caretVisible = !caretVisible;
        paint.SetCaretVisible(caretVisible);

        // Only a focused text control has a caret to blink, so only that element
        // is invalidated.
        const ElementIndex focused = state.Focused();

        if (focused != NoElement && state.Input(focused) != nullptr)
        {
            state.Element(focused).SetFlag(RuntimeFlagPaintDirty);
            paint.Invalidate(focused);
        }
    }

    bool RoboRenderer::RenderFrame()
    {
        if (!initialized || backend == nullptr || document == nullptr)
        {
            return false;
        }

        const auto frameStart = std::chrono::steady_clock::now();

        if (haveLastFrameTime)
        {
            metrics.frameIntervalMs =
                std::chrono::duration<double, std::milli>(frameStart - lastFrameTime).count();
        }

        lastFrameTime = frameStart;
        haveLastFrameTime = true;
        ++metrics.frameIndex;

        const auto updateStart = std::chrono::steady_clock::now();
        metrics.updateMs = MillisecondsSince(updateStart);

        // --- layout ------------------------------------------------------
        const bool forceLayout = needsFullRepaint;
        const bool layoutNeeded = forceLayout || layout.NeedsLayout();
        bool layoutRan = false;

        if (layoutNeeded)
        {
            const auto layoutStart = std::chrono::steady_clock::now();

            if (forceLayout)
            {
                layout.ForceLayout(state);
            }
            else
            {
                layout.UpdateLayout(state);
            }

            metrics.layoutMs = MillisecondsSince(layoutStart);
            ++metrics.layoutPasses;
            layoutRan = true;

            // Geometry moved, so every rectangle the display list holds is stale.
            // Refining this to the elements whose geometry actually changed is a
            // profiling decision; the correctness rule is this one.
            paint.InvalidateAll();
        }
        else
        {
            metrics.layoutMs = 0.0;
        }

        // --- paint -------------------------------------------------------
        bool paintRan = false;
        const bool paintWasFull = paint.IsFullRepaint();

        if (paint.IsDirty())
        {
            const auto paintStart = std::chrono::steady_clock::now();
            paint.Build();
            metrics.paintMs = MillisecondsSince(paintStart);
            ++metrics.paintPasses;
            paintRan = true;
        }
        else
        {
            metrics.paintMs = 0.0;
        }

        // A layout pass moves geometry, so every rectangle the display list held
        // is stale: the whole window is repainted, not just the elements that
        // were invalidated.
        const bool fullRepaint = forceLayout || layoutRan || paintWasFull;
        const Rect damage = paintRan ? paint.DirtyRegion() : metrics.damage;

        metrics.layoutRan = layoutRan;
        metrics.paintRan = paintRan;
        metrics.fullRepaint = fullRepaint;

        // Nothing changed and nothing is animating: the frame costs nothing,
        // which is what an idle native window should cost.
        if (!layoutRan && !paintRan)
        {
            metrics.damage = damage;
            metrics.drawnPixels = 0;
            metrics.executeMs = 0.0;
            metrics.totalMs = MillisecondsSince(frameStart);
            return true;
        }

        // The debug overlay appends to the same stream the UI used, so a backend
        // still executes exactly one list in exactly one order.
        if (debug.enabled)
        {
            BuildDebugCommands();
        }

        FrameContext context;
        context.commands = &paint.List();
        context.resources = &resources;
        context.fonts = &layout.Fonts();
        context.glyphs = &glyphCache;

        const auto executeStart = std::chrono::steady_clock::now();

        if (!backend->BeginFrame(damage, context))
        {
            diagnostics.Error(RendererDiagnosticCode::RenderingBackendFailure,
                              NoElement,
                              "the graphics backend could not begin a frame",
                              backend->LastError());
            return false;
        }

        const std::vector<PaintCommand>& commands = paint.List().Commands();

        for (const PaintCommand& command : commands)
        {
            backend->Execute(command);
        }

        if (!backend->EndFrame())
        {
            diagnostics.Error(RendererDiagnosticCode::RenderingBackendFailure,
                              NoElement,
                              "the graphics backend could not present the frame",
                              backend->LastError());
            return false;
        }

        metrics.executeMs = MillisecondsSince(executeStart);

        metrics.damage = damage;
        metrics.elementCount = static_cast<std::uint32_t>(layout.LaidOutCount());
        metrics.lineCount = static_cast<std::uint32_t>(layout.Result().lineCount);
        metrics.commandCount = static_cast<std::uint32_t>(paint.List().CommandCount());
        metrics.glyphCount = static_cast<std::uint32_t>(paint.List().GlyphCount());
        metrics.imageCount = paint.Stats().imageCount;
        metrics.drawnPixels = backend->DrawnPixelCount();
        metrics.totalMs = MillisecondsSince(frameStart);

        needsFullRepaint = false;

        return true;
    }

    EventResult RoboRenderer::SendEvent(const InputEvent& event)
    {
        const float scrollX = state.ScrollX();
        const float scrollY = state.ScrollY();

        const EventResult result = inputRouter.Dispatch(event);

        // The page scroll moves everything, so it repaints the window. Any other
        // pixel change repaints only the element the event targeted; the paint
        // engine unions the affected element's subtree into its damage region.
        if (state.ScrollX() != scrollX || state.ScrollY() != scrollY)
        {
            paint.InvalidateAll();
        }
        else if (result.paintChanged)
        {
            // The element the event acted on and the element that gained or lost
            // focus both changed appearance, so both are invalidated. An event
            // that names neither left the renderer no region to work with.
            const bool namedTarget = result.target != NoElement;
            const bool namedFocus = result.focusChanged != NoElement;

            if (namedTarget)
            {
                paint.Invalidate(result.target);
            }

            if (namedFocus)
            {
                paint.Invalidate(result.focusChanged);
            }

            if (!namedTarget && !namedFocus)
            {
                paint.InvalidateAll();
            }
        }

        return result;
    }

    void RoboRenderer::BuildDebugCommands()
    {
#if ROBO_RENDERER_DEBUG
        PaintList& list = paint.CommandList();

        const LayoutResult& result = layout.Result();
        const Rect viewport{ 0.0f, 0.0f, options.target.logicalWidth, options.target.logicalHeight };

        // The translation the hit tester uses, so an overlay rectangle lands on
        // the pixels the element actually occupies.
        const float documentScrollX = state.ScrollX();
        const float documentScrollY = state.ScrollY();

        if (debug.dirtyRegion && !metrics.damage.IsEmpty())
        {
            PaintCommand& command =
                list.Add(PaintCommandType::StrokeRect, metrics.damage.Rounded(), NoElement);
            command.color = Color::Rgba(255, 0, 255, 160);
            command.edges = Edges::Uniform(1.0f);
        }

        for (ElementIndex index = 0; index < document->elementCount; ++index)
        {
            const ElementLayout* box = result.Element(index);
            if (box == nullptr || (box->flags & LayoutFlagDisplayNone) != 0)
            {
                continue;
            }

            if (debug.layoutBoxes || debug.elementBounds)
            {
                const Rect bounds = box->border.Translated(-documentScrollX, -documentScrollY);

                if (bounds.Intersects(viewport))
                {
                    PaintCommand& command =
                        list.Add(PaintCommandType::StrokeRect, bounds.Rounded(), index);
                    command.color = Color::Rgba(0, 200, 90, 150);
                    command.edges = Edges::Uniform(1.0f);
                }
            }

            if (debug.clipRegions && (box->flags & (LayoutFlagClips | LayoutFlagScrolls)) != 0)
            {
                const Rect clip = box->clip.Translated(-documentScrollX, -documentScrollY);
                PaintCommand& command = list.Add(PaintCommandType::StrokeRect, clip.Rounded(), index);
                command.color = Color::Rgba(255, 140, 0, 150);
                command.edges = Edges::Uniform(1.0f);
            }
        }

        if (debug.hitRegions)
        {
            // The interactive regions, which is what hit testing can return. An
            // oddly shaped result here is a layout problem, visible immediately.
            for (ElementIndex index = 0; index < document->elementCount; ++index)
            {
                const CompiledElement* element = ElementAt(*document, index);
                if (element == nullptr || !element->HasFlag(ElementFlagInteractive))
                {
                    continue;
                }

                const ElementLayout* box = result.Element(index);
                if (box == nullptr)
                {
                    continue;
                }

                PaintCommand& command = list.Add(
                    PaintCommandType::StrokeRect,
                    box->border.Translated(-documentScrollX, -documentScrollY).Rounded(),
                    index);
                command.color = Color::Rgba(0, 120, 255, 180);
                command.edges = Edges::Uniform(1.0f);
            }
        }
#else
        // A release build carries no overlay: the flag is compiled in, but it
        // unlocks nothing and the per-frame bookkeeping above does not exist.
#endif
    }
}
