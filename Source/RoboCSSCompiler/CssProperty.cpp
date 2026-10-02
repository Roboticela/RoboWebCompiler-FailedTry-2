#include "RoboCSSCompiler/CssProperty.h"

#include "RoboCSSCompiler/GeneratedCssVocabulary.h"

#include <algorithm>
#include <cstring>

namespace RoboCSSCompiler
{
    namespace
    {
        /// The names of properties this compiler reports as unimplemented that
        /// the reference checkout under Reference/CSS does not define, because
        /// they belong to specifications outside it: Pointer Events defines
        /// `touch-action`, MathML Core the `math-*` trio, and `font-stretch` was
        /// renamed to `font-width` by css-fonts-4. Everything else comes from the
        /// generated vocabulary, so this list cannot quietly drift from the
        /// reference the way a long hand-written one does.
        const char* const UnsupportedNamesOutsideTheReference[] = {
            "font-stretch",
            "math-depth",
            "math-shift",
            "math-style",
            "touch-action",
        };

        /// The reference's property table is sorted by name, so this is a binary
        /// search and not a scan: the table has a definition per (module,
        /// property) pair.
        const Generated::ReferenceProperty* FindReferenceProperty(const std::string& name)
        {
            std::size_t low = 0;
            std::size_t high = Generated::ReferencePropertyCount;

            while (low < high)
            {
                const std::size_t middle = low + (high - low) / 2;
                const int order = name.compare(Generated::ReferenceProperties[middle].name);

                if (order == 0)
                {
                    return &Generated::ReferenceProperties[middle];
                }

                if (order < 0)
                {
                    high = middle;
                }
                else
                {
                    low = middle + 1;
                }
            }

            return nullptr;
        }

        const char* FindNameOutsideTheReference(const std::string& name)
        {
            for (const char* candidate : UnsupportedNamesOutsideTheReference)
            {
                if (name == candidate)
                {
                    return candidate;
                }
            }

            return nullptr;
        }
    }

    const char* ToString(CSSPropertyId id)
    {
        const CSSPropertyDefinition* definition = CSSPropertyRegistry::Instance().Find(id);
        return definition != nullptr ? definition->name : "unknown";
    }

    const char* ToString(CSSPropertyCategory category)
    {
        switch (category)
        {
        case CSSPropertyCategory::Unknown: return "unknown";
        case CSSPropertyCategory::BoxModel: return "box-model";
        case CSSPropertyCategory::Border: return "border";
        case CSSPropertyCategory::Background: return "background";
        case CSSPropertyCategory::Color: return "color";
        case CSSPropertyCategory::Font: return "font";
        case CSSPropertyCategory::Text: return "text";
        case CSSPropertyCategory::Overflow: return "overflow";
        case CSSPropertyCategory::Display: return "display";
        case CSSPropertyCategory::Layout: return "layout";
        case CSSPropertyCategory::Position: return "position";
        case CSSPropertyCategory::Transform: return "transform";
        case CSSPropertyCategory::Animation: return "animation";
        case CSSPropertyCategory::Transition: return "transition";
        case CSSPropertyCategory::Filter: return "filter";
        case CSSPropertyCategory::List: return "list";
        case CSSPropertyCategory::Table: return "table";
        case CSSPropertyCategory::Content: return "content";
        case CSSPropertyCategory::Interaction: return "interaction";
        case CSSPropertyCategory::Logical: return "logical";
        case CSSPropertyCategory::Cascade: return "cascade";
        }

        return "unknown";
    }

    namespace
    {
        using Id = CSSPropertyId;
        using Cat = CSSPropertyCategory;
        using Grammar = CSSValueGrammar;

        /// One row of the registry. Written as an aggregate so that a new
        /// property is one line and its metadata sits beside its name.
        struct Row
        {
            Id id;
            const char* name;
            Cat category;
            bool inherited;
            const char* initialValue;
            Grammar grammar;
            const char* keywords;
            bool affectsLayout;
            bool affectsPaint;
            bool affectsText;
            bool affectsHitTesting;
            bool supported;
            const char* spec;
        };

