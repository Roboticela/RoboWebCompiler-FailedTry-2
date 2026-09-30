// ---------------------------------------------------------------------------
// RoboCSSCompiler - the CSS property registry.
//
//     "background-color"  ->  CSSPropertyId::BackgroundColor
//
// Every property the compiler accepts has one record here: its id, its category,
// whether it inherits, its initial value, the grammar its value must match, and
// what it affects. That record is the single place a property is *defined* -
// the cascade reads `inherited` from it, the value parser reads `grammar` from
// it, the diagnostics read `supported` from it, and the renderer's dirty
// bookkeeping reads its effect flags from it.
//
// Nothing looks a property up by string at runtime, and nothing outside this
// module decides what a property means.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboCSSCompiler/CssValue.h"

#include <cstdint>
#include <string>
#include <vector>

namespace RoboCSSCompiler
{
    /// The properties this compiler knows. `Unknown` is what an unrecognised
    /// name resolves to, and it is never applied.
    enum class CSSPropertyId : std::uint16_t
    {
        Unknown = 0,

        // --- display and boxes ---------------------------------------------
        Display,
        Width,
        Height,
        MinWidth,
        MinHeight,
        MaxWidth,
        MaxHeight,
        MarginTop,
        MarginRight,
        MarginBottom,
        MarginLeft,
        PaddingTop,
        PaddingRight,
        PaddingBottom,
        PaddingLeft,

        // --- borders --------------------------------------------------------
        BorderTopWidth,
        BorderRightWidth,
        BorderBottomWidth,
        BorderLeftWidth,
        BorderTopStyle,
        BorderRightStyle,
        BorderBottomStyle,
        BorderLeftStyle,
        BorderTopColor,
        BorderRightColor,
        BorderBottomColor,
        BorderLeftColor,
        BorderRadius,

        // --- color and background -------------------------------------------
        Color,
        BackgroundColor,

        // --- text and fonts -------------------------------------------------
        FontSize,
        FontWeight,
        LineHeight,
        LetterSpacing,
        TextAlign,

        // --- overflow -------------------------------------------------------
        OverflowX,
        OverflowY,

        Count
    };

    const char* ToString(CSSPropertyId id);

    /// Broad grouping, used by the coverage report and by the generated
    /// stylesheet, never by the cascade.
    enum class CSSPropertyCategory : std::uint8_t
    {
        Unknown = 0,
        BoxModel,
        Border,
        Background,
        Color,
        Font,
        Text,
        Overflow,
        Display,
        Layout,
        Position,
        Transform,
        Animation,
        Transition,
        Filter,
        List,
        Table,
        Content,
        Interaction,
        Logical
    };

    const char* ToString(CSSPropertyCategory category);

    /// The shape a property's value must have. One value parser per grammar;
    /// the parser is chosen from this field and from nothing else.
    enum class CSSValueGrammar : std::uint8_t
    {
        Unknown = 0,
        /// `auto | none | <length-percentage> | min-content | max-content | fit-content()`
        Size,
        /// `<length-percentage> | auto`
        LengthPercentageOrAuto,
        /// `<length-percentage>`
        LengthPercentage,
        /// `<length> | thin | medium | thick`
        BorderWidth,
        /// A length that may also be `normal`.
        LengthOrNormal,
        /// `normal | <number> | <length-percentage>`
        LineHeight,
        /// A color, including `currentColor` and the system colors.
        Color,
        /// One of a closed keyword set.
        Keyword,
        /// `none | hidden | dotted | dashed | solid | double | groove | ridge | inset | outset`
        BorderStyle,
        /// One to four lengths, or a pair of one-to-four-length groups.
        BorderRadius,
        /// `block | inline | inline-block | flex | grid | none | contents | table | ...`
        Display,
        /// `visible | hidden | clip | scroll | auto`
        Overflow,
        /// `start | end | left | right | center | justify`
        TextAlign,
        /// `normal | bold | bolder | lighter | 1..1000`
        FontWeight,
        /// `xx-small ... xxx-large | smaller | larger | <length-percentage>`
        FontSize
    };

