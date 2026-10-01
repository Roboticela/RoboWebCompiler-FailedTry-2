// ---------------------------------------------------------------------------
// RoboRenderer - compiled UI data.
//
// This header is the contract between RoboHTMLCompiler and RoboRenderer.
//
//     HTML source
//         |
//         v
//     RoboHTMLCompiler            (understands HTML: parsing, validation,
//         |                        normalisation, resource discovery)
//         v
//     Compiled data (this file)   (flat, strongly typed, immutable, indexed)
//         |
//         v
//     RoboRenderer                (understands compiled native UI data only)
//
// Everything here is a plain aggregate of integers and pointers, so a compiler
// can emit it as `static constexpr` arrays and the runtime can read it without
// a single allocation, string comparison or lookup on the hot path.
//
// The renderer never parses HTML. It does not know what a tag is, does not
// build a document object model, and never resolves an attribute by name. The
// categories it understands are the native ElementKind values produced by the
// compiler, and every relationship (parent, child, sibling, text, resource,
// binding, identifier) is a dense index into a contiguous array.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/GeneratedEnums.h"

#include <cstdint>
#include <string>

namespace RoboRenderer
{
    /// Native element category. The vocabulary is owned by the compiler's
    /// registry (CompilerKnowledge/Include/RoboHTMLCompiler/GeneratedEnums.h);
    /// the renderer shares it instead of inventing a second one.
    using ElementKind = RoboHTMLCompiler::ElementKind;

    /// Dense index into CompiledDocument::elements. 0 is the root element.
    using ElementIndex = std::uint32_t;

    /// Index into CompiledDocument::strings. 0 is always the empty string, so a
    /// zero index means "no text" without a validity flag.
    using StringIndex = std::uint32_t;

    /// Index into CompiledDocument::resources. 0 means "no resource": the
    /// compiler keeps that slot empty so it can never be confused with a real
    /// resource.
    using ResourceIndex = std::uint32_t;

    /// Index into CompiledDocument::styles. 0 means "use the native default for
    /// this element category". The table is empty in the HTML-only phase and is
    /// where a future CSS lowering stage publishes resolved native styles.
    using StyleIndex = std::uint32_t;

    /// Index into CompiledDocument::bindings.
    using BindingIndex = std::uint32_t;

    /// Sentinel for "no element".
    constexpr ElementIndex NoElement = 0xFFFFFFFFu;

    /// Maximum element count the compiled format addresses.
    constexpr std::uint32_t MaxElements = 0xFFFFFFFEu;

    // -----------------------------------------------------------------------
    // Events
    // -----------------------------------------------------------------------

    /// Event kinds a compiled binding can ask the runtime to deliver. The list
    /// matches the events the current HTML behaviour implementation produces;
    /// `Unknown` keeps a binding that names something the runtime does not
    /// model yet instead of silently dropping it (validation reports it).
    enum class EventKind : std::uint16_t
    {
        Unknown = 0,
        Click = 1,
        DoubleClick = 2,
        PointerDown = 3,
        PointerUp = 4,
        PointerMove = 5,
        PointerEnter = 6,
        PointerLeave = 7,
        Wheel = 8,
        KeyDown = 9,
        KeyUp = 10,
        Focus = 11,
        Blur = 12,
        InputEvent = 13,
        Change = 14,
        Submit = 15,
        Reset = 16,
        Toggle = 17,
        Scroll = 18,
        Load = 19,
        ErrorEvent = 20
    };

    // -----------------------------------------------------------------------
    // Resources
    // -----------------------------------------------------------------------

    enum class ResourceKind : std::uint8_t
    {
        None = 0,
        Image = 1,
        Media = 2,
        Font = 3,
        StyleSheet = 4,
        Script = 5,
        Document = 6,
        /// Inline SVG markup, carried in the string table rather than on disk.
        /// An `<svg>` element has no file of its own, so the compiler serializes
        /// its subtree and the runtime rasterizes it like a linked `.svg`.
        SvgMarkup = 7
    };

