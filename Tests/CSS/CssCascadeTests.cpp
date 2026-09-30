// ---------------------------------------------------------------------------
// CSS tests.
//
// The suites are ordered the way the compiler is: the tokenizer and parser,
// then typed values, then the cascade that turns them into computed styles.
// A failure in one of them points at the stage that regressed rather than at
// "CSS".
// ---------------------------------------------------------------------------
#include "../TestFramework.h"
#include "../TestHelpers.h"

#include "RoboCSSCompiler/CssCascade.h"
#include "RoboCSSCompiler/CssParser.h"
#include "RoboCSSCompiler/CssSyntax.h"
#include "RoboCSSCompiler/CssValue.h"
#include "RoboCSSCompiler/CssValueParser.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace RoboCSSCompiler;
using namespace RoboTest;

namespace
{
    /// A small element tree builder: the cascade takes a tree, not text, so the
    /// tests build one.
    struct TreeBuilder
    {
        std::vector<std::unique_ptr<CSSElementNode>> pool;

        CSSElementNode& Add(CSSElementNode* parent, const std::string& tag, const std::string& id,
                            const std::string& classes, const std::string& text = std::string())
        {
            pool.push_back(std::make_unique<CSSElementNode>());
            CSSElementNode& node = *pool.back();
            node.tagName = tag;
            node.id = id;

            std::string token;
            for (std::size_t index = 0; index <= classes.size(); ++index)
            {
                if (index == classes.size() || classes[index] == ' ')
                {
                    if (!token.empty())
                    {
                        node.classes.push_back(token);
                    }
                    token.clear();
                    continue;
                }

                token += classes[index];
            }

            node.textContent = text;

            // A real element carries the same information in its attribute list
            // as in its parsed class list, and attribute selectors read the
            // former.
            if (!classes.empty())
            {
                node.attributes.emplace_back("class", classes);
            }

            if (!id.empty())
            {
                node.attributes.emplace_back("id", id);
            }

            if (parent != nullptr)
            {
                node.parent = parent;
                parent->children.push_back(&node);
            }

            return node;
        }

        /// Numbers sibling positions and document order, which structural
        /// pseudo-classes need.
        void Number(CSSElementNode& root)
        {
            CSSElementNode::NumberSiblings(root);
        }
    };

    /// The computed value of a property, as CSS text, or an empty string.
    std::string ComputedText(const CSSComputedStyle* style, CSSPropertyId id)
    {
        if (style == nullptr)
        {
            return std::string();
        }

        const CSSValue* value = style->Get(id);
        return value != nullptr ? value->ToString() : std::string();
    }

    bool HasCode(const CSSDiagnosticSink& diagnostics, CSSDiagnosticCode code)
    {
        for (const CSSDiagnostic& diagnostic : diagnostics.Entries())
        {
            if (diagnostic.code == code)
            {
                return true;
            }
        }

        return false;
    }
}

// ---------------------------------------------------------------------------
// Tokenizer and parser
// ---------------------------------------------------------------------------

ROBO_TEST(CssSyntax, TokensCoverTheSpecifiedForms)
{
    CSSDiagnosticSink diagnostics;
    CSSTokenizer tokenizer("a#b.c:hover { width: 50%; margin: -1.5e2px; background: url(x.png) }", "t.css",
                           &diagnostics);
    const std::vector<CSSToken> tokens = tokenizer.Tokenize();

    bool sawPercentage = false;
    bool sawDimension = false;
    bool sawUrl = false;

    for (const CSSToken& token : tokens)
    {
        if (token.type == CSSTokenType::Percentage)
        {
            sawPercentage = true;
            ROBO_CHECK(std::fabs(token.number - 50.0) < 0.001);
        }

        if (token.type == CSSTokenType::Dimension)
        {
            sawDimension = true;
            ROBO_CHECK_EQ(token.unit, std::string("px"));
            ROBO_CHECK(std::fabs(token.number - -150.0) < 0.001);
        }

        if (token.type == CSSTokenType::Url)
        {
            sawUrl = true;
            ROBO_CHECK_EQ(token.text, std::string("x.png"));
        }
    }

    ROBO_CHECK(sawPercentage);
    ROBO_CHECK(sawDimension);
    ROBO_CHECK(sawUrl);
    ROBO_CHECK(!diagnostics.HasErrors());
}

