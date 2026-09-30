// ---------------------------------------------------------------------------
// RoboCSSCompiler - selectors.
//
//     ".card > .button:hover"  ->  Class(card) Child Class(button) PseudoClass(Hover)
//
// A selector is parsed once, into compounds and combinators, and then matched
// against the document tree the compiler already built. Nothing downstream of
// this module ever sees a selector string again: the cascade needs specificity
// and a match result, and the runtime needs the *state* a rule depends on, not
// the selector it was written as.
//
// The matcher walks the element tree through `CSSElementNode`, a plain view the
// compiler fills in from its own AST. That is the whole interface between CSS
// and HTML: CSS never parses HTML and HTML never parses CSS.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboCSSCompiler/CssDiagnostics.h"

#include <cstdint>
#include <string>
#include <vector>

namespace RoboCSSCompiler
{
    // -----------------------------------------------------------------------
    // The element view
    // -----------------------------------------------------------------------

    /// The runtime state a selector can test. The compiler fills what the
    /// document says (a `checked` attribute, a `disabled` attribute, an open
    /// disclosure); the runtime supplies the rest (hover, focus, press).
    struct CSSElementState
    {
        bool hovered = false;
        bool active = false;
        bool focused = false;
        bool focusVisible = false;
        bool focusWithin = false;
        bool checked = false;
        bool disabled = false;
        bool enabled = true;
        bool required = false;
        bool readOnly = false;
        bool open = false;
        bool placeholderShown = false;
        bool link = false;

        /// The state a compiled style variant is selected for.
        static CSSElementState None() { return CSSElementState{}; }
        static CSSElementState Hover()
        {
            CSSElementState state;
            state.hovered = true;
            return state;
        }
        static CSSElementState Active()
        {
            CSSElementState state;
            state.active = true;
            return state;
        }
        static CSSElementState Focus()
        {
            CSSElementState state;
            state.focused = true;
            state.focusVisible = true;
            return state;
        }
        static CSSElementState Checked()
        {
            CSSElementState state;
            state.checked = true;
            return state;
        }
        static CSSElementState Disabled()
        {
            CSSElementState state;
            state.disabled = true;
            state.enabled = false;
            return state;
        }
        static CSSElementState Open()
        {
            CSSElementState state;
            state.open = true;
            return state;
        }
    };

    /// One element, as a selector sees it. Built by the caller; the CSS library
    /// owns neither the tree nor its lifetime.
    struct CSSElementNode
    {
        std::string tagName;             ///< lower-cased, `div`
        std::string id;
        std::vector<std::string> classes;
        std::string namespaceName = "html";
        std::vector<std::pair<std::string, std::string>> attributes;
        std::string textContent;

        CSSElementNode* parent = nullptr;
        std::vector<CSSElementNode*> children;

        /// Position among element siblings, 1-based.
        std::size_t siblingIndex = 1;
        std::size_t siblingCount = 1;
        /// Position among siblings of the same tag, 1-based, and how many there
        /// are. Structural pseudo-classes need both.
        std::size_t typeIndex = 1;
        std::size_t typeCount = 1;

        /// Document order, which the cascade uses as its last resort.
        std::uint32_t order = 0;

        /// The state the compiler can know from the document alone.
        CSSElementState state;

        const std::string* Attribute(const std::string& name) const
        {
            for (const std::pair<std::string, std::string>& attribute : attributes)
            {
                if (attribute.first == name)
                {
                    return &attribute.second;
                }
            }

            return nullptr;
        }

        bool HasClass(const std::string& name) const
        {
            for (const std::string& value : classes)
            {
                if (value == name)
                {
                    return true;
                }
            }

            return false;
        }

        /// Recomputes siblingIndex/siblingCount/typeIndex/typeCount for a whole
        /// tree. Called once, after the view is built.
        static void NumberSiblings(CSSElementNode& node);
    };

    // -----------------------------------------------------------------------
    // The selector
    // -----------------------------------------------------------------------

    enum class CSSCombinator : std::uint8_t
    {
        None = 0,
        /// `a b`
        Descendant,
        /// `a > b`
        Child,
        /// `a + b`
        NextSibling,
        /// `a ~ b`
        SubsequentSibling
    };

    const char* ToString(CSSCombinator combinator);

    enum class CSSAttributeOperator : std::uint8_t
    {
        Exists = 0,
        Equals,
        Includes,
        DashMatch,
        Prefix,
        Suffix,
        Substring
    };

    enum class CSSPseudoClass : std::uint8_t
    {
        Unknown = 0,

        // Structural: decided during compilation.
        Root,
        FirstChild,
        LastChild,
        OnlyChild,
        NthChild,
        NthLastChild,
        FirstOfType,
        LastOfType,
        OnlyOfType,
        NthOfType,
        NthLastOfType,
        Empty,
        Scope,

        // Logical: decided during compilation, over their argument.
        Not,
        Is,
        Where,
        Has,