        // (id, name, category, inherited, initial, grammar, keywords,
        //  layout, paint, text, hit, supported, spec)
        const Row PropertyRows[] = {
            // `all` is a shorthand over every property and takes only the
            // CSS-wide keywords, which is why its keyword set is exactly those.
            // The cascade expands it; nothing else has to know about it.
            { Id::All, "all", Cat::Cascade, false, "initial", Grammar::Keyword,
              "initial inherit unset revert revert-layer", true, true, true, true, true, "css-cascade-3" },

            { Id::Display, "display", Cat::Display, false, "inline", Grammar::Display, nullptr, true, true, false, true, true, "css-display-3" },

            { Id::Width, "width", Cat::BoxModel, false, "auto", Grammar::Size, nullptr, true, true, true, true, true, "css-sizing-3" },
            { Id::Height, "height", Cat::BoxModel, false, "auto", Grammar::Size, nullptr, true, true, true, true, true, "css-sizing-3" },
            { Id::MinWidth, "min-width", Cat::BoxModel, false, "0", Grammar::Size, nullptr, true, true, true, true, true, "css-sizing-3" },
            { Id::MinHeight, "min-height", Cat::BoxModel, false, "0", Grammar::Size, nullptr, true, true, true, true, true, "css-sizing-3" },
            { Id::MaxWidth, "max-width", Cat::BoxModel, false, "none", Grammar::Size, nullptr, true, true, true, true, true, "css-sizing-3" },
            { Id::MaxHeight, "max-height", Cat::BoxModel, false, "none", Grammar::Size, nullptr, true, true, true, true, true, "css-sizing-3" },

            { Id::MarginTop, "margin-top", Cat::BoxModel, false, "0", Grammar::LengthPercentageOrAuto, nullptr, true, true, false, true, true, "css-box-3" },
            { Id::MarginRight, "margin-right", Cat::BoxModel, false, "0", Grammar::LengthPercentageOrAuto, nullptr, true, true, false, true, true, "css-box-3" },
            { Id::MarginBottom, "margin-bottom", Cat::BoxModel, false, "0", Grammar::LengthPercentageOrAuto, nullptr, true, true, false, true, true, "css-box-3" },
            { Id::MarginLeft, "margin-left", Cat::BoxModel, false, "0", Grammar::LengthPercentageOrAuto, nullptr, true, true, false, true, true, "css-box-3" },

            { Id::PaddingTop, "padding-top", Cat::BoxModel, false, "0", Grammar::NonNegativeLengthPercentage, nullptr, true, true, false, true, true, "css-box-3" },
            { Id::PaddingRight, "padding-right", Cat::BoxModel, false, "0", Grammar::NonNegativeLengthPercentage, nullptr, true, true, false, true, true, "css-box-3" },
            { Id::PaddingBottom, "padding-bottom", Cat::BoxModel, false, "0", Grammar::NonNegativeLengthPercentage, nullptr, true, true, false, true, true, "css-box-3" },
            { Id::PaddingLeft, "padding-left", Cat::BoxModel, false, "0", Grammar::NonNegativeLengthPercentage, nullptr, true, true, false, true, true, "css-box-3" },

            // The out-of-flow model. `position` and its four offsets are layout
            // properties: moving a box moves what it contains, and an offset
            // change never repaints the page, so only `position` is a paint
            // property too (a static box paints in flow order while an
            // out-of-flow one paints by stacking order).
            { Id::Position, "position", Cat::Position, false, "static", Grammar::Keyword,
              "static relative absolute fixed sticky", true, true, false, true, true, "css-position-3" },
            { Id::Top, "top", Cat::Position, false, "auto", Grammar::LengthPercentageOrAuto, nullptr, true, false, false, true, true, "css-position-3" },
            { Id::Right, "right", Cat::Position, false, "auto", Grammar::LengthPercentageOrAuto, nullptr, true, false, false, true, true, "css-position-3" },
            { Id::Bottom, "bottom", Cat::Position, false, "auto", Grammar::LengthPercentageOrAuto, nullptr, true, false, false, true, true, "css-position-3" },
            { Id::Left, "left", Cat::Position, false, "auto", Grammar::LengthPercentageOrAuto, nullptr, true, false, false, true, true, "css-position-3" },
            { Id::ZIndex, "z-index", Cat::Position, false, "auto", Grammar::ZIndex, nullptr, false, true, false, true, true, "css2" },

            // The float model. `float` decides which side a box hugs and takes
            // it out of the line flow, and `clear` pushes a box below the floats
            // on the side it names. Both change where boxes end up, so both are
            // layout properties.
            { Id::Float, "float", Cat::Position, false, "none", Grammar::Keyword,
              "none left right inline-start inline-end", true, false, false, true, true, "css2" },
            { Id::Clear, "clear", Cat::Position, false, "none", Grammar::Keyword,
              "none left right both inline-start inline-end", true, false, false, true, true, "css2" },

            // The flex model. The container properties pick the main axis and
            // how items are placed on it; the item properties decide how much
            // room each item takes and in what order.
            { Id::FlexDirection, "flex-direction", Cat::Layout, false, "row", Grammar::Keyword,
              "row row-reverse column column-reverse", true, false, false, true, true, "css-flexbox-1" },
            { Id::JustifyContent, "justify-content", Cat::Layout, false, "normal", Grammar::Keyword,
              "normal flex-start flex-end start end center space-between space-around space-evenly", true, false,
              false, true, true, "css-align-3" },
            { Id::AlignItems, "align-items", Cat::Layout, false, "normal", Grammar::Keyword,
              "normal stretch flex-start flex-end start end center baseline", true, false, false, true, true,
              "css-align-3" },
            { Id::AlignSelf, "align-self", Cat::Layout, false, "auto", Grammar::Keyword,
              "auto normal stretch flex-start flex-end start end center baseline", true, false, false, true, true,
              "css-align-3" },
            { Id::FlexGrow, "flex-grow", Cat::Layout, false, "0", Grammar::Number, nullptr, true, false, false,
              true, true, "css-flexbox-1" },
            { Id::FlexShrink, "flex-shrink", Cat::Layout, false, "1", Grammar::Number, nullptr, true, false, false,
              true, true, "css-flexbox-1" },
            { Id::FlexBasis, "flex-basis", Cat::Layout, false, "auto", Grammar::Size, nullptr, true, false, false,
              true, true, "css-flexbox-1" },
            { Id::FlexOrder, "order", Cat::Layout, false, "0", Grammar::Integer, nullptr, true, false, false, true,
              true, "css-flexbox-1" },
            { Id::FlexWrap, "flex-wrap", Cat::Layout, false, "nowrap", Grammar::Keyword,
              "nowrap wrap wrap-reverse", true, false, false, true, true, "css-flexbox-1" },
            { Id::AlignContent, "align-content", Cat::Layout, false, "normal", Grammar::Keyword,
              "normal flex-start flex-end start end center space-between space-around space-evenly stretch", true,
              false, false, true, true, "css-align-3" },
            // `row-gap` and `column-gap`: the space between flex lines and between
            // items. `normal` is the initial value and means no gap for a flex
            // container; a length is the only other form the native box holds.
            { Id::RowGap, "row-gap", Cat::Layout, false, "normal", Grammar::LengthPercentageOrNormal, nullptr, true,
              false, false, true, true, "css-align-3" },
            { Id::ColumnGap, "column-gap", Cat::Layout, false, "normal", Grammar::LengthPercentageOrNormal, nullptr,
              true, false, false, true, true, "css-align-3" },

            { Id::BorderTopWidth, "border-top-width", Cat::Border, false, "medium", Grammar::BorderWidth, nullptr, true, true, false, true, true, "css-backgrounds-3" },
            { Id::BorderRightWidth, "border-right-width", Cat::Border, false, "medium", Grammar::BorderWidth, nullptr, true, true, false, true, true, "css-backgrounds-3" },
            { Id::BorderBottomWidth, "border-bottom-width", Cat::Border, false, "medium", Grammar::BorderWidth, nullptr, true, true, false, true, true, "css-backgrounds-3" },
            { Id::BorderLeftWidth, "border-left-width", Cat::Border, false, "medium", Grammar::BorderWidth, nullptr, true, true, false, true, true, "css-backgrounds-3" },

            { Id::BorderTopStyle, "border-top-style", Cat::Border, false, "none", Grammar::BorderStyle, nullptr, true, true, false, false, true, "css-backgrounds-3" },
            { Id::BorderRightStyle, "border-right-style", Cat::Border, false, "none", Grammar::BorderStyle, nullptr, true, true, false, false, true, "css-backgrounds-3" },
            { Id::BorderBottomStyle, "border-bottom-style", Cat::Border, false, "none", Grammar::BorderStyle, nullptr, true, true, false, false, true, "css-backgrounds-3" },
            { Id::BorderLeftStyle, "border-left-style", Cat::Border, false, "none", Grammar::BorderStyle, nullptr, true, true, false, false, true, "css-backgrounds-3" },

            { Id::BorderTopColor, "border-top-color", Cat::Border, false, "currentcolor", Grammar::Color, nullptr, false, true, false, false, true, "css-backgrounds-3" },
            { Id::BorderRightColor, "border-right-color", Cat::Border, false, "currentcolor", Grammar::Color, nullptr, false, true, false, false, true, "css-backgrounds-3" },
            { Id::BorderBottomColor, "border-bottom-color", Cat::Border, false, "currentcolor", Grammar::Color, nullptr, false, true, false, false, true, "css-backgrounds-3" },
            { Id::BorderLeftColor, "border-left-color", Cat::Border, false, "currentcolor", Grammar::Color, nullptr, false, true, false, false, true, "css-backgrounds-3" },

            { Id::BorderRadius, "border-radius", Cat::Border, false, "0", Grammar::BorderRadius, nullptr, true, true, false, true, true, "css-backgrounds-3" },

            // The initial value of `color` is CanvasText, a system color. A
            // native runtime has one theme, and the compiler does not invent a
            // palette, so a system color is reported as unsupported; the initial
            // value therefore has to be one the value parser can actually
            // produce. Black is CSS 2.1's initial value, which is what this
            // runtime's default text color is, so `color: initial` means what
            // it means everywhere else.
            { Id::Color, "color", Cat::Color, true, "black", Grammar::Color, nullptr, false, true, true, false, true, "css-color-4" },
            { Id::BackgroundColor, "background-color", Cat::Background, false, "transparent", Grammar::Color, nullptr, false, true, false, false, true, "css-backgrounds-3" },

            { Id::FontSize, "font-size", Cat::Font, true, "medium", Grammar::FontSize, nullptr, true, true, true, true, true, "css-fonts-3" },
            { Id::FontWeight, "font-weight", Cat::Font, true, "normal", Grammar::FontWeight, nullptr, true, true, true, true, true, "css-fonts-3" },
            { Id::FontFamily, "font-family", Cat::Font, true, "serif", Grammar::FontFamily, nullptr, true, true, true, true, true, "css-fonts-3" },
            // `oblique` is accepted as the slanted face `italic` already is: the
            // native text engine has one slanted face per family, so an oblique
            // request resolves to it. The `oblique <angle>` form is not a keyword
            // and is reported rather than silently squared away.
            { Id::FontStyle, "font-style", Cat::Font, true, "normal", Grammar::Keyword, "normal italic oblique", true, true, true, true, true, "css-fonts-3" },
            { Id::LineHeight, "line-height", Cat::Font, true, "normal", Grammar::LineHeight, nullptr, true, true, true, true, true, "css-inline-3" },
            { Id::LetterSpacing, "letter-spacing", Cat::Text, true, "normal", Grammar::LengthOrNormal, nullptr, true, true, true, true, true, "css-text-3" },
            { Id::WordSpacing, "word-spacing", Cat::Text, true, "normal", Grammar::LengthOrNormal, nullptr, true, true, true, true, true, "css-text-3" },
            { Id::TextAlign, "text-align", Cat::Text, true, "start", Grammar::TextAlign, nullptr, false, true, true, false, true, "css-text-3" },
            { Id::TextIndent, "text-indent", Cat::Text, true, "0", Grammar::TextIndent, "hanging each-line", true, true, true, true, true, "css-text-3" },
            { Id::TextTransform, "text-transform", Cat::Text, true, "none", Grammar::Keyword, "none uppercase lowercase capitalize", false, true, true, false, true, "css-text-3" },
            // `white-space` became a shorthand for `white-space-collapse` and
            // `text-wrap-mode`, so the reference now names the longhand values too:
            // `wrap` and `collapse` mean the initial collapsing-and-wrapping
            // behaviour, `preserve` means keep the spaces and wrap, and
            // `break-spaces` is that plus the preservation of trailing spaces.
            { Id::WhiteSpace, "white-space", Cat::Text, true, "normal", Grammar::WhiteSpace, "normal nowrap pre pre-wrap pre-line", true, true, true, true, true, "css-text-3" },
            { Id::Direction, "direction", Cat::Text, true, "ltr", Grammar::Keyword, "ltr rtl", false, true, true, true, true, "css-writing-modes-3" },
            { Id::FontKerning, "font-kerning", Cat::Font, true, "auto", Grammar::Keyword, "auto normal none", false, true, true, false, true, "css-fonts-3" },
            { Id::FontFeatureSettings, "font-feature-settings", Cat::Font, true, "normal", Grammar::FontFeatureSettings, nullptr, false, true, true, false, true, "css-fonts-3" },
            // `text-decoration-line` is not inherited by the specification, but
            // a decoration propagates to the descendants it covers. The runtime
            // has one style per element and no propagation channel, so the two
            // longhands inherit here, which is what a decorated paragraph with
            // children inside it needs to look right.
            // The `font-variant-*` longhands are a closed keyword set each; the
            // lowering turns them into OpenType features, which is why they can
            // share the shaper's own `font-feature-settings` channel.
            { Id::FontVariantLigatures, "font-variant-ligatures", Cat::Font, true, "normal", Grammar::KeywordSetList, "normal none common-ligatures no-common-ligatures discretionary-ligatures no-discretionary-ligatures historical-ligatures no-historical-ligatures contextual no-contextual", false, true, true, false, true, "css-fonts-3" },
            { Id::FontVariantCaps, "font-variant-caps", Cat::Font, true, "normal", Grammar::KeywordSetList, "normal small-caps all-small-caps petite-caps all-petite-caps unicase titling-caps", false, true, true, false, true, "css-fonts-3" },
            { Id::FontVariantNumeric, "font-variant-numeric", Cat::Font, true, "normal", Grammar::KeywordSetList, "normal ordinal slashed-zero lining-nums oldstyle-nums proportional-nums tabular-nums diagonal-fractions stacked-fractions", false, true, true, false, true, "css-fonts-3" },
            { Id::FontVariantPosition, "font-variant-position", Cat::Font, true, "normal", Grammar::KeywordSetList, "normal sub super", false, true, true, false, true, "css-fonts-3" },
            { Id::FontVariantEastAsian, "font-variant-east-asian", Cat::Font, true, "normal", Grammar::KeywordSetList, "normal jis78 jis83 jis90 jis04 simplified traditional full-width proportional-width ruby", false, true, true, false, true, "css-fonts-3" },

            { Id::TextDecorationLine, "text-decoration-line", Cat::Text, true, "none", Grammar::TextDecorationLine, "none underline overline line-through spelling-error grammar-error", false, true, true, false, true, "css-text-decor-3" },
            { Id::TextDecorationColor, "text-decoration-color", Cat::Text, true, "currentcolor", Grammar::Color, nullptr, false, true, false, false, true, "css-text-decor-3" },
            { Id::TextDecorationStyle, "text-decoration-style", Cat::Text, true, "solid", Grammar::Keyword, "solid double dotted dashed wavy", false, true, false, false, true, "css-text-decor-3" },
            // These two moved on to css-text-decor-4; the reference states them
            // there, so that is the module the citation names.
            { Id::TextDecorationThickness, "text-decoration-thickness", Cat::Text, true, "auto", Grammar::TextDecorationThickness, "auto from-font", false, true, false, false, true, "css-text-decor-4" },
            { Id::TextUnderlineOffset, "text-underline-offset", Cat::Text, true, "auto", Grammar::LengthPercentageOrAuto, nullptr, false, true, false, false, true, "css-text-decor-4" },

            // The line-breaking family: each controls where a line may break,
            // which is a property of the text layout rather than the paint.
            { Id::Hyphens, "hyphens", Cat::Text, true, "manual", Grammar::Keyword, "none manual auto", true, false, true, false, true, "css-text-3" },
            { Id::OverflowWrap, "overflow-wrap", Cat::Text, true, "normal", Grammar::Keyword, "normal break-word anywhere", true, false, true, false, true, "css-text-3" },
            // `auto-phrase` breaks between words of a phrase rather than inside
            // them, which is the behaviour `normal` already has here.
            { Id::WordBreak, "word-break", Cat::Text, true, "normal", Grammar::Keyword, "normal break-all keep-all break-word auto-phrase", true, false, true, false, true, "css-text-3" },
            { Id::LineBreak, "line-break", Cat::Text, true, "auto", Grammar::Keyword, "auto loose normal strict anywhere", true, false, true, false, true, "css-text-3" },
            { Id::TabSize, "tab-size", Cat::Text, true, "8", Grammar::TabSize, nullptr, true, false, true, false, true, "css-text-3" },
            { Id::TextAlignLast, "text-align-last", Cat::Text, true, "auto", Grammar::Keyword, "auto start end left right center justify", false, true, true, false, true, "css-text-3" },
            // `distribute` is the deprecated spelling of distributing the slack
            // between words, which is what `inter-word` does.
            { Id::TextJustify, "text-justify", Cat::Text, true, "auto", Grammar::Keyword, "auto none inter-word inter-character distribute", false, true, true, false, true, "css-text-3" },

            // The writing mode decides which axis a line runs along, so both are
            // layout properties that also affect how the text is painted.
            // The two-letter and axis spellings are the reference's deprecated
            // aliases for the two older writing modes, and `sideways-right` and
            // `use-glyph-orientation` are the deprecated aliases for
            // `sideways` and `mixed`.
            { Id::WritingMode, "writing-mode", Cat::Text, true, "horizontal-tb", Grammar::Keyword, "horizontal-tb vertical-rl vertical-lr sideways-rl sideways-lr lr lr-tb rl tb tb-rl", true, true, true, true, true, "css-writing-modes-3" },
            { Id::TextOrientation, "text-orientation", Cat::Text, true, "mixed", Grammar::Keyword, "mixed upright sideways sideways-right use-glyph-orientation", true, false, true, false, true, "css-writing-modes-3" },

            { Id::OverflowX, "overflow-x", Cat::Overflow, false, "visible", Grammar::Overflow, nullptr, true, true, false, true, true, "css-overflow-3" },
            { Id::OverflowY, "overflow-y", Cat::Overflow, false, "visible", Grammar::Overflow, nullptr, true, true, false, true, true, "css-overflow-3" }
        };

