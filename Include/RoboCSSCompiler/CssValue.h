// ---------------------------------------------------------------------------
// RoboCSSCompiler - typed CSS values.
//
//     "10px 20%"  ->  List[ Dimension(10, px), Percentage(20) ]
//
// The single most important rule of this module is that a CSS value is never a
// string once it has been parsed. A value is a typed node: a keyword, a number,
// a percentage, a dimension with a unit that remembers what family it belongs
// to, a color in a native color structure, a url, a function call, a
// mathematical expression, or a list of those.
//
// Keeping the unit rather than a raw float is what makes the runtime able to do
// its job: `50%` and `0.5` are different values, and only one of them knows it
// needs a containing block before it means anything.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboCSSCompiler/CssDiagnostics.h"

#include <cstdint>
#include <string>
#include <vector>

namespace RoboCSSCompiler
{
    // -----------------------------------------------------------------------
    // Units
    // -----------------------------------------------------------------------

    /// Every unit the reference defines, in one closed set. `None` means "not a
    /// dimension": a number, a keyword, an unparsed anything.
    enum class CSSUnit : std::uint8_t
    {
        None = 0,

        // Absolute lengths.
        Px,
        Cm,
        Mm,
        Q,
        In,
        Pt,
        Pc,

        // Font relative lengths.
        Em,
        Rem,
        Ex,
        Ch,
        Cap,
        Ic,
        Lh,
        Rlh,

        // Viewport relative lengths.
        Vw,
        Vh,
        Vmin,
        Vmax,
        Vb,
        Vi,

        // Small viewport.
        Svw,
        Svh,
        Svmin,
        Svmax,
        Svb,
        Svi,

        // Large viewport.
        Lvw,
        Lvh,
        Lvmin,
        Lvmax,
        Lvb,
        Lvi,

        // Dynamic viewport.
        Dvw,
        Dvh,
        Dvmin,
        Dvmax,
        Dvb,
        Dvi,

        // Container query lengths. Parsed and typed; the runtime has no
        // container context yet, so resolving them is reported as unsupported
        // rather than guessed.
        Cqw,
        Cqh,
        Cqi,
        Cqb,
        Cqmin,
        Cqmax,

        // Angles.
        Deg,
        Rad,
        Grad,
        Turn,

        // Time.
        S,
        Ms,

        // Frequency.
        Hz,
        Khz,

        // Resolution.
        Dpi,
        Dpcm,
        Dppx,

        // Fractional unit. Only meaningful inside a grid track list.
        Fr
    };

    /// What a unit measures. A property's grammar says which families it accepts.
    enum class CSSUnitFamily : std::uint8_t
    {
        None = 0,
        Length,
        Angle,
        Time,
        Frequency,
        Resolution,
        Flex
    };

    /// What a length needs at runtime before it has a number.
    enum class CSSUnitContext : std::uint8_t
    {
        /// px and friends: a value at compile time.
        Absolute = 0,
        /// em, ex, ch, lh: needs the element's own font.
        FontRelative,
        /// rem, rlh: needs the root font.
        RootFontRelative,
        /// vw, vh, ...: needs the viewport.
        ViewportRelative,
        /// cqw, cqh, ...: needs a container.
        ContainerRelative
    };

    CSSUnitFamily UnitFamily(CSSUnit unit);
    CSSUnitContext UnitContext(CSSUnit unit);

    /// True when the unit's value can be turned into a logical unit at compile
    /// time (an absolute length, or an angle, a time, a resolution).
    bool IsAbsoluteUnit(CSSUnit unit);

    /// Canonical lowercase spelling, or an empty string for `None`.
    const char* UnitName(CSSUnit unit);

    /// The unit a dimension spelling denotes, or false when it is unknown.
    bool ParseUnit(const std::string& text, CSSUnit& unit);

    /// Multiplier that turns this unit into its canonical absolute unit: a
    /// logical unit for lengths (1 logical unit = 1 CSS px), degrees for angles,
    /// seconds for time, hertz for frequency, dots-per-pixel for resolution.
    double AbsoluteUnitScale(CSSUnit unit);

    // -----------------------------------------------------------------------
    // Colors
    // -----------------------------------------------------------------------

    /// A color as the compiler understood it. Modern color functions are
    /// converted to sRGB at compile time, because a software rasterizer with an
    /// sRGB framebuffer needs sRGB; `currentColor` is the one color that cannot
    /// be resolved during compilation, so it survives as its own kind.
    enum class CSSColorKind : std::uint8_t
    {
        Srgba = 0,
        CurrentColor
    };

    struct CSSColor
    {
        CSSColorKind kind = CSSColorKind::Srgba;
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        float alpha = 1.0f;

        static CSSColor Rgba(float r, float g, float b, float a = 1.0f);
        static CSSColor Gray(float value);
        static CSSColor FromCurrentColor();

        /// Packed 0xAARRGGBB, which is what a compiled table stores.
        std::uint32_t Packed() const;

        /// Serialised as a CSS color, for diagnostics and for the web target.
        std::string ToString() const;

        bool operator==(const CSSColor& other) const
        {
            return kind == other.kind && red == other.red && green == other.green && blue == other.blue &&
                   alpha == other.alpha;
        }
    };

    /// Named colors and `transparent`. Returns false when the identifier is not
    /// a color. `currentColor` is not one of these: it is a keyword that the
    /// cascade resolves, see `CSSColorKind::CurrentColor`.
    bool ParseNamedColor(const std::string& name, CSSColor& color);