    /// A statically discovered resource. Identical sources are deduplicated by
    /// the compiler, so two elements that reference "logo.png" share one entry
    /// and the ResourceManager loads and decodes it once.
    struct CompiledResource
    {
        /// StringIndex of the source path, as written in the document.
        StringIndex source;
        ResourceKind kind;
        /// ResourceKind of the file the compiler inferred from the extension,
        /// kept separately so a mis-declared kind is still visible.
        ResourceKind inferredKind;
        /// Declared intrinsic size in logical pixels, 0 when unspecified.
        std::uint16_t declaredWidth;
        std::uint16_t declaredHeight;
        /// True when the element that referenced this resource declared
        /// alternative text (so a placeholder can show it instead of a border).
        std::uint8_t hasAlternativeText;
        std::uint8_t reserved;
    };

    // -----------------------------------------------------------------------
    // Native style
    // -----------------------------------------------------------------------

    enum class LayoutValueType : std::uint8_t
    {
        /// Use the native default for the element category.
        Auto = 0,
        /// A fixed value in logical pixels (an HTML width/height attribute).
        Fixed = 1,
        /// The element's own content size (text, image, control).
        Content = 2,
        /// A fraction of the containing block.
        ParentRelative = 3,
        /// A fraction of the viewport.
        ViewportRelative = 4
    };

    /// A number whose meaning is decided by the source that produced it. The
    /// renderer consumes the resolved value and never asks where it came from.
    struct LayoutValue
    {
        float value;
        LayoutValueType type;

        static constexpr LayoutValue Auto() { return LayoutValue{ 0.0f, LayoutValueType::Auto }; }
        static constexpr LayoutValue Fixed(float pixels) { return LayoutValue{ pixels, LayoutValueType::Fixed }; }
        static constexpr LayoutValue Content() { return LayoutValue{ 0.0f, LayoutValueType::Content }; }
    };

    enum class DisplayKind : std::uint8_t
    {
        Block = 0,
        Inline = 1,
        InlineBlock = 2,
        TableRow = 3,
        TableCell = 4,
        /// Participates in layout but paints nothing (head-like elements).
        Hidden = 5
    };

    enum class OverflowKind : std::uint8_t
    {
        Visible = 0,
        Clip = 1,
        Scroll = 2
    };

    enum class TextAlignKind : std::uint8_t
    {
        Start = 0,
        Center = 1,
        End = 2,
        /// `justify`: the line is stretched to fill its container by growing the
        /// spaces between words. The last line of a block stays start-aligned
        /// unless `text-align-last` says otherwise.
        Justify = 3
    };

    /// `hyphens`: whether words may break with a hyphen.
    enum class HyphensKind : std::uint8_t
    {
        None = 0,
        Manual = 1,
        Auto = 2
    };

    /// `overflow-wrap` and `word-break`: where a line may break inside a word.
    enum class OverflowWrapKind : std::uint8_t
    {
        Normal = 0,
        BreakWord = 1,
        Anywhere = 2
    };

    enum class WordBreakKind : std::uint8_t
    {
        Normal = 0,
        BreakAll = 1,
        KeepAll = 2
    };

    /// `line-break`. `Anywhere` permits a break between any two characters, the
    /// rest only relax or tighten the script rules the breaker already applies.
    enum class LineBreakKind : std::uint8_t
    {
        Auto = 0,
        Loose = 1,
        Normal = 2,
        Strict = 3,
        Anywhere = 4
    };

    /// `text-justify`: which gaps justification grows.
    enum class TextJustifyKind : std::uint8_t
    {
        Auto = 0,
        None = 1,
        InterWord = 2,
        InterCharacter = 3
    };

    /// `writing-mode`: which axis the inline direction runs along. The sideways
    /// modes are the vertical modes with a forced sideways orientation.
    enum class WritingModeKind : std::uint8_t
    {
        HorizontalTb = 0,
        VerticalRl = 1,
        VerticalLr = 2,
        SidewaysRl = 3,
        SidewaysLr = 4
    };

    /// `text-orientation`: how a glyph stands when the writing mode is vertical.
    enum class TextOrientationKind : std::uint8_t
    {
        Mixed = 0,
        Upright = 1,
        Sideways = 2
    };