    /// Where a property's four longhands live, for shorthand expansion.
    struct CSSShorthandTargets
    {
        CSSPropertyId top = CSSPropertyId::Unknown;
        CSSPropertyId right = CSSPropertyId::Unknown;
        CSSPropertyId bottom = CSSPropertyId::Unknown;
        CSSPropertyId left = CSSPropertyId::Unknown;

        /// How many of the four are used: 1 for a single-value shorthand, 2 for
        /// a horizontal/vertical pair, 4 for a full box.
        std::uint8_t count = 0;
    };

    struct CSSPropertyDefinition
    {
        CSSPropertyId id;
        const char* name;
        CSSPropertyCategory category;
        /// True when the property's computed value is inherited from the parent.
        bool inherited;
        /// The initial value's CSS text, parsed with the same parser a
        /// declaration uses, so the registry cannot state a value the value
        /// parser would reject.
        const char* initialValue;
        CSSValueGrammar grammar;
        /// The keyword set a `Keyword` grammar accepts, space separated.
        /// Unused by every other grammar.
        const char* keywords;

        /// What a change to the property can affect. The renderer's invalidation
        /// reads these: a paint-only change must not relayout the page.
        bool affectsLayout;
        bool affectsPaint;
        bool affectsText;
        bool affectsHitTesting;

        /// False for a property the registry knows but the compiler cannot
        /// compile yet: it is reported as an unsupported feature rather than as
        /// an unknown property, and its value is never applied.
        bool supported;

        /// The reference specification the property is defined by, as a
        /// directory name under Reference/CSS. The coverage report checks these
        /// against what is actually on disk.
        const char* spec;

        bool IsCustomPropertyTarget() const { return false; }
    };

    /// A shorthand and the longhands it expands into.
    struct CSSShorthandDefinition
    {
        const char* name;
        CSSPropertyCategory category;
        /// Four longhands in top/right/bottom/left order for a box shorthand.
        CSSShorthandTargets targets;
        /// A single longhand both `border` and `border-top` need, expressed as a
        /// named expansion rather than a table.
        const char* rule;
        bool supported;
        const char* spec;
    };

    /// The registry. Built once, immutable, and looked up by name or by id.
    class CSSPropertyRegistry
    {
    public:
        static const CSSPropertyRegistry& Instance();

        /// The definition of a property, or nullptr when the name is unknown.
        const CSSPropertyDefinition* Find(const std::string& name) const;
        const CSSPropertyDefinition* Find(CSSPropertyId id) const;

        /// The shorthand of that name, or nullptr.
        const CSSShorthandDefinition* FindShorthand(const std::string& name) const;

        /// True when the name is a CSS property the reference defines but this
        /// compiler does not compile yet. Used to choose between RCC001 and
        /// RCC010.
        bool IsKnownUnsupportedName(const std::string& name) const;

        /// The name and category of an unknown-but-real CSS property, for the
        /// diagnostic's subject.
        const char* DescribeUnsupported(const std::string& name) const;

        const std::vector<CSSPropertyDefinition>& Properties() const { return properties; }
        const std::vector<CSSShorthandDefinition>& Shorthands() const { return shorthands; }
        const std::vector<const char*>& UnsupportedNames() const { return unsupportedNames; }

        /// Properties that inherit, in registry order. The cascade walks this
        /// list per element rather than testing every property.
        const std::vector<CSSPropertyId>& InheritedProperties() const { return inheritedProperties; }

    private:
        CSSPropertyRegistry();

        std::vector<CSSPropertyDefinition> properties;
        std::vector<CSSShorthandDefinition> shorthands;
        std::vector<const char*> unsupportedNames;
        std::vector<CSSPropertyId> inheritedProperties;
    };
}