ROBO_TEST(CssParser, StylesheetKeepsRulesAndComponentValues)
{
    CSSDiagnosticSink diagnostics;
    const CSSStylesheet sheet = CSSParser(diagnostics).ParseStylesheet(
        "t.css", "/* c */ .a, .b { color: red; margin: 0 auto } @media screen { .c { top: 0 } }");

    ROBO_CHECK_EQ(sheet.rules.size(), static_cast<std::size_t>(2));
    ROBO_CHECK(sheet.rules[0].kind == CSSRule::Kind::Qualified);
    ROBO_CHECK_EQ(sheet.rules[0].selectors.size(), static_cast<std::size_t>(2));
    ROBO_CHECK_EQ(sheet.rules[0].declarations.size(), static_cast<std::size_t>(2));
    ROBO_CHECK(sheet.rules[1].kind == CSSRule::Kind::At);
    ROBO_CHECK_EQ(sheet.rules[1].at.name, std::string("media"));
    ROBO_CHECK_EQ(sheet.rules[1].at.children.size(), static_cast<std::size_t>(1));
}

ROBO_TEST(CssParser, WhitespaceOnlyValueIsReportedAsMissing)
{
    CSSDiagnosticSink diagnostics;
    const CSSStylesheet sheet =
        CSSParser(diagnostics).ParseStylesheet("t.css", ".a { color: ; margin:   ; padding: 1px }");

    // `color` and `margin` have no value; `padding` does. The check that decides
    // this looks at the tokens rather than building the value's text, so the two
    // shapes - immediately empty, and whitespace only - have to stay the same
    // answer.
    ROBO_CHECK_EQ(sheet.rules.size(), static_cast<std::size_t>(1));
    ROBO_CHECK_EQ(sheet.rules[0].declarations.size(), static_cast<std::size_t>(1));
    ROBO_CHECK_EQ(sheet.rules[0].declarations[0].property, std::string("padding"));
    ROBO_CHECK(diagnostics.ErrorCount() == 2);
}

// ---------------------------------------------------------------------------
// Typed values
// ---------------------------------------------------------------------------

ROBO_TEST(CssValue, UnitsKeepTheirFamily)
{
    CSSUnit unit = CSSUnit::None;

    ROBO_CHECK(ParseUnit("vw", unit));
    ROBO_CHECK(unit == CSSUnit::Vw);
    ROBO_CHECK(UnitFamily(unit) == CSSUnitFamily::Length);
    ROBO_CHECK(UnitContext(unit) == CSSUnitContext::ViewportRelative);
    ROBO_CHECK(!IsAbsoluteUnit(unit));

    ROBO_CHECK(ParseUnit("rem", unit));
    ROBO_CHECK(UnitContext(unit) == CSSUnitContext::RootFontRelative);

    ROBO_CHECK(ParseUnit("deg", unit));
    ROBO_CHECK(UnitFamily(unit) == CSSUnitFamily::Angle);

    ROBO_CHECK(!ParseUnit("furlong", unit));
}

ROBO_TEST(CssValue, ColorsConvertToSrgb)
{
    CSSColor color;

    ROBO_CHECK(ParseHexColor("2563eb", color));
    ROBO_CHECK_EQ(static_cast<int>(color.red), 0x25);
    ROBO_CHECK_EQ(static_cast<int>(color.green), 0x63);
    ROBO_CHECK_EQ(static_cast<int>(color.blue), 0xeb);

    ROBO_CHECK(ParseNamedColor("rebeccapurple", color));
    ROBO_CHECK_EQ(color.Packed(), static_cast<std::uint32_t>(0xFF663399u));

    // A percentage is a percentage, and a number is a byte.
    CSSDiagnosticSink diagnostics;
    CSSValue value;
    std::vector<CSSDeclaration> declarations =
        CSSParser(diagnostics).ParseDeclarationList("t.css", "color: rgb(100% 0% 0% / 50%)");
    ROBO_CHECK(!declarations.empty());

    const CSSPropertyDefinition* definition = CSSPropertyRegistry::Instance().Find(CSSPropertyId::Color);
    ROBO_CHECK(definition != nullptr);
    ROBO_CHECK(ParsePropertyValue(*definition, declarations[0].value, value, declarations[0].location, diagnostics));
    ROBO_CHECK(value.kind == CSSValueKind::Color);
    ROBO_CHECK_EQ(static_cast<int>(value.color.red), 255);
    ROBO_CHECK(std::fabs(value.color.alpha - 0.5f) < 0.01f);
}