        /// True when the name is a property the reference defines that this
        /// compiler does not implement, or one of the few names whose
        /// specification lives outside the reference checkout. Neither is a
        /// typo, and neither is silently ignored: both are reported by name.
        bool IsReferenceOnlyName(const std::string& name)
        {
            return FindReferenceProperty(name) != nullptr || FindNameOutsideTheReference(name) != nullptr;
        }

    }

    CSSPropertyRegistry::CSSPropertyRegistry()
    {
        properties.reserve(sizeof(PropertyRows) / sizeof(PropertyRows[0]));

        for (const Row& row : PropertyRows)
        {
            CSSPropertyDefinition definition{};
            definition.id = row.id;
            definition.name = row.name;
            definition.category = row.category;
            definition.inherited = row.inherited;
            definition.initialValue = row.initialValue;
            definition.grammar = row.grammar;
            definition.keywords = row.keywords;
            definition.affectsLayout = row.affectsLayout;
            definition.affectsPaint = row.affectsPaint;
            definition.affectsText = row.affectsText;
            definition.affectsHitTesting = row.affectsHitTesting;
            definition.supported = row.supported;
            definition.spec = row.spec;

            properties.push_back(definition);

            if (row.inherited)
            {
                inheritedProperties.push_back(row.id);
            }
        }

        // Box shorthands: four longhands, expanded from one to four values.
        const auto box = [](Id top, Id right, Id bottom, Id left) {
            CSSShorthandTargets targets;
            targets.top = top;
            targets.right = right;
            targets.bottom = bottom;
            targets.left = left;
            targets.count = 4;
            return targets;
        };

        const auto pair = [](Id first, Id second) {
            CSSShorthandTargets targets;
            targets.top = first;
            targets.right = second;
            targets.count = 2;
            return targets;
        };

        const auto single = [](Id only) {
            CSSShorthandTargets targets;
            targets.top = only;
            targets.count = 1;
            return targets;
        };

        shorthands.push_back({ "margin", Cat::BoxModel,
                               box(Id::MarginTop, Id::MarginRight, Id::MarginBottom, Id::MarginLeft), "box", true,
                               "css-box-3" });
        shorthands.push_back({ "padding", Cat::BoxModel,
                               box(Id::PaddingTop, Id::PaddingRight, Id::PaddingBottom, Id::PaddingLeft), "box", true,
                               "css-box-3" });
        shorthands.push_back({ "border-width", Cat::Border,
                               box(Id::BorderTopWidth, Id::BorderRightWidth, Id::BorderBottomWidth,
                                   Id::BorderLeftWidth),
                               "box", true, "css-backgrounds-3" });
        shorthands.push_back({ "border-style", Cat::Border,
                               box(Id::BorderTopStyle, Id::BorderRightStyle, Id::BorderBottomStyle,
                                   Id::BorderLeftStyle),
                               "box", true, "css-backgrounds-3" });
        shorthands.push_back({ "border-color", Cat::Border,
                               box(Id::BorderTopColor, Id::BorderRightColor, Id::BorderBottomColor,
                                   Id::BorderLeftColor),
                               "box", true, "css-backgrounds-3" });
        shorthands.push_back({ "border-radius", Cat::Border, single(Id::BorderRadius), "radius", true,
                               "css-backgrounds-3" });
        shorthands.push_back({ "border", Cat::Border,
                               box(Id::BorderTopWidth, Id::BorderRightWidth, Id::BorderBottomWidth,
                                   Id::BorderLeftWidth),
                               "border", true, "css-backgrounds-3" });
        shorthands.push_back({ "border-top", Cat::Border, single(Id::BorderTopWidth), "border-side", true,
                               "css-backgrounds-3" });
        shorthands.push_back({ "border-right", Cat::Border, single(Id::BorderRightWidth), "border-side", true,
                               "css-backgrounds-3" });
        shorthands.push_back({ "border-bottom", Cat::Border, single(Id::BorderBottomWidth), "border-side", true,
                               "css-backgrounds-3" });
        shorthands.push_back({ "border-left", Cat::Border, single(Id::BorderLeftWidth), "border-side", true,
                               "css-backgrounds-3" });
        // `inset` is the four offsets as one box shorthand, exactly like `margin`
        // is the four margins: the same 1-to-4 value distribution, because an
        // offset and a margin are the same kind of value.
        shorthands.push_back({ "inset", Cat::Position,
                               box(Id::Top, Id::Right, Id::Bottom, Id::Left), "box", true,
                               "css-position-3" });
        // `gap`: the row gap and, when written, the column gap. The pair rule
        // distributes `gap: <row> <column>` and gives `gap: <row>` the same
        // value for the column, which is exactly the specification's expansion.
        shorthands.push_back({ "gap", Cat::Layout, pair(Id::RowGap, Id::ColumnGap), "pair", true,
                               "css-align-3" });
        // `flex-flow`: `<flex-direction> || <flex-wrap>`, in either order, so it
        // is decoded by asking each longhand's own keyword set whether it owns
        // the token rather than by position.
        shorthands.push_back({ "flex-flow", Cat::Layout, pair(Id::FlexDirection, Id::FlexWrap), "flex-flow",
                               true, "css-flexbox-1" });
        // `flex`: the growth, the shrink and the basis, expanded by rule because
        // the three can be written in any order and the basis is a whole value
        // rather than a keyword. The three targets are grow, shrink and basis.
        CSSShorthandTargets flexTargets;
        flexTargets.top = Id::FlexGrow;
        flexTargets.right = Id::FlexShrink;
        flexTargets.bottom = Id::FlexBasis;
        flexTargets.count = 3;
        shorthands.push_back({ "flex", Cat::Layout, flexTargets, "flex", true, "css-flexbox-1" });
        shorthands.push_back({ "overflow", Cat::Overflow, pair(Id::OverflowX, Id::OverflowY), "pair", true,
                               "css-overflow-3" });
        shorthands.push_back({ "background", Cat::Background, single(Id::BackgroundColor), "background", true,
                               "css-backgrounds-3" });        shorthands.push_back({"font", Cat::Font, single(Id::FontSize), "font", true, "css-fonts-3"});
        // `text-decoration`: the line keywords, the colour, and the style and
        // thickness the runtime does not model. The line keywords and the
        // colour are lowered; anything else is reported by the expansion.
        CSSShorthandTargets decorationTargets;
        decorationTargets.top = Id::TextDecorationLine;
        decorationTargets.right = Id::TextDecorationColor;
        decorationTargets.count = 2;

        // `font-variant` is a shorthand over the five `font-variant-*` longhands.
        // The target table holds four slots, so the expansion is by rule: the
        // cascade distributes each keyword to the longhand whose set owns it.
        shorthands.push_back({"font-variant", Cat::Font, single(Id::FontVariantLigatures), "font-variant", true,
                              "css-fonts-3"});

        shorthands.push_back({"text-decoration", Cat::Text, decorationTargets, "text-decoration", true,
                              "css-text-decor-3"});

        // Everything the reference defines and this compiler does not compile:
        // derived from the generated vocabulary rather than listed, so a module
        // that adds a property adds it to the report the same day. Registration
        // and reporting are two answers to the same question - what does the
        // compiler know about this name - so they are built in one pass.
        for (std::size_t index = 0; index < Generated::ReferencePropertyCount; ++index)
        {
            const Generated::ReferenceProperty& property = Generated::ReferenceProperties[index];

            RegisterReferenceProperty(property.name, property.module, property.inherited,
                                      property.initialValue);
        }

        // A retired module's properties are registered for the same reason its
        // name stays in the module table: the reference had them, and once the
        // directory that stated them is gone these tables are all the compiler
        // has left to be built against.
        for (std::size_t index = 0; index < Generated::RetiredReferencePropertyCount; ++index)
        {
            const Generated::ReferenceProperty& property = Generated::RetiredReferenceProperties[index];

            RegisterReferenceProperty(property.name, property.module, property.inherited,
                                      property.initialValue);
        }

        // One row per name however many modules define it, sorted so that the
        // lookup is the same binary search the reference table uses.
        std::stable_sort(registeredProperties.begin(), registeredProperties.end(),
                         [](const CSSPropertyDefinition& left, const CSSPropertyDefinition& right) {
                             return std::strcmp(left.name, right.name) < 0;
                         });
        registeredProperties.erase(
            std::unique(registeredProperties.begin(), registeredProperties.end(),
                        [](const CSSPropertyDefinition& left, const CSSPropertyDefinition& right) {
                            return std::strcmp(left.name, right.name) == 0;
                        }),
            registeredProperties.end());

        // Identifiers come last, after the table is sorted and deduplicated,
        // because an identifier is the row's position: `DynamicBase` plus the
        // row's index, so that a carried id resolves to its definition with one
        // subtraction and the assignment cannot depend on the order the modules
        // happened to be read in.
        //
        // The inherited ones join the cascade's inheritance list: a carried
        // property inherits because the specification says it does, and the
        // cascade copies it from the parent exactly as it copies a compiled one.
        for (std::size_t index = 0; index < registeredProperties.size(); ++index)
        {
            CSSPropertyDefinition& definition = registeredProperties[index];
            definition.id = static_cast<CSSPropertyId>(
                static_cast<std::uint16_t>(CSSPropertyId::DynamicBase) + static_cast<std::uint16_t>(index));

            if (definition.inherited)
            {
                inheritedProperties.push_back(definition.id);
            }
        }

        // What the compiler implements, as one name list a caller can walk to
        // show what it does: derived from the two tables just built rather than
        // written out, so it cannot fall behind them.
        compiledProperties.reserve(properties.size() + shorthands.size());

        for (const CSSPropertyDefinition& definition : properties)
        {
            compiledProperties.push_back({ definition.name, false });
        }

        for (const CSSShorthandDefinition& shorthand : shorthands)
        {
            compiledProperties.push_back({ shorthand.name, true });
        }

        std::stable_sort(compiledProperties.begin(), compiledProperties.end(),
                         [](const CompiledName& left, const CompiledName& right) {
                             return std::strcmp(left.name, right.name) < 0;
                         });

        // What the reference does not define at all, and this compiler cannot
        // compile: five names whose specifications live outside the checkout.
        // They are the only names left that are reported as unimplemented, now
        // that a name the reference defines is either compiled or carried.
        for (const char* name : UnsupportedNamesOutsideTheReference)
        {
            unsupportedNames.push_back(name);
        }
    }