    /// True when a hash token's body could be a hex color: 3, 4, 6 or 8 hex
    /// digits. `#abc` is a perfectly good identifier, so whether it is a color
    /// is decided by the property's grammar, and this is how that decision is
    /// made without guessing.
    bool IsHexColorBody(const std::string& text);

    /// Parses a hex color body without the leading `#`.
    bool ParseHexColor(const std::string& body, CSSColor& color);

    // -----------------------------------------------------------------------
    // calc()
    // -----------------------------------------------------------------------

    enum class CSSMathOperator : std::uint8_t
    {
        Invalid = 0,
        Add,
        Subtract,
        Multiply,
        Divide,
        Min,
        Max,
        Clamp
    };

    const char* ToString(CSSMathOperator op);

    /// A compiled mathematical expression, in reverse polish order.
    ///
    /// A tree would be the obvious representation and the wrong one here: this
    /// expression is evaluated at runtime, once per resolved length, against a
    /// context (containing block, viewport, font), and a flat program of pushes
    /// and operators evaluates with a three-slot stack and no allocation. It is
    /// also what makes the fully static case foldable: a program whose operands
    /// are all absolute evaluates at compile time and never reaches the runtime.
    struct CSSMathProgram
    {
        /// One step: push `value` with `unit` (or a plain number when the unit is
        /// None), or apply `op` to the top of the stack.
        struct Step
        {
            CSSMathOperator op = CSSMathOperator::Invalid;
            double value = 0.0;
            CSSUnit unit = CSSUnit::None;
            /// True when the operand is a percentage of the reference.
            bool isPercentage = false;
        };

        std::vector<Step> steps;

        bool IsEmpty() const { return steps.empty(); }
        std::size_t Size() const { return steps.size(); }

        bool operator==(const CSSMathProgram& other) const;
        std::string ToString() const;
    };

    // -----------------------------------------------------------------------
    // Values
    // -----------------------------------------------------------------------

    enum class CSSValueKind : std::uint8_t
    {
        /// Nothing was parsed. A value with this kind is never applied.
        Invalid = 0,
        /// A CSS-wide or property keyword, lower-cased: `auto`, `bold`, `inherit`.
        Keyword,
        /// An identifier that is not a keyword of the property being parsed,
        /// such as a font family name or a custom identifier.
        Ident,
        /// A plain number, `unit == None`, `isInteger` says how it was written.
        Number,
        /// A percentage, with `number` holding the value as written (50 for 50%).
        Percentage,
        /// A number with a unit. `unit` is never None.
        Dimension,
        Color,
        String,
        Url,
        /// A function call: `text` is the lower-cased name, `items` the arguments.
        Function,
        /// A space separated list.
        List,
        /// A comma separated list of lists: the top level of `font-family`, or the
        /// arguments of a multi-argument function.
        CommaList,
        /// A mathematical expression that survived simplification.
        Math,
        /// `var(--name)`, which is resolved during the cascade. A value that
        /// still has this kind when the cascade finishes is an error.
        Variable
    };

    const char* ToString(CSSValueKind kind);

    struct CSSValue
    {
        CSSValueKind kind = CSSValueKind::Invalid;

        /// Number, Percentage (as written) and Dimension payload.
        double number = 0.0;
        bool isInteger = false;
        CSSUnit unit = CSSUnit::None;

        /// Keyword, Ident (name or custom property name), String, Url, Function
        /// name.
        std::string text;

        /// Color payload.
        CSSColor color;

        /// Math payload.
        CSSMathProgram math;

        /// List, CommaList and Function arguments.
        std::vector<CSSValue> items;

        /// Fallback of a `var()` reference: the second argument, or invalid.
        std::vector<CSSValue> fallback;

        /// Source position of the value, for diagnostics.
        CSSSourceLocation location;

        static CSSValue MakeKeyword(std::string name);
        static CSSValue MakeIdent(std::string name);
        static CSSValue MakeNumber(double value, bool isInteger = false);
        static CSSValue MakePercentage(double value);
        static CSSValue MakeDimension(double value, CSSUnit unit);
        static CSSValue MakeColor(const CSSColor& value);
        static CSSValue MakeString(std::string value);
        static CSSValue MakeUrl(std::string value);
        static CSSValue MakeInvalid();

        bool IsValid() const { return kind != CSSValueKind::Invalid; }
        bool IsKeyword(const char* name) const;
        /// A keyword equal to any of the given names.
        bool IsAnyKeyword(std::initializer_list<const char*> names) const;
        /// True for a Keyword or Ident with this text.
        bool IsIdent(const char* name) const;

        /// The value as a length in logical units, when that is decidable at
        /// compile time. Percentages and context-relative units are not.
        bool IsAbsoluteLength(float& out) const;

        /// True when the value needs a runtime context to become a number.
        bool IsRuntimeDependent() const;

        /// Structural equality, which is what style interning keys on.
        bool operator==(const CSSValue& other) const;
        bool operator!=(const CSSValue& other) const { return !(*this == other); }

        /// Canonical text. Diagnostics and the generated web stylesheet use it.
        std::string ToString() const;

        /// A stable, order sensitive key for a whole value: the interning key of
        /// a declaration list.
        void AppendKey(std::string& out) const;
    };

    /// A comma separated list of values, which is what a multi-value property
    /// (`font-family`, `transition`) parses into.
    std::string JoinValueList(const std::vector<CSSValue>& values);
}