ROBO_TEST(CssValue, CalcFoldsWhenItCan)
{
    CSSDiagnosticSink diagnostics;
    CSSValue value;
    std::vector<CSSDeclaration> declarations =
        CSSParser(diagnostics).ParseDeclarationList("t.css", "width: calc(100px - 20px)");

    ROBO_CHECK(!declarations.empty());
    const CSSPropertyDefinition* definition = CSSPropertyRegistry::Instance().Find(CSSPropertyId::Width);
    ROBO_CHECK(ParsePropertyValue(*definition, declarations[0].value, value, declarations[0].location, diagnostics));
    ROBO_CHECK(value.kind == CSSValueKind::Dimension);
    ROBO_CHECK(std::fabs(value.number - 80.0) < 0.001);
    ROBO_CHECK(value.unit == CSSUnit::Px);
}

ROBO_TEST(CssValue, CalcThatMixesDimensionsIsReported)
{
    CSSDiagnosticSink diagnostics;
    CSSValue value;
    std::vector<CSSDeclaration> declarations =
        CSSParser(diagnostics).ParseDeclarationList("t.css", "width: calc(100% - 20px)");

    ROBO_CHECK(!declarations.empty());
    const CSSPropertyDefinition* definition = CSSPropertyRegistry::Instance().Find(CSSPropertyId::Width);

    // A native width carries one dimension, so an expression mixing a
    // percentage with a length is reported rather than silently resolved.
    ROBO_CHECK(!ParsePropertyValue(*definition, declarations[0].value, value, declarations[0].location, diagnostics));
    ROBO_CHECK(HasCode(diagnostics, CSSDiagnosticCode::UnsupportedFeature));
}

ROBO_TEST(CssValue, InvalidValuesAreErrors)
{
    CSSDiagnosticSink diagnostics;
    CSSValue value;
    std::vector<CSSDeclaration> declarations =
        CSSParser(diagnostics).ParseDeclarationList("t.css", "width: completely-invalid-value");

    ROBO_CHECK(!declarations.empty());
    const CSSPropertyDefinition* definition = CSSPropertyRegistry::Instance().Find(CSSPropertyId::Width);
    ROBO_CHECK(!ParsePropertyValue(*definition, declarations[0].value, value, declarations[0].location, diagnostics));
    ROBO_CHECK(diagnostics.HasErrors());
}

// ---------------------------------------------------------------------------
// The cascade
// ---------------------------------------------------------------------------

namespace
{
    /// Runs one stylesheet against a two level tree: `root` (html) with a `body`,
    /// which is what most of the cascade tests need.
    struct Fixture
    {
        CSSDiagnosticSink diagnostics;
        TreeBuilder tree;
        CSSElementNode* html = nullptr;
        CSSElementNode* body = nullptr;
        CSSCascadeResult result;
        CSSStylesheet sheet;

        void Build(const std::string& css, const std::string& bodyClasses = std::string(),
                   const std::string& bodyId = std::string())
        {
            html = &tree.Add(nullptr, "html", std::string(), std::string());
            body = &tree.Add(html, "body", bodyId, bodyClasses);
            tree.Number(*html);

            sheet = CSSParser(diagnostics).ParseStylesheet("test.css", css);

            CSSCascade cascade(diagnostics);
            cascade.AddStyleSheet(sheet);
            result = cascade.Compute(*html);
        }

        const CSSComputedStyle* Body() const
        {
            return result.elementStyles.size() > 1 ? &result.elementStyles[1] : nullptr;
        }

        const CSSComputedStyle* Html() const
        {
            return result.elementStyles.empty() ? nullptr : &result.elementStyles[0];
        }
    };
}

ROBO_TEST(CssCascade, HigherSpecificityWins)
{
    Fixture fixture;
    fixture.Build("body { color: red } html { color: blue } .page { color: green }",
                  /*bodyClasses=*/"page");

    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#008000"));
}

ROBO_TEST(CssCascade, SourceOrderBreaksTies)
{
    Fixture fixture;
    fixture.Build("body { color: red } body { color: blue }");

    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#0000ff"));
}

ROBO_TEST(CssCascade, ImportantBeatsSpecificity)
{
    Fixture fixture;
    fixture.Build("body { color: red !important } #page { color: blue }", std::string(), /*bodyId=*/"page");

    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#ff0000"));
}