    /// True for a writing mode whose inline direction is vertical.
    inline bool IsVerticalWritingMode(WritingModeKind mode)
    {
        return mode != WritingModeKind::HorizontalTb;
    }

    /// True when the columns run right to left (`vertical-rl`, `sideways-rl`).
    inline bool IsRightToLeftColumnWritingMode(WritingModeKind mode)
    {
        return mode == WritingModeKind::VerticalRl || mode == WritingModeKind::SidewaysRl;
    }

    enum class FontWeightKind : std::uint8_t
    {
        Normal = 0,
        Bold = 1
    };

    /// `font-style`. A real italic face is preferred; when none is installed the
    /// runtime shears the upright outline, which is what a browser does too.
    enum class FontSlantKind : std::uint8_t
    {
        Upright = 0,
        Italic = 1
    };

    /// `white-space`: whether whitespace collapses and whether the line wraps.
    enum class WhiteSpaceKind : std::uint8_t
    {
        /// collapse spaces, wrap at break opportunities.
        Normal = 0,
        /// collapse spaces, never wrap.
        NoWrap = 1,
        /// keep spaces and explicit newlines, never wrap.
        Pre = 2,
        /// keep spaces, wrap at break opportunities.
        PreWrap = 3,
        /// collapse runs of spaces, honour explicit newlines, wrap.
        PreLine = 4
    };

    /// `text-transform`, applied to a text run before it is measured.
    enum class TextTransformKind : std::uint8_t
    {
        None = 0,
        Uppercase = 1,
        Lowercase = 2,
        Capitalize = 3
    };

    /// `direction`. `Auto` keeps the runtime's rule (the first strong character
    /// decides); the other two state the base direction outright.
    enum class TextDirectionKind : std::uint8_t
    {
        Auto = 0,
        LeftToRight = 1,
        RightToLeft = 2
    };

    /// Which lines `text-decoration-line` draws, as a bitmask: one run can carry
    /// an underline and a strikethrough at once.
    enum TextDecorationFlags : std::uint8_t
    {
        TextDecorationNone = 0u,
        TextDecorationUnderline = 1u << 0,
        TextDecorationOverline = 1u << 1,
        TextDecorationLineThrough = 1u << 2
    };

    /// `text-decoration-style`. The painter draws each of these differently:
    /// one line, two lines, a dotted line, a dashed line, or a wave.
    enum class TextDecorationStyleKind : std::uint8_t
    {
        Solid = 0,
        Double,
        Dotted,
        Dashed,
        Wavy
    };

    /// `text-decoration-thickness`: derive it from the font, or use the length
    /// stored beside this kind.
    enum class TextDecorationThicknessKind : std::uint8_t
    {
        Auto = 0,
        FromFont,
        Length
    };

    /// `text-underline-offset`: the default offset, or the length stored beside
    /// this kind.
    enum class TextUnderlineOffsetKind : std::uint8_t
    {
        Auto = 0,
        Length
    };

    /// `font-kerning`.
    enum class FontKerningKind : std::uint8_t
    {
        /// `auto`: the font's kerning applies, which is the default.
        Auto = 0,
        Normal = 1,
        None = 2
    };

    /// What activating a button does. Resolved at compile time from the button's
    /// `type` attribute so the runtime never reads an attribute.
    enum class ButtonBehavior : std::uint8_t
    {
        Submit = 0,
        Reset = 1,
        /// type="button": activates without touching a form.
        None = 2
    };

    struct Color
    {
        std::uint8_t red;
        std::uint8_t green;
        std::uint8_t blue;
        std::uint8_t alpha;

        static constexpr Color Rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
        {
            return Color{ r, g, b, a };
        }
    };

    /// Edges in logical pixels, clockwise from the top.
    struct Edges
    {
        float top;
        float right;
        float bottom;
        float left;

        static constexpr Edges Uniform(float value) { return Edges{ value, value, value, value }; }
        static constexpr Edges Zero() { return Edges{ 0.0f, 0.0f, 0.0f, 0.0f }; }

        constexpr float Horizontal() const { return left + right; }
        constexpr float Vertical() const { return top + bottom; }
    };

