// ---------------------------------------------------------------------------
// RoboCSSCompiler - the cascade.
//
//     stylesheets + element tree  ->  one computed style per element
//
// This is where CSS stops being text. Every matching rule's declarations are
// collected with their specificity and source order, the cascade picks a winner
// per property, shorthand expansion has already turned `margin: 0 auto` into
// four longhands, `var()` has been substituted against the element's inherited
// custom properties, and the result is parsed against the property registry's
// grammar into a typed value.
//
// The output is a computed style: property -> CSSValue, with the property set
// deduplicated across elements. Nothing downstream ever sees a selector, a
// declaration or a CSS string again.
//
// What happens here is compile-time work that a browser does per frame: selector
// matching, specificity, the cascade, inheritance and shorthand expansion. That
// is the whole point of compiling CSS. What is *not* here is anything that
// depends on runtime information - percentage and viewport-unit resolution,
// layout, text measurement - which stays typed so the runtime can resolve it
// against the context only it knows.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboCSSCompiler/CssDiagnostics.h"
#include "RoboCSSCompiler/CssParser.h"
#include "RoboCSSCompiler/CssProperty.h"
#include "RoboCSSCompiler/CssSelector.h"
#include "RoboCSSCompiler/CssValue.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace RoboCSSCompiler
{
    /// Who wrote a stylesheet. The cascade orders origins, and an important user
    /// declaration beats an important author one, which is what makes a user
    /// stylesheet able to override a page.
    enum class CSSOrigin : std::uint8_t
    {
        UserAgent = 0,
        User = 1,
        Author = 2
    };

    const char* ToString(CSSOrigin origin);

    /// The computed value of the properties that apply to one element.
    ///
    /// Only specified properties are stored. A property that no rule set and
    /// that does not inherit is simply absent, which is what lets "the native
    /// default" be a real concept rather than a magic value: the renderer's
    /// default is used exactly when CSS said nothing.
    class CSSComputedStyle
    {
    public:
        /// The value of a property, or nullptr when nothing specified it.
        const CSSValue* Get(CSSPropertyId id) const;

        /// Stores a value, replacing an earlier one.
        void Set(CSSPropertyId id, CSSValue value);

        /// Removes a property, which is what `unset` does to an inherited
        /// property that the parent did not set.
        void Remove(CSSPropertyId id);

        bool Has(CSSPropertyId id) const { return Get(id) != nullptr; }

        /// Specified properties, ordered by property id.
        const std::vector<std::pair<CSSPropertyId, CSSValue>>& Values() const { return values; }

        /// Custom properties, which are inherited as a whole and substituted
        /// into `var()` during this element's own computation.
        void SetCustomProperty(const std::string& name, std::vector<CSSComponentValue> value);
        const std::vector<CSSComponentValue>* GetCustomProperty(const std::string& name) const;
        const std::map<std::string, std::vector<CSSComponentValue>>& CustomProperties() const
        {
            return customProperties;
        }

        /// A stable, order sensitive key over the whole computed style. Two
        /// elements with the same key have the same style, which is how the
        /// style table deduplicates them.
        std::string Key() const;

        /// Canonical CSS text, for diagnostics and for the generated
        /// stylesheet. Not a runtime concern.
        std::string ToString() const;

        /// True when nothing was specified at all.
        bool IsEmpty() const { return values.empty() && customProperties.empty(); }

    private:
        std::vector<std::pair<CSSPropertyId, CSSValue>> values;
        std::map<std::string, std::vector<CSSComponentValue>> customProperties;
    };

    /// What the cascade did, for the coverage report and the benchmark.
    struct CSSCascadeStatistics
    {
        std::size_t elementCount = 0;
        std::size_t ruleCount = 0;
        std::size_t matchedRuleCount = 0;
        std::size_t declarationCount = 0;
        std::size_t appliedDeclarationCount = 0;
        std::size_t overriddenDeclarationCount = 0;
        /// Declarations dropped because their value did not parse.
        std::size_t invalidDeclarationCount = 0;
        std::size_t skippedRuleCount = 0;
        /// Selectors whose state only exists at runtime (`:hover`).
        std::size_t runtimeStateRuleCount = 0;
        std::size_t uniqueStyleCount = 0;
        /// Selector tests performed while matching.
        std::size_t selectorComparisonCount = 0;
    };

    struct CSSCascadeResult
    {
        /// One entry per element, in document order. An element with no CSS is
        /// empty rather than absent, so the caller can index by element.
        std::vector<CSSComputedStyle> elementStyles;
        /// The elements the styles belong to, in the same order.
        std::vector<CSSElementNode*> elements;

        /// The distinct computed styles, and each element's index into them.
        /// `internedIndex[i]` is 0 when element `i` has no style at all.
        std::vector<CSSComputedStyle> internedStyles;
        std::vector<std::uint32_t> internedIndex;

        CSSCascadeStatistics statistics;

        std::size_t StyleCount() const { return internedStyles.size(); }
        std::size_t ElementCount() const { return elementStyles.size(); }

        /// The computed style of an element, or nullptr.
        const CSSComputedStyle* StyleForElement(std::size_t index) const
        {
            return index < elementStyles.size() ? &elementStyles[index] : nullptr;
        }
    };

    /// Options the compiler decides, not the stylesheet.
    struct CSSCascadeOptions
    {
        /// The viewport a media query is evaluated against. Media queries are
        /// evaluated during compilation against the application's target size;
        /// a query that depends on something the compiler cannot know is
        /// reported as unsupported rather than guessed.
        float viewportWidth = 1024.0f;
        float viewportHeight = 768.0f;

        /// The root font size used to resolve `rem` and the initial `font-size`.
        float rootFontSize = 16.0f;

        /// True when a stylesheet may override the compiler's native defaults
        /// from an author origin. Always true for a compiled application; the
        /// switch exists so a test can compute defaults alone.
        bool applyAuthorStyles = true;

        /// Reports every declaration that was overridden by a later one, which
        /// is noise in a normal build and useful in a coverage report.
        bool reportOverriddenDeclarations = false;
    };

    /// The cascade. Build once, add every stylesheet, then compute.
    class CSSCascade
    {
    public:
        explicit CSSCascade(CSSDiagnosticSink& diagnostics);

        /// Adds a parsed stylesheet. Author order is the order of the calls.
        void AddStyleSheet(const CSSStylesheet& stylesheet, CSSOrigin origin = CSSOrigin::Author);

        /// Adds the compiler's own user-agent stylesheet, built from the
        /// registry's initial values. Its declarations lose to every author
        /// declaration and beat nothing.
        void AddUserAgentStyleSheet(const CSSStylesheet& stylesheet);

        void SetOptions(const CSSCascadeOptions& value) { options = value; }
        const CSSCascadeOptions& Options() const { return options; }

        /// Computes one style per element of the tree, root first.
        CSSCascadeResult Compute(CSSElementNode& root);

        std::size_t StyleSheetCount() const { return styleSheets.size(); }

    private:
        struct StyleSheet
        {
            CSSOrigin origin = CSSOrigin::Author;
            CSSStylesheet sheet;
            std::uint32_t order = 0;
        };

        /// One declaration that matched an element, before the cascade ordered
        /// them. `value` points into the parsed stylesheet, which outlives the
        /// computation.
        struct MatchedDeclaration
        {
            const CSSDeclaration* declaration = nullptr;
            CSSOrigin origin = CSSOrigin::Author;
            std::uint32_t specificity = 0;
            std::uint32_t order = 0;
            bool important = false;
        };

        /// What the compiler decides about a rule once, before any element is
        /// matched against it: its parsed selectors, and whether the at-rule it
        /// lives under applies at all. Parsing a selector per element per rule
        /// would be the very thing compiling CSS exists to avoid.
        struct RuleCacheEntry
        {
            CSSSelectorList selectors;
            /// False when the rule is inside an `@media` that did not match, or an
            /// `@supports` that did not hold.
            bool conditionPasses = true;
        };

        using RuleCache = std::map<const CSSRule*, RuleCacheEntry>;

        /// One rule to test, with its parsed selectors already resolved.
        ///
        /// The cache answers "what did this rule compile to" once. This list is
        /// that answer, flattened into the order the cascade walks: matching an
        /// element against the stylesheet is a walk over a contiguous list, not a
        /// lookup per rule per element. The distinction is not a detail - a
        /// document of a thousand elements against a thousand rules performs a
        /// million of these, and a map lookup apiece dominated the compile.
        struct FlattenedRule
        {
            const CSSRule* rule = nullptr;
            const RuleCacheEntry* entry = nullptr;
        };

        void BuildRuleCache(const std::vector<CSSRule>& rules, RuleCache& cache, bool conditionPasses,
                            CSSCascadeStatistics& statistics);

        /// Flattens the rules of one stylesheet, dropping at-rule wrappers and
        /// every rule inside an at-rule that did not apply.
        void FlattenRules(const std::vector<CSSRule>& rules, const RuleCache& cache,
                          std::vector<FlattenedRule>& out);

        void CollectMatchingDeclarations(CSSElementNode& element,
                                         std::vector<MatchedDeclaration>& out,
                                         const std::vector<std::vector<FlattenedRule>>& sheetRules,
                                         CSSCascadeStatistics& statistics);

        void ComputeElement(const CSSComputedStyle* parent,
                            const std::vector<MatchedDeclaration>& matched,
                            CSSComputedStyle& out);

        /// Expands a shorthand into its longhands, appending them to `out`.
        /// Returns false when the shorthand could not be expanded, in which
        /// case the diagnostic has already been reported.
        bool ExpandShorthand(const CSSShorthandDefinition& shorthand,
                             const std::vector<CSSComponentValue>& value,
                             std::vector<std::pair<CSSPropertyId, CSSValue>>& out,
                             const CSSSourceLocation& location);

        /// Applies one longhand declaration to the element's style, handling the
        /// CSS-wide keywords, `var()` substitution and grammar errors.
        void ApplyLonghand(CSSPropertyId id,
                           const CSSDeclaration& declaration,
                           const CSSComputedStyle* parent,
                           CSSComputedStyle& out,
                           bool& applied);

        /// Expands `all: <css-wide-keyword>` over every property the registry
        /// carries, which is the one declaration whose effect is the whole style.
        void ApplyAll(const CSSDeclaration& declaration, const CSSComputedStyle* parent, CSSComputedStyle& out);

        CSSDiagnosticSink& diagnostics;
        CSSCascadeOptions options;

        std::vector<StyleSheet> styleSheets;
        std::uint32_t sheetOrder = 0;

        /// The initial value of every property, parsed once from the registry.
        std::vector<CSSValue> initialValues;
        std::vector<std::uint8_t> hasInitialValue;
    };

    /// Builds the user-agent stylesheet a native application starts from: the
    /// registry's initial values, expressed as declarations so that there is
    /// exactly one style-resolution path.
    std::string GenerateUserAgentStyleSheet();

    /// The coverage report: which registry properties the compiler implements,
    /// which the reference defines but the compiler does not, and what the
    /// generated stylesheet text is.
    struct CSSCoverageReport
    {
        std::size_t implementedPropertyCount = 0;
        std::size_t unsupportedPropertyCount = 0;
        std::size_t shorthandCount = 0;
        std::size_t selectorCount = 0;
        std::size_t unitCount = 0;
        std::size_t pseudoClassCount = 0;
        std::vector<std::string> implemented;
        std::vector<std::string> unsupported;
        std::vector<std::string> shorthands;

        std::string ToMarkdown() const;
    };

    CSSCoverageReport BuildCoverageReport();
}