ROBO_TEST(CssCascade, InheritedPropertiesReachChildrenAndOthersDoNot)
{
    Fixture fixture;
    fixture.Build("html { color: red; padding: 2rem }");

    // `color` inherits; `padding` does not.
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#ff0000"));
    ROBO_CHECK(fixture.Body() != nullptr && !fixture.Body()->Has(CSSPropertyId::PaddingTop));
}

ROBO_TEST(CssCascade, InitialInheritAndUnsetBehave)
{
    Fixture fixture;
    fixture.Build("html { color: red } body { color: initial }");

    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#000000"));

    Fixture second;
    second.Build("html { color: red } body { color: inherit }");
    ROBO_CHECK_EQ(ComputedText(second.Body(), CSSPropertyId::Color), std::string("#ff0000"));
}

ROBO_TEST(CssCascade, ShorthandsExpandToLonghands)
{
    Fixture fixture;
    fixture.Build("body { margin: 0 auto; padding: 1rem 2rem 3rem 4rem }");

    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::MarginTop), std::string("0px"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::MarginRight), std::string("auto"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::MarginBottom), std::string("0px"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::MarginLeft), std::string("auto"));

    // A relative unit is kept as a unit: `rem` is not a number the compiler may
    // flatten, because the runtime resolves it against the root font size.
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::PaddingTop), std::string("1rem"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::PaddingRight), std::string("2rem"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::PaddingBottom), std::string("3rem"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::PaddingLeft), std::string("4rem"));
}

ROBO_TEST(CssCascade, BorderShorthandSetsWidthStyleAndColor)
{
    Fixture fixture;
    fixture.Build("body { border: 2px dashed #123456 }");

    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::BorderTopWidth), std::string("2px"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::BorderLeftStyle), std::string("dashed"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::BorderBottomColor), std::string("#123456"));

    // A width is a width, whatever position it appears in.
    Fixture reordered;
    reordered.Build("body { border: solid 4px blue }");
    ROBO_CHECK_EQ(ComputedText(reordered.Body(), CSSPropertyId::BorderRightWidth), std::string("4px"));
    ROBO_CHECK_EQ(ComputedText(reordered.Body(), CSSPropertyId::BorderRightStyle), std::string("solid"));
}

ROBO_TEST(CssCascade, CustomPropertiesSubstituteAndInherit)
{
    Fixture fixture;
    fixture.Build(":root { --primary: #2563eb } body { color: var(--primary) }");

    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#2563eb"));

    Fixture fallback;
    fallback.Build("body { color: var(--missing, #00ff00) }");
    ROBO_CHECK_EQ(ComputedText(fallback.Body(), CSSPropertyId::Color), std::string("#00ff00"));

    // An undefined custom property without a fallback is an error, never a
    // silently dropped declaration.
    Fixture undefined;
    undefined.Build("body { color: var(--missing) }");
    ROBO_CHECK(undefined.diagnostics.HasErrors());
    ROBO_CHECK(!undefined.Body()->Has(CSSPropertyId::Color));
}

ROBO_TEST(CssCascade, StructuralPseudoClassesMatchAtCompileTime)
{
    CSSDiagnosticSink diagnostics;
    TreeBuilder tree;
    CSSElementNode& html = tree.Add(nullptr, "html", std::string(), std::string());

    // Three children: two paragraphs and a div, which is what the structural
    // pseudo-classes below are written against. Only their position matters.
    tree.Add(&html, "p", std::string(), "a");
    tree.Add(&html, "p", std::string(), "b");
    tree.Add(&html, "div", std::string(), "c");
    tree.Number(html);

    const CSSStylesheet sheet = CSSParser(diagnostics).ParseStylesheet(
        "t.css", "p:first-child { color: red } p:nth-child(2) { color: blue } div:last-child { color: green }");

    CSSCascade cascade(diagnostics);
    cascade.AddStyleSheet(sheet);
    const CSSCascadeResult result = cascade.Compute(html);

    ROBO_CHECK_EQ(result.elementStyles.size(), static_cast<std::size_t>(4));
    ROBO_CHECK_EQ(ComputedText(&result.elementStyles[1], CSSPropertyId::Color), std::string("#ff0000"));
    ROBO_CHECK_EQ(ComputedText(&result.elementStyles[2], CSSPropertyId::Color), std::string("#0000ff"));
    ROBO_CHECK_EQ(ComputedText(&result.elementStyles[3], CSSPropertyId::Color), std::string("#008000"));
}