    /// Which fields of a `CompiledStyle` the document actually specified.
    ///
    /// A compiled style has two possible lives. In the category default table it
    /// is complete: every field holds the native default. In the authored table
    /// it is sparse, and these bits are how it says which fields it carries.
    ///
    /// The distinction is not bookkeeping, it is the whole point of the format.
    /// `margin-top: 8px` must not silently zero the other three edges, and a
    /// stylesheet that sets only a colour must not erase the native look of a
    /// control: an unspecified field means "the native default still applies",
    /// which is exactly what a user-agent stylesheet means in a browser.
    enum StyleFlags : std::uint64_t
    {
        StyleFlagNone = 0ull,

        StyleFlagWidth = 1ull << 0,
        StyleFlagHeight = 1ull << 1,
        StyleFlagMinimumWidth = 1ull << 2,
        StyleFlagMinimumHeight = 1ull << 3,

        StyleFlagMarginTop = 1ull << 4,
        StyleFlagMarginRight = 1ull << 5,
        StyleFlagMarginBottom = 1ull << 6,
        StyleFlagMarginLeft = 1ull << 7,

        StyleFlagPaddingTop = 1ull << 8,
        StyleFlagPaddingRight = 1ull << 9,
        StyleFlagPaddingBottom = 1ull << 10,
        StyleFlagPaddingLeft = 1ull << 11,

        StyleFlagBorderTopWidth = 1ull << 12,
        StyleFlagBorderRightWidth = 1ull << 13,
        StyleFlagBorderBottomWidth = 1ull << 14,
        StyleFlagBorderLeftWidth = 1ull << 15,
        StyleFlagBorderRadius = 1ull << 16,

        StyleFlagBackground = 1ull << 17,
        StyleFlagForeground = 1ull << 18,
        StyleFlagBorderColor = 1ull << 19,

        StyleFlagFontSize = 1ull << 20,
        StyleFlagFontWeight = 1ull << 21,
        StyleFlagLetterSpacing = 1ull << 22,
        StyleFlagTextAlign = 1ull << 23,

        StyleFlagDisplay = 1ull << 24,
        StyleFlagOverflowX = 1ull << 25,
        StyleFlagOverflowY = 1ull << 26,

        // The text and font properties the runtime lays text out with. Each is
        // opt-in: an unspecified flag means "the derived default applies", so a
        // document that never sets `line-height` is laid out exactly as before.
        StyleFlagLineHeight = 1ull << 27,
        StyleFlagWordSpacing = 1ull << 28,
        StyleFlagTextIndent = 1ull << 29,
        StyleFlagWhiteSpace = 1ull << 30,
        StyleFlagTextTransform = 1ull << 31,
        StyleFlagDirection = 1ull << 32,
        StyleFlagFontStyle = 1ull << 33,
        StyleFlagFontFamily = 1ull << 34,
        StyleFlagFontKerning = 1ull << 35,
        StyleFlagFontFeatures = 1ull << 36,
        StyleFlagTextDecoration = 1ull << 37,
        StyleFlagTextDecorationColor = 1ull << 38,
        StyleFlagTextDecorationStyle = 1ull << 39,
        StyleFlagTextDecorationThickness = 1ull << 40,
        StyleFlagTextUnderlineOffset = 1ull << 41,
        StyleFlagHyphens = 1ull << 42,
        StyleFlagOverflowWrap = 1ull << 43,
        StyleFlagWordBreak = 1ull << 44,
        StyleFlagLineBreak = 1ull << 45,
        StyleFlagTabSize = 1ull << 46,
        StyleFlagTextAlignLast = 1ull << 47,
        StyleFlagTextJustify = 1ull << 48,
        StyleFlagWritingMode = 1ull << 49,
        StyleFlagTextOrientation = 1ull << 50,