    void CSSPropertyRegistry::RegisterReferenceProperty(const char* name, const char* module, bool inherited,
                                                        const char* initialValue)
    {
        // A name the compiler already compiles is compiled, not registered: the
        // two tables answer different questions and must not overlap.
        if (FindCompiled(name) != nullptr || FindShorthand(name) != nullptr)
        {
            return;
        }

        CSSPropertyDefinition definition{};
        // The identifier is assigned once the table is sorted, because it is the
        // row's position: see the constructor.
        definition.id = CSSPropertyId::Unknown;
        definition.name = name;
        definition.category = CSSPropertyCategory::Unknown;
        definition.inherited = inherited;
        definition.initialValue = initialValue;
        // Carried, not interpreted: the value parser keeps the written tokens
        // rather than judging them against a grammar this compiler does not
        // model.
        definition.grammar = CSSValueGrammar::Dynamic;
        definition.keywords = nullptr;
        // Nothing carried is lowered into the native style, so it cannot affect
        // layout, paint, text or hit testing; a later stage that does lower one
        // is the stage that will set these.
        definition.affectsLayout = false;
        definition.affectsPaint = false;
        definition.affectsText = false;
        definition.affectsHitTesting = false;
        definition.supported = false;
        definition.dynamic = true;
        definition.spec = module;

        registeredProperties.push_back(definition);
    }