ROBO_TEST(CssCascade, AttributeAndDescendantSelectorsMatch)
{
    CSSDiagnosticSink diagnostics;
    TreeBuilder tree;
    CSSElementNode& html = tree.Add(nullptr, "html", std::string(), std::string());
    CSSElementNode& body = tree.Add(&html, "body", std::string(), "page wide");
    CSSElementNode& first = tree.Add(&body, "p", std::string(), "note");
    CSSElementNode& second = tree.Add(&body, "span", std::string(), "note");
    (void)first;
    (void)second;
    tree.Number(html);

    const CSSStylesheet sheet = CSSParser(diagnostics).ParseStylesheet(
        "t.css", "body.wide > p { color: red } .note { color: blue } p[class] { background-color: black }");

    CSSCascade cascade(diagnostics);
    cascade.AddStyleSheet(sheet);
    const CSSCascadeResult result = cascade.Compute(html);

    ROBO_CHECK(!diagnostics.HasErrors());

    // The `p` is matched by all three rules. `body.wide > p` has one class and two
    // type selectors and `.note` has one class alone, so the descendant rule wins
    // the colour, while the attribute selector's background is uncontested.
    ROBO_CHECK_EQ(ComputedText(&result.elementStyles[2], CSSPropertyId::Color), std::string("#ff0000"));
    ROBO_CHECK_EQ(ComputedText(&result.elementStyles[2], CSSPropertyId::BackgroundColor), std::string("#000000"));

    // The `span` is not a `p`, so neither the descendant rule nor the attribute
    // rule applies to it: only the class does.
    ROBO_CHECK_EQ(ComputedText(&result.elementStyles[3], CSSPropertyId::Color), std::string("#0000ff"));
    ROBO_CHECK(!result.elementStyles[3].Has(CSSPropertyId::BackgroundColor));

    // The body is matched by the descendant rule's own class, but a rule's
    // declarations apply to the element the *rightmost* compound matches, so the
    // body itself carries nothing.
    ROBO_CHECK(!result.elementStyles[1].Has(CSSPropertyId::Color));
}

ROBO_TEST(CssCascade, MediaQueriesAreEvaluatedDuringCompilation)
{
    Fixture fixture;
    fixture.Build("@media (min-width: 100px) { body { color: red } } "
                  "@media (min-width: 100000px) { body { background-color: blue } }");

    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#ff0000"));
    ROBO_CHECK(!fixture.Body()->Has(CSSPropertyId::BackgroundColor));
}

ROBO_TEST(CssCascade, SupportsIsAnsweredByTheValueParser)
{
    Fixture fixture;
    fixture.Build("@supports (display: block) { body { color: red } } "
                  "@supports (display: grid) { body { background-color: blue } }");

    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#ff0000"));
    ROBO_CHECK(!fixture.Body()->Has(CSSPropertyId::BackgroundColor));
}

ROBO_TEST(CssCascade, UnsupportedFeaturesAreReportedNotApplied)
{
    Fixture fixture;
    fixture.Build("body { position: fixed; color: red; whats-it: 3 }");

    // A real property the compiler does not compile: RCC010.
    ROBO_CHECK(HasCode(fixture.diagnostics, CSSDiagnosticCode::UnsupportedFeature));
    // A name that is not a property at all: RCC001.
    ROBO_CHECK(HasCode(fixture.diagnostics, CSSDiagnosticCode::UnknownProperty));
    // What did parse still applied.
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#ff0000"));
}

ROBO_TEST(CssCascade, InvalidValuesFailTheDeclaration)
{
    Fixture fixture;
    fixture.Build("body { width: banana }");

    ROBO_CHECK(fixture.diagnostics.HasErrors());
    ROBO_CHECK(!fixture.Body()->Has(CSSPropertyId::Width));
}