        /// Every margin edge, for a shorthand that set all four.
        StyleFlagMargin = StyleFlagMarginTop | StyleFlagMarginRight | StyleFlagMarginBottom | StyleFlagMarginLeft,
        /// Every padding edge.
        StyleFlagPadding =
            StyleFlagPaddingTop | StyleFlagPaddingRight | StyleFlagPaddingBottom | StyleFlagPaddingLeft,
        /// Every border width.
        StyleFlagBorderWidth = StyleFlagBorderTopWidth | StyleFlagBorderRightWidth |
                               StyleFlagBorderBottomWidth | StyleFlagBorderLeftWidth,
        /// Every edge of both boxes, which is what a `box-sizing` style is about.
        StyleFlagBox = StyleFlagMargin | StyleFlagPadding | StyleFlagBorderWidth
    };

    /// True when a flag is set in a specified mask.
    inline bool IsSpecified(std::uint64_t specified, StyleFlags flag)
    {
        return (specified & static_cast<std::uint64_t>(flag)) != 0ull;
    }

    /// A fully resolved native style. Not CSS: this is the small set of
    /// properties the layout and paint engines actually consume. The category
    /// default table produces every one of these from the native defaults; the
    /// compiler's CSS lowering publishes resolved declarations into a table of
    /// the same type, so the renderer stays CSS-unaware either way.
    struct CompiledStyle
    {
        /// Which fields below are specified. Zero means "nothing here applies".
        std::uint64_t specified;
        LayoutValue width;
        LayoutValue height;
        LayoutValue minimumWidth;
        LayoutValue minimumHeight;

        Edges margin;
        Edges padding;
        Edges borderWidth;

        float borderRadius;

        Color background;
        Color foreground;
        Color borderColor;

        float fontSize;
        /// `line-height` as a multiple of the font size. Zero means "normal":
        /// the runtime derives the line box from the font's metrics.
        float lineHeight;
        float letterSpacing;
        /// `word-spacing`, in logical pixels, added to every space.
        float wordSpacing = 0.0f;
        /// `text-indent`, in logical pixels, applied to the first line of a block.
        float textIndent = 0.0f;

        DisplayKind display;
        OverflowKind overflowX;
        OverflowKind overflowY;
        TextAlignKind textAlign;
        FontWeightKind fontWeight;
        FontSlantKind fontStyle = FontSlantKind::Upright;
        WhiteSpaceKind whiteSpace = WhiteSpaceKind::Normal;
        TextTransformKind textTransform = TextTransformKind::None;
        TextDirectionKind direction = TextDirectionKind::Auto;

        /// `hyphens`. `Auto` is treated as `Manual` because the runtime carries
        /// no hyphenation dictionary; a jump at a soft hyphen still happens.
        HyphensKind hyphens = HyphensKind::Manual;
        OverflowWrapKind overflowWrap = OverflowWrapKind::Normal;
        WordBreakKind wordBreak = WordBreakKind::Normal;
        LineBreakKind lineBreak = LineBreakKind::Auto;
        /// `tab-size` as a count of space advances. Zero means "the default",
        /// which is eight spaces.
        float tabSize = 0.0f;
        /// `tab-size` written as a length, in logical pixels. Zero means the
        /// count above applies instead.
        float tabSizeLength = 0.0f;
        /// `text-align-last`; only meaningful when `textAlign` justifies.
        TextAlignKind textAlignLast = TextAlignKind::Start;
        bool hasTextAlignLast = false;
        TextJustifyKind textJustify = TextJustifyKind::Auto;

        /// `writing-mode` and `text-orientation`. `HorizontalTb` is the initial
        /// value, so a document that never sets these lays out as before.
        WritingModeKind writingMode = WritingModeKind::HorizontalTb;
        TextOrientationKind textOrientation = TextOrientationKind::Mixed;

        /// `font-family`, as an index into the document's strings. 0 means "the
        /// primary family", so an unstyled element needs no entry.
        StringIndex fontFamily = 0;

        /// `font-feature-settings` as written, as an index into the document's
        /// strings. 0 means "the defaults", which is what an unstyled element
        /// needs. Parsed by the shaper rather than stored typed.
        StringIndex fontFeatures = 0;

        /// `font-kerning`. `Auto` leaves the font's kerning on, which is the
        /// shaper's own default.
        FontKerningKind fontKerning = FontKerningKind::Auto;

        /// `text-decoration-line`, as `TextDecorationFlags` bits.
        std::uint8_t textDecoration = TextDecorationNone;