    const CSSPropertyDefinition* CSSPropertyRegistry::FindRegistered(const std::string& name) const
    {
        std::size_t low = 0;
        std::size_t high = registeredProperties.size();

        while (low < high)
        {
            const std::size_t middle = low + (high - low) / 2;
            const int order = name.compare(registeredProperties[middle].name);

            if (order == 0)
            {
                return &registeredProperties[middle];
            }

            if (order < 0)
            {
                high = middle;
            }
            else
            {
                low = middle + 1;
            }
        }

        return nullptr;
    }

    const CSSPropertyRegistry& CSSPropertyRegistry::Instance()
    {
        static const CSSPropertyRegistry registry;
        return registry;
    }

    const CSSPropertyDefinition* CSSPropertyRegistry::FindCompiled(const std::string& name) const
    {
        for (const CSSPropertyDefinition& definition : properties)
        {
            if (name.size() == std::strlen(definition.name) && name == definition.name)
            {
                return &definition;
            }
        }

        return nullptr;
    }

    const CSSPropertyDefinition* CSSPropertyRegistry::Find(const std::string& name) const
    {
        // Compiled first: it is the smaller table and the one a declaration of
        // anything the compiler can actually draw resolves to. A name it does
        // not compile is not unknown - the reference defines it, so it is
        // carried, and the caller gets the definition it can store a value for.
        if (const CSSPropertyDefinition* compiled = FindCompiled(name))
        {
            return compiled;
        }

        return FindRegistered(name);
    }