        // Runtime state: compiled into a state style.
        Hover,
        Active,
        Focus,
        FocusVisible,
        FocusWithin,
        Disabled,
        Enabled,
        Checked,
        Required,
        Optional,
        ReadOnly,
        ReadWrite,
        Open,
        Closed,
        PlaceholderShown,
        Default,
        Indeterminate,
        Link,
        Visited,
        Target,
        AnyLink
    };

    const char* ToString(CSSPseudoClass pseudoClass);

    /// `an+b`, with a and b already reduced.
    struct CSSNthExpression
    {
        int step = 0;   ///< `a`
        int offset = 0; ///< `b`

        /// True when `index` (1-based) matches an+b for some non-negative n.
        bool Matches(std::size_t index) const;
        std::string ToString() const;
    };

    struct CSSSimpleSelector
    {
        enum class Kind : std::uint8_t
        {
            Universal = 0,
            Type,
            Class,
            Id,
            Attribute,
            PseudoClass,
            PseudoElement
        };

        Kind kind = Kind::Universal;

        /// Type name, class name, id, attribute name, pseudo name.
        std::string name;
        std::string value;      ///< Attribute value.
        std::string namespaceName;

        CSSAttributeOperator attributeOperator = CSSAttributeOperator::Exists;
        bool attributeCaseInsensitive = false;

        CSSPseudoClass pseudoClass = CSSPseudoClass::Unknown;
        /// Name the selector wrote (`hover`), kept for diagnostics.
        std::string pseudoName;
        /// `nth-*` argument.
        CSSNthExpression nth;
        /// `of S` argument of nth-child(), or the argument of :not/:is/:where.
        std::vector<std::vector<CSSSimpleSelector>> argumentLists;

        bool IsRuntimeState() const;
        std::string ToString() const;
    };

    /// A run of simple selectors with no combinator between them: `.card:hover`.
    struct CSSCompoundSelector
    {
        std::vector<CSSSimpleSelector> simple;

        std::uint32_t idCount = 0;
        std::uint32_t classCount = 0;
        std::uint32_t typeCount = 0;

        bool IsEmpty() const { return simple.empty(); }
        bool NeedsRuntimeState() const;
        /// True when the compound carries a type or universal selector, which
        /// decides whether it may start a selector.
        bool HasTypeOrUniversal() const;
        std::string ToString() const;
    };

    /// A full selector: compounds left to right, with the combinator between
    /// each pair.
    struct CSSComplexSelector
    {
        std::vector<CSSCompoundSelector> compounds;
        /// `combinators[i]` relates `compounds[i]` to `compounds[i + 1]`.
        std::vector<CSSCombinator> combinators;

        std::uint32_t idCount = 0;
        std::uint32_t classCount = 0;
        std::uint32_t typeCount = 0;

        /// Which runtime state the selector depends on.
        bool needsHover = false;
        bool needsActive = false;
        bool needsFocus = false;
        bool needsChecked = false;
        bool needsDisabled = false;
        bool needsOpen = false;
        /// True when any part of the selector cannot be evaluated by this
        /// compiler. The rule is parsed, reported and skipped.
        bool supported = true;

        std::string text;

        /// (id, class, type) triple, which is what the cascade compares.
        std::uint32_t SpecificityId() const { return idCount; }
        std::uint32_t SpecificityClass() const { return classCount; }
        std::uint32_t SpecificityType() const { return typeCount; }
        /// Packed specificity for ordering: 10 bits per component.
        std::uint32_t PackedSpecificity() const
        {
            return (idCount << 20) | (classCount << 10) | (typeCount & 0x3FFu);
        }

        bool NeedsRuntimeState() const
        {
            return needsHover || needsActive || needsFocus || needsChecked || needsDisabled || needsOpen;
        }

        std::string ToString() const;
    };

    /// A parsed selector list: `a, b > c`.
    struct CSSSelectorList
    {
        std::vector<CSSComplexSelector> selectors;

        bool IsEmpty() const { return selectors.empty(); }
        bool AnyUnsupported() const;

        std::string ToString() const;
    };

    // -----------------------------------------------------------------------
    // Parsing and matching
    // -----------------------------------------------------------------------

    class CSSSelectorParser
    {
    public:
        explicit CSSSelectorParser(CSSDiagnosticSink& diagnostics);

        /// Parses a selector list. Unsupported parts are reported and marked,
        /// never silently dropped.
        CSSSelectorList Parse(const std::string& selectorText, const CSSSourceLocation& location);

    private:
        CSSDiagnosticSink& diagnostics;
    };

    /// True when `element` matches the selector.
    bool MatchSelector(const CSSComplexSelector& selector,
                       const CSSElementNode& element,
                       const CSSElementState* runtimeState = nullptr);

    /// The state a selector needs resolved, or `None` when it is static.
    CSSElementState StateForSelector(const CSSComplexSelector& selector);
}