        /// `text-decoration-color`. The text colour paints the decoration when
        /// `StyleFlagTextDecorationColor` is not set, which is CSS's
        /// `currentColor`.
        Color textDecorationColor = Color::Rgba(0, 0, 0, 255);

        /// `text-decoration-style`. `Solid` draws the line in one stroke.
        TextDecorationStyleKind textDecorationStyle = TextDecorationStyleKind::Solid;

        /// `text-decoration-thickness`. The float is the length in logical
        /// pixels and is only read when the kind is `Length`.
        TextDecorationThicknessKind textDecorationThicknessKind = TextDecorationThicknessKind::Auto;
        float textDecorationThickness = 0.0f;

        /// `text-underline-offset`. The float is the length in logical pixels
        /// and is only read when the kind is `Length`.
        TextUnderlineOffsetKind textUnderlineOffsetKind = TextUnderlineOffsetKind::Auto;
        float textUnderlineOffset = 0.0f;

        /// True when the color fields carry a value rather than a default that
        /// the category default should fill in. Kept beside `specified` because
        /// the painters read them directly.
        std::uint8_t hasBackground;
        std::uint8_t hasForeground;
        std::uint8_t hasBorder;
        std::uint8_t hasFontSize;
        /// Style identity used by debug output to show where a style came from.
        std::uint32_t sourceStyleId;
    };

    // -----------------------------------------------------------------------
    // Elements
    // -----------------------------------------------------------------------

    /// Element flags. One word keeps the hot struct compact while still
    /// carrying every boolean the compiled representation produces.
    enum ElementFlags : std::uint64_t
    {
        ElementFlagNone = 0ull,

        /// State written by the document (HTML state attributes). These are the
        /// initial values; the runtime state beside the compiled data owns the
        /// current values.
        ElementFlagHidden = 1ull << 0,
        ElementFlagDisabled = 1ull << 1,
        ElementFlagChecked = 1ull << 2,
        ElementFlagSelected = 1ull << 3,
        ElementFlagReadOnly = 1ull << 4,
        ElementFlagRequired = 1ull << 5,
        ElementFlagInert = 1ull << 6,
        ElementFlagOpen = 1ull << 7,
        ElementFlagMultiple = 1ull << 8,
        ElementFlagAutofocus = 1ull << 9,

        /// Media.
        ElementFlagControls = 1ull << 10,
        ElementFlagAutoPlay = 1ull << 11,
        ElementFlagLoop = 1ull << 12,
        ElementFlagMuted = 1ull << 13,

        /// Additional state defaults.
        ElementFlagReversed = 1ull << 14,
        ElementFlagNoValidate = 1ull << 15,
        ElementFlagRightToLeft = 1ull << 40,

        /// Presence flags for optional numeric and string fields. A presence
        /// flag is how the format says "the document specified this" without
        /// needing a sentinel value in the field.
        ElementFlagHasWidth = 1ull << 16,
        ElementFlagHasHeight = 1ull << 17,
        ElementFlagHasRows = 1ull << 18,
        ElementFlagHasColumns = 1ull << 19,
        ElementFlagHasValue = 1ull << 20,
        ElementFlagHasMinimum = 1ull << 21,
        ElementFlagHasMaximum = 1ull << 22,
        ElementFlagHasTabIndex = 1ull << 23,
        ElementFlagHasId = 1ull << 24,
        ElementFlagHasText = 1ull << 25,
        ElementFlagHasHref = 1ull << 26,
        ElementFlagHasPlaceholder = 1ull << 27,
        ElementFlagHasLabelTarget = 1ull << 28,
        ElementFlagHasAlternativeText = 1ull << 29,
        ElementFlagHasResource = 1ull << 30,
        ElementFlagHasName = 1ull << 31,
        ElementFlagHasListStart = 1ull << 32,
        ElementFlagHasListStyle = 1ull << 33,
        ElementFlagHasNumericValue = 1ull << 37,

        /// Behaviour the runtime models itself, resolved at compile time from
        /// the element category so the runtime never re-derives it from HTML.
        ElementFlagInteractive = 1ull << 34,
        ElementFlagFocusable = 1ull << 35,