    const CSSPropertyDefinition* CSSPropertyRegistry::Find(CSSPropertyId id) const
    {
        for (const CSSPropertyDefinition& definition : properties)
        {
            if (definition.id == id)
            {
                return &definition;
            }
        }

        // A carried id is the row's position in the registration, so this is a
        // subtraction and one comparison rather than another search.
        if (static_cast<std::uint16_t>(id) >= static_cast<std::uint16_t>(CSSPropertyId::DynamicBase))
        {
            const std::size_t index =
                static_cast<std::size_t>(static_cast<std::uint16_t>(id) -
                                         static_cast<std::uint16_t>(CSSPropertyId::DynamicBase));

            if (index < registeredProperties.size() && registeredProperties[index].id == id)
            {
                return &registeredProperties[index];
            }
        }

        return nullptr;
    }

    const CSSShorthandDefinition* CSSPropertyRegistry::FindShorthand(const std::string& name) const
    {
        for (const CSSShorthandDefinition& definition : shorthands)
        {
            if (name.size() == std::strlen(definition.name) && name == definition.name)
            {
                return &definition;
            }
        }

        return nullptr;
    }

    const char* CSSPropertyRegistry::SuggestSimilarName(const std::string& name) const
    {
        // Only near misses: a reader who wrote `colour` wants `color`, and a
        // reader who wrote `grid` wants a different answer than a guess.
        const auto distanceWithin = [](const std::string& left, const std::string& right, std::size_t limit) {
            if (left.size() > right.size() + limit || right.size() > left.size() + limit)
            {
                return limit + 1;
            }

            std::vector<std::size_t> previous(right.size() + 1);
            std::vector<std::size_t> current(right.size() + 1);

            for (std::size_t index = 0; index <= right.size(); ++index)
            {
                previous[index] = index;
            }

            for (std::size_t row = 1; row <= left.size(); ++row)
            {
                current[0] = row;
                std::size_t rowMinimum = current[0];

                for (std::size_t column = 1; column <= right.size(); ++column)
                {
                    const std::size_t substitution =
                        previous[column - 1] + (left[row - 1] == right[column - 1] ? 0 : 1);
                    const std::size_t deletion = previous[column] + 1;
                    const std::size_t insertion = current[column - 1] + 1;
                    current[column] = std::min(substitution, std::min(deletion, insertion));
                    rowMinimum = std::min(rowMinimum, current[column]);
                }

                if (rowMinimum > limit)
                {
                    return limit + 1;
                }

                previous.swap(current);
            }

            return previous[right.size()];
        };

        const char* best = nullptr;
        std::size_t bestDistance = 3;

        for (const CSSPropertyDefinition& definition : Properties())
        {
            const std::size_t distance = distanceWithin(name, definition.name, 2);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = definition.name;
            }
        }

