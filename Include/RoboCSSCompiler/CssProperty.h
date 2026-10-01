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
        FontFamily,
        FontStyle,
        LineHeight,
        LetterSpacing,
        WordSpacing,
        TextAlign,
        TextIndent,
        TextTransform,
        WhiteSpace,
        Direction,
        FontKerning,
        FontFeatureSettings,
        TextDecorationLine,
        TextDecorationColor,

        // --- overflow -------------------------------------------------------
        OverflowX,
        OverflowY,

        // --- the cascade's own property -------------------------------------
        /// `all`: a shorthand over every property, whose only values are the
        /// CSS-wide keywords. The cascade expands it before any longhand sees a
        /// declaration, so it is never set on a computed style itself.
        All,

        /// The compiled identifiers end here: every id below this one names a
        /// property the compiler compiles, and every id at or above
        /// `DynamicBase` names one it carries.
        Count,

        /// The first identifier a *carried* property may use.
        ///
        /// A carried property is one the reference defines and this compiler
        /// keeps without interpreting: it needs an identifier so that a
        /// declared value has somewhere to live on a computed style, but it
        /// needs no compiled slot, because nothing lowers it into the native
        /// style. The gap between `Count` and this keeps the two ranges
        /// disjoint, so a single comparison tells a compiled id from a carried
        /// one and only a compiled id may index a per-property array.
        DynamicBase = 0x100
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
        Logical,
        /// The cascade's own properties: `all`.
        Cascade
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
        /// `none | underline | overline | line-through` or several of them.
        TextDecorationLine,
        /// `normal | <feature-tag-value>#`, the OpenType feature settings.
        FontFeatureSettings,
        /// A comma separated list of family names, quoted or not.
        FontFamily,
        /// `normal | bold | bolder | lighter | 1..1000`
        FontWeight,
        /// `xx-small ... xxx-large | smaller | larger | <length-percentage>`
        FontSize,
        /// Anything at all, for a property the compiler carries rather than
        /// interprets. The specification defines 669 properties beyond the 51
        /// this compiler compiles or expands, each with a grammar of its own; a
        /// compiler that models none of those grammars cannot honestly judge
        /// their values, so it keeps what was written instead of refusing a
        /// declaration that is valid CSS. A carried value is preserved as the
        /// tokens the author wrote, which is enough for it to be stored,
        /// inherited, compared and printed back.
        Dynamic
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

        /// True for a property the compiler compiles: it has a compiled id, the
        /// cascade applies it and the style builder lowers it into the native
        /// style.
        bool supported;

        /// True for a property the compiler *carries*: the reference defines it,
        /// the cascade parses and stores its value so that nothing the author
        /// wrote is discarded, and its effect is left to the runtime rather than
        /// lowered into the native style. A carried property is not a missing
        /// feature and is never reported as one - it is the second way a
        /// definition can be usable. Its id is at or above
        /// `CSSPropertyId::DynamicBase` and its grammar is `Dynamic`; `supported`
        /// and `dynamic` are never both true.
        bool dynamic;

        /// The specification the property is defined by, as the module directory
        /// name the reference used. The coverage report names it so a carried or
        /// unimplemented declaration is placed rather than only refused.
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

        /// The definition of a property, whether the compiler compiles it or
        /// carries it, or nullptr when the name is not a property the
        /// compiler knows at all.
        const CSSPropertyDefinition* Find(const std::string& name) const;
        const CSSPropertyDefinition* Find(CSSPropertyId id) const;

        /// The shorthand of that name, or nullptr.
        const CSSShorthandDefinition* FindShorthand(const std::string& name) const;

        /// The definition of a property the reference defines and this compiler
        /// carries: its name, the module that defines it, whether it inherits
        /// and its initial value. A declaration of it is accepted and kept, and
        /// the reference's coverage tests require every name it defines to be
        /// either compiled or found here, which is what makes consuming the
        /// reference safe.
        const CSSPropertyDefinition* FindRegistered(const std::string& name) const;

        /// What the reference defines and this compiler carries, in name order.
        /// The compiler is built against this vocabulary now that the reference
        /// directory itself is gone.
        const std::vector<CSSPropertyDefinition>& RegisteredProperties() const { return registeredProperties; }

        /// True when the name is a real CSS property the compiler does not
        /// even carry, because its specification is outside the reference. Such
        /// a name is not a typo, so the declaration is reported as an
        /// unimplemented feature rather than as an unknown property. Every name
        /// the reference defines answers false: it is compiled or carried.
        bool IsKnownUnsupportedName(const std::string& name) const;

        /// The name and category of an unknown-but-real CSS property, for the
        /// diagnostic's subject.
        const char* DescribeUnsupported(const std::string& name) const;

        /// The property or shorthand whose name is closest to `name`, or nullptr
        /// when nothing is close. Used to answer a mistyped property name with
        /// the name the reader probably meant instead of only saying no.
        const char* SuggestSimilarName(const std::string& name) const;

        const std::vector<CSSPropertyDefinition>& Properties() const { return properties; }
        const std::vector<CSSShorthandDefinition>& Shorthands() const { return shorthands; }
        const std::vector<const char*>& UnsupportedNames() const { return unsupportedNames; }

        /// Properties that inherit, in registry order. The cascade walks this
        /// list per element rather than testing every property.
        const std::vector<CSSPropertyId>& InheritedProperties() const { return inheritedProperties; }

    private:
        CSSPropertyRegistry();

        /// The compiled definition of a name, or nullptr. The public lookup
        /// answers for the carried properties too; this one is what registration
        /// consults, because it runs while the carried table is still being
        /// built and cannot be searched.
        const CSSPropertyDefinition* FindCompiled(const std::string& name) const;

        /// One registered property, from the generated reference tables.
        void RegisterReferenceProperty(const char* name, const char* module, bool inherited,
                                       const char* initialValue);

        std::vector<CSSPropertyDefinition> properties;
        /// Registered rather than dropped: the compiler is built against this
        /// vocabulary once the reference directory itself is gone.
        std::vector<CSSPropertyDefinition> registeredProperties;
        std::vector<CSSShorthandDefinition> shorthands;
        std::vector<const char*> unsupportedNames;
        std::vector<CSSPropertyId> inheritedProperties;
    };
}