        /// The element is a text run produced by lowering mixed content.
        ElementFlagTextRun = 1ull << 36,

        /// The element flows in an inline formatting context (phrasing content).
        ElementFlagInline = 1ull << 38,

        /// The document preserves this element's whitespace verbatim.
        ElementFlagPreserveWhitespace = 1ull << 39
    };

    /// One native element. Field order is deliberate: everything the layout
    /// engine touches every frame (kind, flags, tree links, geometry inputs)
    /// comes first so a traversal walks neighbouring cache lines.
    struct CompiledElement
    {
        std::uint64_t flags;         ///< ElementFlags
        ElementKind kind;
        std::uint16_t styleIndex;    ///< 0 = native default for `kind`
        std::uint16_t headingLevel;  ///< 1..6 for Heading, else 0
        std::uint16_t columnSpan;    ///< table cells, default 1
        std::uint16_t rowSpan;       ///< table cells, default 1
        std::uint16_t columns;       ///< textarea cols / input size, 0 = unspecified
        std::uint16_t rows;          ///< textarea rows, 0 = unspecified

        std::int16_t tabIndex;       ///< only meaningful with ElementFlagHasTabIndex
        ButtonBehavior buttonBehavior;
        std::uint8_t reserved;       ///< padding, keeps listStart 4-byte aligned
        std::int32_t listStart;      ///< ordered list start value, default 1

        ElementIndex parent;
        ElementIndex firstChild;
        ElementIndex lastChild;
        ElementIndex nextSibling;

        StringIndex text;          ///< owned text content, 0 = none
        StringIndex identifier;    ///< the `id` attribute, 0 = none
        StringIndex name;          ///< the `name` attribute, 0 = none
        StringIndex auxiliaryText; ///< placeholder (controls) or alt text (images)
        StringIndex value;         ///< value attribute: control value, option value
        StringIndex href;          ///< link or navigation target
        StringIndex labelTarget;   ///< `for` attribute (a document identifier)
        StringIndex listStyle;     ///< list marker style, 0 = category default

        ResourceIndex resource;    ///< primary image/media resource, 0 = none

        float attributeWidth;      ///< width attribute in logical pixels
        float attributeHeight;     ///< height attribute in logical pixels
        float numericValue;        ///< progress/meter/range value
        float minimum;             ///< range lower bound
        float maximum;             ///< range upper bound

        std::uint32_t firstBinding;
        std::uint32_t bindingCount;

        /// Source position, kept for diagnostics and the debug renderer. It is
        /// data, not code: it costs 8 bytes and no runtime work.
        std::uint32_t sourceLine;
        std::uint32_t sourceColumn;

        bool HasFlag(ElementFlags flag) const { return (flags & flag) != 0ull; }
        bool HasAllFlags(ElementFlags flag) const { return (flags & flag) == flag; }
    };

    /// A binding discovered in the behaviour module and resolved to a native
    /// element at compile time, so dispatch never looks an element up by name.
    struct CompiledBinding
    {
        ElementIndex element;
        EventKind event;
        StringIndex handler;
    };

    /// Identifier index entry. Sorted by identifier text so LookupElementById
    /// can binary search without a hash map.
    struct CompiledIdEntry
    {
        StringIndex identifier;
        ElementIndex element;
    };

    /// The complete compiled UI for one document. Immutable: the runtime keeps
    /// its own state beside it and never writes through these pointers.
    struct CompiledDocument
    {
        /// Application name, for window titles and diagnostics.
        const char* applicationName;

        const CompiledElement* elements;
        std::uint32_t elementCount;

        /// Interned, deduplicated strings. Index 0 is always "".
        const char* const* strings;
        std::uint32_t stringCount;

        const CompiledResource* resources;
        std::uint32_t resourceCount;

        const CompiledBinding* bindings;
        std::uint32_t bindingCount;

        const CompiledIdEntry* identifiers;
        std::uint32_t identifierCount;

        const CompiledStyle* styles;
        std::uint32_t styleCount;

        ElementIndex root;

        /// Compiler build information (version string) for diagnostics.
        const char* compilerVersion;

