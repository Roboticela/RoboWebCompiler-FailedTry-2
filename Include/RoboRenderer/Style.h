// ---------------------------------------------------------------------------
// RoboRenderer - native style.
//
// Style is where a renderer normally depends on CSS. This one does not: it
// depends on resolved native style values.
//
//     HTML attributes ─┐
//     authored CSS  ───┼─> compiler ─> CompiledStyle[] ─> layout + paint
//     native defaults ─┘
//
// Two origins, one shape. The native defaults answer the question "what does a
// <button> look like with no styling at all": they are this runtime's
// user-agent stylesheet. The compiled table answers "what did the stylesheet
// ask for": one sparse `CompiledStyle` per interned computed style, with the
// fields the cascade actually decided marked in `specified`.
//
// `StyleTableProvider` composes them - author over default - which is the whole
// of the renderer's involvement with CSS. No selector is matched, no declaration
// is parsed and no property is looked up by name at run time.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboRenderer/CompiledUi.h"

#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>

namespace RoboRenderer
{
    /// Number of RenderCategory values the default style table covers.
    constexpr std::size_t RenderCategoryCount =
        static_cast<std::size_t>(RenderCategory::UnsupportedNamespace) + 1;

    /// The viewport the document is laid out against. Layout is expressed in
    /// logical units; a device scale maps those to physical pixels.
    struct RenderTargetInfo
    {
        float logicalWidth = 0.0f;
        float logicalHeight = 0.0f;
        /// Physical pixels per logical unit (DPI / 96). Layout is unaffected by
        /// it; rasterization and text rasterization are not.
        float deviceScale = 1.0f;

        /// Window size in physical pixels.
        std::uint32_t DeviceWidth() const
        {
            return static_cast<std::uint32_t>(logicalWidth * deviceScale + 0.5f);
        }

        std::uint32_t DeviceHeight() const
        {
            return static_cast<std::uint32_t>(logicalHeight * deviceScale + 0.5f);
        }
    };

    /// Supplies the resolved style of an element.
    class StyleProvider
    {
    public:
        virtual ~StyleProvider() = default;

        /// The resolved style that applies to an element. The returned reference
        /// stays valid for the document's lifetime.
        virtual const CompiledStyle& StyleFor(const CompiledDocument& document,
                                              ElementIndex index,
                                              RenderCategory category) = 0;

        /// True when the element's style was overridden rather than defaulted.
        virtual bool HasOverride(const CompiledDocument& document, ElementIndex index) const = 0;
    };

    /// The native default style: one resolved style per render category, adjusted
    /// per element by the compile-time nature the compiler recorded (inline vs
    /// block, hidden, disabled). These are the formatting defaults of HTML
    /// expressed as native values.
    class DefaultStyleProvider : public StyleProvider
    {
    public:
        DefaultStyleProvider();

        const CompiledStyle& StyleFor(const CompiledDocument& document,
                                      ElementIndex index,
                                      RenderCategory category) override;

        bool HasOverride(const CompiledDocument& document, ElementIndex index) const override;

        /// The unmodified style of a category. Layout uses it for the boxes it
        /// generates itself (anonymous inline rows, table sections).
        const CompiledStyle& CategoryStyle(RenderCategory category) const;

        /// The default font size all other sizes are relative to.
        static constexpr float BaseFontSize = 16.0f;

    private:
        std::array<CompiledStyle, RenderCategoryCount> categoryStyles;

        /// Per-element adjustment is rare (inline/hidden/direction), so only the
        /// temporary for the requested element is materialised.
        CompiledStyle elementStyle;
    };

    /// The native defaults with the compiled style table laid over them.
    ///
    /// This is the author origin of the cascade applied to a compiled document.
    /// It runs once per element per layout pass and does no work that depends on
    /// the source language: an element with no authored style is answered by the
    /// default table directly, and one with a style gets the default's fields
    /// replaced by the fields the style actually specified.
    class StyleTableProvider : public StyleProvider
    {
    public:
        StyleTableProvider();

        const CompiledStyle& StyleFor(const CompiledDocument& document,
                                      ElementIndex index,
                                      RenderCategory category) override;

        bool HasOverride(const CompiledDocument& document, ElementIndex index) const override;

        /// The native defaults, which a tool can inspect on their own.
        const DefaultStyleProvider& Defaults() const { return defaults; }

        /// Elements the compiled table carried a style for, during the last pass.
        std::size_t AuthoredElementCount() const { return authoredElementCount; }

        /// Distinct authored styles the document carries.
        std::size_t AuthoredStyleCount(const CompiledDocument& document) const;

    private:
        /// Composes one authored style over a native default.
        static void ApplyAuthored(CompiledStyle& target, const CompiledStyle& authored);

        /// The style an element was compiled with, or nullptr.
        static const CompiledStyle* AuthoredStyleFor(const CompiledDocument& document, ElementIndex index);

        DefaultStyleProvider defaults;

        /// One slot per element, filled on demand. Per-element storage rather than
        /// one reused buffer: a painter may hold an element's style across a
        /// recursive call, and a shared temporary would quietly hand it the
        /// child's style instead.
        mutable std::vector<CompiledStyle> merged;
        mutable std::vector<std::uint8_t> mergedValid;
        mutable std::size_t authoredElementCount = 0;
    };

    /// Multiplies every channel by a factor. The painters use it for the hover
    /// and pressed appearance of a control, which is native appearance rather
    /// than metadata the document carries.
    Color ShadeColor(Color color, float factor);

    /// True when the category lays its children out as a block: the containing
    /// block width is the available width and sibling boxes stack vertically.
    bool IsBlockCategory(RenderCategory category);

    /// True when the category participates in an inline formatting context.
    bool IsInlineCategory(RenderCategory category);

    /// True when the category is a replaced element with an intrinsic size.
    bool IsReplacedCategory(RenderCategory category);

    /// True when the category renders text content directly.
    bool IsTextCategory(RenderCategory category);

    /// The default intrinsic size of a control, before DPI scaling. Used when the
    /// document declares no size and the content provides none.
    void CategoryIntrinsicSize(RenderCategory category, float& width, float& height);
}