        for (const CSSShorthandDefinition& shorthand : Shorthands())
        {
            const std::size_t distance = distanceWithin(name, shorthand.name, 2);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = shorthand.name;
            }
        }

        return best;
    }

    bool CSSPropertyRegistry::IsKnownUnsupportedName(const std::string& name) const
    {
        // A property this compiler compiles is never reported as unimplemented,
        // and neither is a shorthand it expands.
        if (FindCompiled(name) != nullptr || FindShorthand(name) != nullptr)
        {
            return false;
        }

        // Nor is one it carries. The reference defined the name, so the compiler
        // keeps the declaration instead of refusing it; calling that
        // "unimplemented" would report valid CSS as a defect.
        if (FindRegistered(name) != nullptr)
        {
            return false;
        }

        // What is left is a real property from a specification outside the
        // reference, which the compiler knows by name and cannot compile.
        return IsReferenceOnlyName(name);
    }

    const char* CSSPropertyRegistry::DescribeUnsupported(const std::string& name) const
    {
        if (FindCompiled(name) != nullptr || FindShorthand(name) != nullptr)
        {
            return nullptr;
        }

        // Carried rather than unsupported: the compiler can act on the name, so
        // there is nothing to describe as a missing feature.
        if (FindRegistered(name) != nullptr)
        {
            return nullptr;
        }

        // The returned pointer is stable: it is one of the generated tables' own
        // spellings or a literal, never the argument.
        if (const Generated::ReferenceProperty* property = FindReferenceProperty(name))
        {
            return property->name;
        }

        return FindNameOutsideTheReference(name);
    }
}