        /// True when the document was produced in a debug configuration and the
        /// source position fields are populated.
        std::uint8_t hasSourcePositions;
    };

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /// Safe string access: out-of-range indices yield "" rather than a crash so
    /// diagnostics can be produced instead of a segfault.
    const char* StringAt(const CompiledDocument& document, StringIndex index);

    /// Safe element access; returns nullptr for NoElement or an out-of-range
    /// index.
    const CompiledElement* ElementAt(const CompiledDocument& document, ElementIndex index);

    /// `text-transform` for a UTF-8 run.
    ///
    /// The mapping is deliberately length preserving: only ASCII letters are
    /// rewritten, so a code point whose case mapping would change the byte
    /// length (the German 'ß', say) is left as it is. The runtime slices a line
    /// by byte offset, and a transform that moved the offsets would have to be
    /// applied before those offsets were recorded, not after.
    std::string ApplyTextTransform(const char* text, TextTransformKind transform);

    /// Resolves an element by its `id` attribute. Returns NoElement when there
    /// is no such identifier. Binary search over the sorted identifier table.
    ElementIndex FindElementById(const CompiledDocument& document, const char* identifier);

    /// Depth-first traversal helper: the index of the next element in document
    /// order, or NoElement at the end of the tree.
    ElementIndex NextInDocumentOrder(const CompiledDocument& document, ElementIndex index);

    /// The element before `index` in document order, or NoElement at the start.
    /// Used by reverse focus traversal.
    ElementIndex PreviousInDocumentOrder(const CompiledDocument& document, ElementIndex index);

    /// The last element in document order: the end of the tree.
    ElementIndex LastInDocumentOrder(const CompiledDocument& document);

    /// Number of children of an element.
    std::uint32_t ChildCount(const CompiledDocument& document, ElementIndex index);

    // -----------------------------------------------------------------------
    // Render categories
    // -----------------------------------------------------------------------

    /// The renderer's normalised categories. Several HTML tags share one
    /// category and therefore one rendering implementation, exactly as the
    /// compiler collapses many tags into one ElementKind.
    enum class RenderCategory : std::uint8_t
    {
        None = 0,
        Document,
        Container,      ///< div, section, article, header, footer, main, nav, ...
        Text,           ///< p, span, em, strong, code, ...
        PreformattedText,
        Heading,
        LineBreak,
        Separator,
        Link,
        List,           ///< ul, ol, menu, dl
        ListItem,
        DescriptionTerm,
        DescriptionDescription,
        Figure,
        FigureCaption,
        Image,
        Media,          ///< audio, video
        Canvas,
        Form,
        FieldSet,
        Legend,
        Label,
        Button,
        Input,          ///< text-like inputs
        Checkbox,
        Radio,
        Range,
        ColorInput,
        FileInput,
        TextArea,
        Select,
        Option,
        OptionGroup,
        Output,
        Progress,
        Meter,
        Table,
        TableCaption,
        TableColumn,
        TableSection,
        TableRow,
        TableCell,
        Disclosure,
        DisclosureSummary,
        Dialog,
        Frame,
        EmbeddedObject,
        Ruby,
        RubyText,
        CustomElement,
        /// Recognised by the compiler but outside the HTML-only runtime phase
        /// (SVG and MathML namespaces). Never silently dropped: the renderer
        /// records every occurrence as a diagnostic.
        UnsupportedNamespace
    };

    /// Normalised render category for a native element category.
    RenderCategory CategoryFor(ElementKind kind);

    /// Stable diagnostic name of a render category.
    const char* ToString(RenderCategory category);

    /// Stable diagnostic name of an event kind.
    const char* ToString(EventKind event);

    /// Stable diagnostic name of a resource kind.
    const char* ToString(ResourceKind kind);

    /// Stable diagnostic name of an element kind. The renderer does not embed
    /// the HTML vocabulary, so the application installs the compiler's name
    /// table (see ConfigureElementKindNames) and the default reports the
    /// numeric value.
    using ElementKindNameFunction = const char* (*)(ElementKind kind);
    void ConfigureElementKindNames(ElementKindNameFunction function);
    const char* ElementKindName(ElementKind kind);
}
