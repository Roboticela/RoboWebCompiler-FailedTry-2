// ---------------------------------------------------------------------------
// RoboCSSCompiler - values -> typed values.
//
//     "margin: 0 auto"  ->  List[ Dimension(0, px), Keyword(auto) ]
//
// One parser per grammar. The grammar is chosen from the property registry, so
// a property's meaning and its value's shape come from one place, and a
// declaration is parsed at most once - during compilation.
//
// `var()` is substituted here, on the component values, because that is where the
// specification puts it: a custom property's value is a token stream, and it is
// substituted into the stream before the result is matched against the grammar.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboCSSCompiler/CssDiagnostics.h"
#include "RoboCSSCompiler/CssParser.h"
#include "RoboCSSCompiler/CssProperty.h"
#include "RoboCSSCompiler/CssValue.h"

#include <functional>
#include <string>
#include <vector>

namespace RoboCSSCompiler
{
    /// A lookup of custom property values, by name, for one element.
    using CSSCustomPropertyLookup =
        std::function<const std::vector<CSSComponentValue>*(const std::string& name)>;

    /// Substitutes every `var()` in `input`, copying the other component values
    /// through unchanged. Returns false when a reference cannot be resolved, in
    /// which case the declaration is invalid at computed-value time.
    bool SubstituteVariables(const std::vector<CSSComponentValue>& input,
                             const CSSCustomPropertyLookup& lookup,
                             std::vector<CSSComponentValue>& output,
                             CSSDiagnosticSink& diagnostics,
                             const CSSSourceLocation& location,
                             std::string& unresolvedName,
                             int depth = 0);

    /// Parses a declaration's component values against the property's grammar.
    /// Reports and returns false when the value does not match, which is what
    /// makes an invalid value an error rather than a silently dropped one.
    bool ParsePropertyValue(const CSSPropertyDefinition& definition,
                            const std::vector<CSSComponentValue>& components,
                            CSSValue& out,
                            const CSSSourceLocation& location,
                            CSSDiagnosticSink& diagnostics);

    /// Parses component values as a color. Used by the value grammars and by the
    /// `background` shorthand.
    bool ParseColorComponents(const std::vector<CSSComponentValue>& components,
                              CSSColor& out,
                              const CSSSourceLocation& location,
                              CSSDiagnosticSink& diagnostics,
                              std::string& subject);

    /// Evaluates a `calc()`/`min()`/`max()`/`clamp()` expression into a typed
    /// value. A fully absolute program folds here; a program that mixes a
    /// relative operand with an absolute one is reported as unsupported, because
    /// a compiled native length has one dimension and cannot carry both.
    bool ParseMathFunction(const CSSComponentValue& functionValue,
                           CSSValue& out,
                           const CSSSourceLocation& location,
                           CSSDiagnosticSink& diagnostics);

    /// The text of a component value list, for diagnostics.
    std::string ComponentsToText(const std::vector<CSSComponentValue>& components);
}