ROBO_TEST(CssCascade, IdenticalStylesAreDeduplicated)
{
    CSSDiagnosticSink diagnostics;
    TreeBuilder tree;
    CSSElementNode& html = tree.Add(nullptr, "html", std::string(), std::string());
    CSSElementNode& first = tree.Add(&html, "p", std::string(), "note");
    CSSElementNode& second = tree.Add(&html, "p", std::string(), "note");
    CSSElementNode& third = tree.Add(&html, "p", std::string(), "other");
    (void)first;
    (void)second;
    (void)third;
    tree.Number(html);

    const CSSStylesheet sheet =
        CSSParser(diagnostics).ParseStylesheet("t.css", ".note { color: red; padding: 1px } .other { color: blue }");

    CSSCascade cascade(diagnostics);
    cascade.AddStyleSheet(sheet);
    const CSSCascadeResult result = cascade.Compute(html);

    // Two `.note` elements share one interned style; `.other` gets its own.
    ROBO_CHECK_EQ(result.StyleCount(), static_cast<std::size_t>(2));
    ROBO_CHECK_EQ(result.internedIndex[1], result.internedIndex[2]);
    ROBO_CHECK_NE(result.internedIndex[1], result.internedIndex[3]);
    ROBO_CHECK_EQ(result.statistics.uniqueStyleCount, static_cast<std::size_t>(2));
}

ROBO_TEST(CssCascade, ElementsWithNoStyleShareNoInternedEntry)
{
    Fixture fixture;
    fixture.Build("body { color: red }");

    // The `html` element matched nothing and inherits nothing, so its style is
    // empty and it carries no interned entry at all.
    ROBO_CHECK(fixture.Html() != nullptr);
    ROBO_CHECK(fixture.Html()->IsEmpty());
    ROBO_CHECK_EQ(fixture.result.internedIndex[0], static_cast<std::uint32_t>(0));
}

// ---------------------------------------------------------------------------
// Reference coverage
// ---------------------------------------------------------------------------

ROBO_TEST(CssCoverage, ReportNamesWhatIsImplemented)
{
    const CSSCoverageReport report = BuildCoverageReport();

    ROBO_CHECK(report.implementedPropertyCount > 20);
    ROBO_CHECK(report.shorthandCount > 8);
    ROBO_CHECK(report.unitCount > 40);
    ROBO_CHECK(report.unsupportedPropertyCount > 100);

    const std::string markdown = report.ToMarkdown();
    ROBO_CHECK(markdown.find("CSS coverage") != std::string::npos);
    ROBO_CHECK(markdown.find("`background-color`") != std::string::npos);
    ROBO_CHECK(markdown.find("`grid-template-columns`") != std::string::npos);
}

ROBO_TEST(CssCoverage, EveryInitialValueParses)
{
    // A registry whose initial value the value parser would reject is a registry
    // that lies about a property's default, so each one is parsed here with the
    // same parser a declaration uses.
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();

    for (const CSSPropertyDefinition& definition : registry.Properties())
    {
        CSSDiagnosticSink diagnostics;
        std::vector<CSSDeclaration> declarations =
            CSSParser(diagnostics).ParseDeclarationList("<initial>", std::string(definition.name) + ": " +
                                                                             definition.initialValue);

        ROBO_CHECK(!declarations.empty());

        if (declarations.empty())
        {
            continue;
        }

        CSSValue parsed;
        const bool ok = ParsePropertyValue(definition, declarations.front().value, parsed,
                                           declarations.front().location, diagnostics);

        ROBO_CHECK(ok);
        ROBO_CHECK(!diagnostics.HasErrors());
    }
}

ROBO_TEST(CssCoverage, TheNativeBaselineIsValidCss)
{
    const std::string text = GenerateUserAgentStyleSheet();

    CSSDiagnosticSink diagnostics;
    const CSSStylesheet sheet = CSSParser(diagnostics).ParseStylesheet("<native-defaults>", text);

    ROBO_CHECK_EQ(sheet.rules.size(), static_cast<std::size_t>(1));
    ROBO_CHECK(!diagnostics.HasErrors());

    // Every declaration it writes names a property the registry knows.
    for (const CSSDeclaration& declaration : sheet.rules[0].declarations)
    {
        const bool known = CSSPropertyRegistry::Instance().Find(declaration.property) != nullptr;
        ROBO_CHECK(known);

        if (!known)
        {
            continue;
        }

        const CSSPropertyDefinition* definition =
            CSSPropertyRegistry::Instance().Find(declaration.property);
        CSSValue parsed;
        ROBO_CHECK(ParsePropertyValue(*definition, declaration.value, parsed, declaration.location, diagnostics));
    }
}
