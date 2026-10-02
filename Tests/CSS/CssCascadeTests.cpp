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

#include <algorithm>
#include <cmath>
#include <cstring>
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

ROBO_TEST(CssValue, ACarriedValueIsKeptAsWritten)
{
    // `transition` is defined by the reference and this compiler carries it: it
    // has no grammar to judge the value against, so it keeps the tokens the
    // author wrote rather than refusing a declaration that is valid CSS.
    const CSSPropertyDefinition* definition = CSSPropertyRegistry::Instance().Find("transition");
    ROBO_CHECK(definition != nullptr);
    ROBO_CHECK(definition->dynamic);
    ROBO_CHECK(definition->grammar == CSSValueGrammar::Dynamic);

    CSSDiagnosticSink diagnostics;
    CSSValue value;
    std::vector<CSSDeclaration> declarations =
        CSSParser(diagnostics).ParseDeclarationList("t.css", "transition: opacity 1s ease-in 0.5s");

    ROBO_CHECK(!declarations.empty());
    ROBO_CHECK(ParsePropertyValue(*definition, declarations[0].value, value, declarations[0].location,
                                 diagnostics));
    ROBO_CHECK(!diagnostics.HasErrors());
    ROBO_CHECK(value.kind == CSSValueKind::List);
    ROBO_CHECK_EQ(value.items.size(), static_cast<std::size_t>(4));
    ROBO_CHECK_EQ(value.ToString(), std::string("opacity 1s ease-in 0.5s"));

    // The order is part of the value: a different order is a different style, so
    // the interning key has to see the difference.
    CSSValue reordered;
    std::vector<CSSDeclaration> other =
        CSSParser(diagnostics).ParseDeclarationList("t.css", "transition: 1s opacity ease-in 0.5s");

    ROBO_CHECK(!other.empty());
    ROBO_CHECK(ParsePropertyValue(*definition, other[0].value, reordered, other[0].location, diagnostics));
    ROBO_CHECK(reordered != value);

    // What no value of any property can be is nothing at all.
    CSSDiagnosticSink emptySink;
    CSSValue empty = CSSValue();
    ROBO_CHECK(!ParsePropertyValue(*definition, std::vector<CSSComponentValue>(), empty,
                                   CSSSourceLocation{}, emptySink));
    ROBO_CHECK(emptySink.HasErrors());
}

ROBO_TEST(CssValue, ACompiledPropertyStillRefusesAnInvalidValue)
{
    // Carried properties keep what they are given; the compiled ones stay
    // strict, because a value the runtime cannot draw is a real defect.
    const CSSPropertyDefinition* width = CSSPropertyRegistry::Instance().Find("width");
    ROBO_CHECK(width != nullptr);
    ROBO_CHECK(!width->dynamic);

    CSSDiagnosticSink diagnostics;
    CSSValue value;
    std::vector<CSSDeclaration> declarations =
        CSSParser(diagnostics).ParseDeclarationList("t.css", "width: 50%");

    ROBO_CHECK(!declarations.empty());
    ROBO_CHECK(ParsePropertyValue(*width, declarations[0].value, value, declarations[0].location, diagnostics));
    ROBO_CHECK(value.kind == CSSValueKind::Percentage);
    ROBO_CHECK(value.IsRuntimeDependent());
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
    // `touch-action` is a real CSS property whose specification is outside the
    // reference, so it is reported as unimplemented rather than mistaken for a
    // typo. A property the reference defines is carried instead - see
    // ACarriedPropertyIsAppliedAndKept.
    fixture.Build("body { touch-action: none; color: red; whats-it: 3 }");

    // A real property the compiler does not compile: RCC010.
    ROBO_CHECK(HasCode(fixture.diagnostics, CSSDiagnosticCode::UnsupportedFeature));
    // A name that is not a property at all: RCC001.
    ROBO_CHECK(HasCode(fixture.diagnostics, CSSDiagnosticCode::UnknownProperty));
    // What did parse still applied.
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#ff0000"));
}

ROBO_TEST(CssCascade, ACarriedPropertyIsAppliedAndKept)
{
    Fixture fixture;
    fixture.Build("body { animation-name: fade; color: red }");

    // `animation-name` is defined by the reference and this compiler carries it:
    // the declaration is valid CSS, so nothing is reported, and the value is
    // kept on the computed style for the runtime to use.
    ROBO_CHECK(!fixture.diagnostics.HasErrors());
    ROBO_CHECK(!HasCode(fixture.diagnostics, CSSDiagnosticCode::UnsupportedFeature));
    ROBO_CHECK(!HasCode(fixture.diagnostics, CSSDiagnosticCode::UnknownProperty));

    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();
    const CSSPropertyDefinition* definition = registry.Find("animation-name");

    ROBO_CHECK(definition != nullptr);
    ROBO_CHECK(definition->dynamic);
    ROBO_CHECK(registry.FindRegistered("animation-name") == definition);

    // The registration is what places the name: the module that defines it, and
    // the initial value and inheritance it stated.
    ROBO_CHECK(std::strcmp(definition->spec, "css-animations-1") == 0);
    ROBO_CHECK_EQ(std::string(definition->initialValue), std::string("none"));
    ROBO_CHECK(!definition->inherited);

    // Carried and compiled are different promises: the value is stored, but no
    // effect flag claims the runtime can draw it.
    ROBO_CHECK(!definition->supported);
    ROBO_CHECK(!definition->affectsLayout);
    ROBO_CHECK(!definition->affectsPaint);
    ROBO_CHECK(!definition->affectsText);
    ROBO_CHECK(!definition->affectsHitTesting);

    const CSSValue* value = fixture.Body()->Get(definition->id);
    ROBO_CHECK(value != nullptr);
    ROBO_CHECK(value->IsIdent("fade"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), definition->id), std::string("fade"));

    // What did compile still compiled.
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#ff0000"));
}

ROBO_TEST(CssCascade, ACarriedPropertyInheritsWhenTheReferenceSaysSo)
{
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();
    const CSSPropertyDefinition* visibility = registry.Find("visibility");

    ROBO_CHECK(visibility != nullptr);
    ROBO_CHECK(visibility->dynamic);
    ROBO_CHECK(visibility->inherited);

    Fixture fixture;
    fixture.Build("html { visibility: hidden }");

    ROBO_CHECK_EQ(ComputedText(fixture.Html(), visibility->id), std::string("hidden"));

    // The cascade copies it down: a carried property inherits because the
    // specification says it does, not because the compiler draws it.
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), visibility->id), std::string("hidden"));
    ROBO_CHECK(!fixture.diagnostics.HasErrors());
}

ROBO_TEST(CssCascade, AllDoesNotInventValuesForCarriedProperties)
{
    const CSSPropertyDefinition* animation = CSSPropertyRegistry::Instance().Find("animation-name");
    ROBO_CHECK(animation != nullptr);

    Fixture fixture;
    fixture.Build("body { all: initial }");

    // `all` resets the properties the compiler compiles. It does not materialise
    // an initial value for each of the hundreds it carries: nothing lowers them,
    // and putting them on every element would bloat every interned style for no
    // effect.
    ROBO_CHECK(!fixture.Body()->Has(animation->id));
    ROBO_CHECK(fixture.Body()->Values().size() < 100);
    ROBO_CHECK(!fixture.diagnostics.HasErrors());
}

ROBO_TEST(CssCascade, ACarriedDeclarationDoesNotDisturbTheCompiledOnes)
{
    Fixture fixture;
    fixture.Build("body { animation-name: fade; display: block; width: 50%; color: red }");

    // A carried declaration sits beside the compiled ones without changing them,
    // and a percentage width stays a percentage: it is resolved by the runtime.
    ROBO_CHECK(!fixture.diagnostics.HasErrors());
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#ff0000"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Display), std::string("block"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Width), std::string("50%"));
}

ROBO_TEST(CssCascade, UnknownPropertySuggestsTheNearbyName)
{
    Fixture fixture;
    fixture.Build("body { colour: red }");

    ROBO_CHECK(fixture.diagnostics.HasErrors());
    ROBO_CHECK(HasCode(fixture.diagnostics, CSSDiagnosticCode::UnknownProperty));

    // `colour` is a near miss for `color`: the report has to name both the
    // name that was written and the one the reader probably meant, because
    // "there is no such property" alone leaves them to find it themselves.
    std::string message;
    for (const CSSDiagnostic& entry : fixture.diagnostics.Entries())
    {
        if (entry.code == CSSDiagnosticCode::UnknownProperty)
        {
            message = entry.message;
            break;
        }
    }

    ROBO_CHECK(message.find("colour") != std::string::npos);
    ROBO_CHECK(message.find("'color'") != std::string::npos);
}

ROBO_TEST(CssCascade, UnknownPropertyWithoutANearNameOnlySaysNo)
{
    Fixture fixture;
    fixture.Build("body { zzzzzzzz: red }");

    ROBO_CHECK(fixture.diagnostics.HasErrors());

    // Nothing in the registry is close to this name, so the compiler must not
    // invent a guess: a wrong suggestion is worse than none.
    for (const CSSDiagnostic& entry : fixture.diagnostics.Entries())
    {
        if (entry.code == CSSDiagnosticCode::UnknownProperty)
        {
            ROBO_CHECK(entry.message.find("Did you mean") == std::string::npos);
        }
    }
}

ROBO_TEST(CssCascade, ComputedValuesRememberWhereTheyWereWritten)
{
    Fixture fixture;
    fixture.Build("html { color: red }\nbody { padding: 1px }");

    const CSSComputedStyle* body = fixture.Body();
    ROBO_CHECK(body != nullptr);

    if (body == nullptr)
    {
        return;
    }

    // The colour is inherited from the `html` rule on line 1, and the location
    // follows the value to the ancestor that declared it rather than staying on
    // the element that used it.
    const CSSSourceLocation color = body->SourceLocationOf(CSSPropertyId::Color);
    ROBO_CHECK(color.IsValid());
    ROBO_CHECK_EQ(color.file, std::string("test.css"));
    ROBO_CHECK(color.line == 1);

    // The padding is expanded from the shorthand on line 2: every longhand it
    // produced points at the one declaration the reader wrote.
    const CSSSourceLocation padding = body->SourceLocationOf(CSSPropertyId::PaddingTop);
    ROBO_CHECK(padding.IsValid());
    ROBO_CHECK(padding.line == 2);

    // A property nothing declared has no declaration to point at.
    ROBO_CHECK(!body->SourceLocationOf(CSSPropertyId::BorderTopWidth).IsValid());
}

ROBO_TEST(CssCascade, AllInitialResetsEveryProperty)
{
    Fixture fixture;
    fixture.Build("html { color: #123456; padding: 7px } body { color: red; padding: 3px; width: 40px } "
                  "body { all: initial }");

    // Every property the registry carries takes its initial value: the colour is
    // the initial black rather than the parent's, even though `color` inherits,
    // and the padding and width are their initial values rather than the ones the
    // earlier rule set.
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#000000"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::PaddingTop), std::string("0px"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Width), std::string("auto"));
    ROBO_CHECK(!fixture.diagnostics.HasErrors());
}

ROBO_TEST(CssCascade, AllInheritTakesEvenTheNonInheritedProperties)
{
    Fixture fixture;
    fixture.Build("html { color: #123456; padding: 7px; width: 90px } body { all: inherit }");

    // `padding` and `width` do not inherit, and `all: inherit` copies them
    // anyway: that is the difference between `all` and an inherited property.
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#123456"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::PaddingTop), std::string("7px"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Width), std::string("90px"));
}

ROBO_TEST(CssCascade, AllUnsetSplitsInheritedFromNonInherited)
{
    Fixture fixture;
    fixture.Build("html { color: #abcdef; padding: 7px } body { all: unset }");

    // An inherited property takes the parent's value, a non-inherited one takes
    // its initial value: this is `unset` per property, applied to all of them.
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::Color), std::string("#abcdef"));
    ROBO_CHECK_EQ(ComputedText(fixture.Body(), CSSPropertyId::PaddingTop), std::string("0px"));
}

ROBO_TEST(CssCascade, AllWithANonWideValueIsReported)
{
    Fixture fixture;
    fixture.Build("body { all: 3px }");

    // `all` takes only the CSS-wide keywords, so anything else is an invalid
    // value with a diagnostic - never a declaration that quietly did nothing.
    ROBO_CHECK(fixture.diagnostics.HasErrors());
    ROBO_CHECK(HasCode(fixture.diagnostics, CSSDiagnosticCode::InvalidValue));
}

ROBO_TEST(CssCascade, AllIsImplementedRatherThanReported)
{
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();

    ROBO_CHECK(registry.Find("all") != nullptr);
    ROBO_CHECK(!registry.IsKnownUnsupportedName("all"));
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

    // Carried properties outnumber the compiled ones by an order of magnitude:
    // the report has to say so, or the compiled column reads as the whole of the
    // compiler's knowledge.
    ROBO_CHECK(report.dynamicPropertyCount > 500);
    ROBO_CHECK_EQ(report.dynamic.size(), report.dynamicPropertyCount);

    // Everything left unsupported comes from outside the reference.
    ROBO_CHECK(report.unsupportedPropertyCount > 0);
    ROBO_CHECK(report.unsupportedPropertyCount < 20);
    ROBO_CHECK(std::find(report.unsupported.begin(), report.unsupported.end(), std::string("touch-action")) !=
               report.unsupported.end());

    const std::string markdown = report.ToMarkdown();
    ROBO_CHECK(markdown.find("CSS coverage") != std::string::npos);
    ROBO_CHECK(markdown.find("`background-color`") != std::string::npos);
    ROBO_CHECK(markdown.find("`grid-template-columns`") != std::string::npos);
    ROBO_CHECK(markdown.find("Carried properties") != std::string::npos);
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

// ---------------------------------------------------------------------------
// The numeric functions
//
// Every value here is the one a browser computes for the same declaration; the
// expected number is written beside the expression so the two can be read
// together. A function is only implemented if it produces the browser's number,
// which is why these assert the value and not merely that the declaration parsed.
// ---------------------------------------------------------------------------

namespace
{
    /// Parses one `width` declaration and reports whether it produced a value.
    bool ParseWidthDeclaration(const char* declaration, CSSValue& value, CSSDiagnosticSink& diagnostics)
    {
        const std::vector<CSSDeclaration> declarations =
            CSSParser(diagnostics).ParseDeclarationList("t.css", declaration);

        if (declarations.empty())
        {
            return false;
        }

        const CSSPropertyDefinition* definition = CSSPropertyRegistry::Instance().Find(CSSPropertyId::Width);

        if (definition == nullptr)
        {
            return false;
        }

        return ParsePropertyValue(*definition, declarations[0].value, value, declarations[0].location, diagnostics);
    }
}

ROBO_TEST(CssValue, TheNumericFunctionsComputeTheBrowsersValue)
{
    struct Row
    {
        const char* declaration;
        double expected;
        CSSUnit unit;
    };

    // Each expression is written so that the function's own result is the value
    // the property takes: the number-valued functions are multiplied by 1px and
    // the angle-valued ones divided by 1deg, which is exactly the type algebra
    // the reference defines.
    static const Row rows[] = {
        // calc(): arithmetic, and the absolute units it has to bring to one unit.
        { "width: calc(100px - 20px)", 80.0, CSSUnit::Px },
        { "width: calc(1in + 1px)", 97.0, CSSUnit::Px },
        { "width: calc(1cm + 1mm)", 41.57480314960630, CSSUnit::Px },
        { "width: calc(6px / 2px * 1px)", 3.0, CSSUnit::Px },
        { "width: calc(2 * 3px)", 6.0, CSSUnit::Px },

        // min(), max(), clamp().
        { "width: min(2px, 3px)", 2.0, CSSUnit::Px },
        { "width: max(2px, 5px)", 5.0, CSSUnit::Px },
        { "width: clamp(1px, 10px, 5px)", 5.0, CSSUnit::Px },
        { "width: clamp(1px, 0px, 5px)", 1.0, CSSUnit::Px },
        { "width: max(1px, 2px, 3px)", 3.0, CSSUnit::Px },

        // abs(), sign().
        { "width: abs(-10px)", 10.0, CSSUnit::Px },
        { "width: abs(10px)", 10.0, CSSUnit::Px },
        { "width: calc(sign(-3) * 1px)", -1.0, CSSUnit::Px },
        { "width: calc(sign(0) * 1px)", 0.0, CSSUnit::Px },
        { "width: calc(sign(3) * 1px)", 1.0, CSSUnit::Px },

        // sqrt(), hypot(), pow(), exp(), log().
        { "width: sqrt(16px)", 4.0, CSSUnit::Px },
        { "width: hypot(3px, 4px)", 5.0, CSSUnit::Px },
        { "width: hypot(2px, 3px, 6px)", 7.0, CSSUnit::Px },
        { "width: calc(pow(2, 10) * 1px)", 1024.0, CSSUnit::Px },
        { "width: calc(exp(0) * 1px)", 1.0, CSSUnit::Px },
        { "width: calc(log(8, 2) * 1px)", 3.0, CSSUnit::Px },
        { "width: calc(log(1) * 1px)", 0.0, CSSUnit::Px },

        // mod() and rem(): the one difference between them is the sign they keep.
        { "width: mod(10px, 3px)", 1.0, CSSUnit::Px },
        { "width: rem(10px, 3px)", 1.0, CSSUnit::Px },
        { "width: mod(-10px, 3px)", 2.0, CSSUnit::Px },
        { "width: rem(-10px, 3px)", -1.0, CSSUnit::Px },
        { "width: mod(10px, -3px)", -2.0, CSSUnit::Px },
        { "width: rem(10px, -3px)", 1.0, CSSUnit::Px },

        // round(), with each of the four strategies the reference defines.
        { "width: round(10.4px)", 10.0, CSSUnit::Px },
        { "width: round(10.5px)", 11.0, CSSUnit::Px },
        { "width: round(nearest, 10.5px)", 11.0, CSSUnit::Px },
        { "width: round(up, 10.1px)", 11.0, CSSUnit::Px },
        { "width: round(down, 10.9px)", 10.0, CSSUnit::Px },
        { "width: round(to-zero, -10.9px)", -10.0, CSSUnit::Px },
        { "width: round(nearest, 10px, 3px)", 9.0, CSSUnit::Px },
        { "width: round(up, 10px, 3px)", 12.0, CSSUnit::Px },

        // The trigonometric set. sin/cos/tan take an angle and give a number;
        // asin/acos/atan/atan2 take numbers and give an angle.
        { "width: calc(sin(30deg) * 10px)", 5.0, CSSUnit::Px },
        { "width: calc(cos(60deg) * 10px)", 5.0, CSSUnit::Px },
        { "width: calc(tan(45deg) * 10px)", 10.0, CSSUnit::Px },
        { "width: calc(sin(0.25turn) * 10px)", 10.0, CSSUnit::Px },
        { "width: calc(sin(3.141592653589793rad) * 1px)", 0.0, CSSUnit::Px },
        { "width: calc(sin(100grad) * 10px)", 10.0, CSSUnit::Px },
        { "width: calc(asin(0.5) / 1deg * 1px)", 30.0, CSSUnit::Px },
        { "width: calc(acos(0.5) / 1deg * 1px)", 60.0, CSSUnit::Px },
        { "width: calc(atan(1) / 1deg * 1px)", 45.0, CSSUnit::Px },
        { "width: calc(atan2(1, 1) / 1deg * 1px)", 45.0, CSSUnit::Px },
        { "width: calc(atan2(1, -1) / 1deg * 1px)", 135.0, CSSUnit::Px },

        // Nesting: a function is an operand of another, which is what makes the
        // whole family compose rather than only work at the top level.
        { "width: calc(abs(-10px) + sqrt(16px))", 14.0, CSSUnit::Px },
        { "width: min(hypot(3px, 4px), 6px)", 5.0, CSSUnit::Px },
        { "width: calc(round(up, 1.5px) * 2 + hypot(2px, 2px))", 6.8284271247461903, CSSUnit::Px },
        { "width: hypot(2px, round(up, 1.5px))", 2.8284271247461903, CSSUnit::Px }
    };

    for (const Row& row : rows)
    {
        CSSDiagnosticSink diagnostics;
        CSSValue value;

        const bool parsed = ParseWidthDeclaration(row.declaration, value, diagnostics);

        if (!parsed)
        {
            RoboTest::Fail(std::string("did not parse: ") + row.declaration + "\n" + diagnostics.Format(),
                           __FILE__, __LINE__);
            continue;
        }

        if (value.kind != CSSValueKind::Dimension || value.unit != row.unit)
        {
            RoboTest::Fail(std::string("not a length in the expected unit: ") + row.declaration, __FILE__,
                           __LINE__);
            continue;
        }

        if (std::fabs(value.number - row.expected) > 1e-6)
        {
            RoboTest::Fail(std::string(row.declaration) + " gave " + std::to_string(value.number) +
                               " where a browser gives " + std::to_string(row.expected),
                           __FILE__, __LINE__);
        }
    }
}

ROBO_TEST(CssValue, AMathFunctionThatIsNotAValueIsReported)
{
    // Each of these is invalid CSS, and the compiler has to say so rather than
    // pick a number: a length times a length is an area, `sqrt` has no real
    // answer below zero, a trigonometric function needs an angle, `log` has no
    // answer at zero, and `mod` has none with a zero divisor.
    static const char* const invalid[] = {
        "width: sqrt(-1px)",
        "width: sin(30)",
        "width: calc(log(0) * 1px)",
        "width: mod(1px, 0px)",
        "width: pow(2px, 2)",
        "width: hypot()",
        "width: cos(1px)",
        "width: asin(2)",
        "width: abs(1px, 2px)",
        "width: round(1px, 2px, 3px)"
    };

    for (const char* declaration : invalid)
    {
        CSSDiagnosticSink diagnostics;
        CSSValue value;

        const bool parsed = ParseWidthDeclaration(declaration, value, diagnostics);

        if (parsed)
        {
            RoboTest::Fail(std::string("accepted, but a browser refuses it: ") + declaration, __FILE__,
                           __LINE__);
        }
    }
}

ROBO_TEST(CssValue, EveryCompiledPropertyStatesTheInitialValueTheReferenceDoes)
{
    // The initial value is what a property means when nothing sets it, so a wrong
    // one is invisible until a document relies on it - `flex-shrink` that starts
    // at 0, or a `background-color` that starts opaque. Every compiled property is
    // listed with the initial value its reference page states.
    struct Row
    {
        CSSPropertyId id;
        const char* initial;
    };

    static const Row rows[] = {
        { CSSPropertyId::All, "initial" },
        { CSSPropertyId::Display, "inline" },
        { CSSPropertyId::Width, "auto" },
        { CSSPropertyId::Height, "auto" },
        { CSSPropertyId::MinWidth, "0" },
        { CSSPropertyId::MinHeight, "0" },
        { CSSPropertyId::MaxWidth, "none" },
        { CSSPropertyId::MaxHeight, "none" },
        { CSSPropertyId::MarginTop, "0" },
        { CSSPropertyId::MarginRight, "0" },
        { CSSPropertyId::MarginBottom, "0" },
        { CSSPropertyId::MarginLeft, "0" },
        { CSSPropertyId::PaddingTop, "0" },
        { CSSPropertyId::PaddingRight, "0" },
        { CSSPropertyId::PaddingBottom, "0" },
        { CSSPropertyId::PaddingLeft, "0" },
        { CSSPropertyId::Position, "static" },
        { CSSPropertyId::Top, "auto" },
        { CSSPropertyId::Right, "auto" },
        { CSSPropertyId::Bottom, "auto" },
        { CSSPropertyId::Left, "auto" },
        { CSSPropertyId::ZIndex, "auto" },
        { CSSPropertyId::Float, "none" },
        { CSSPropertyId::Clear, "none" },
        { CSSPropertyId::FlexDirection, "row" },
        { CSSPropertyId::JustifyContent, "normal" },
        { CSSPropertyId::AlignItems, "normal" },
        { CSSPropertyId::AlignSelf, "auto" },
        { CSSPropertyId::FlexGrow, "0" },
        { CSSPropertyId::FlexShrink, "1" },
        { CSSPropertyId::FlexBasis, "auto" },
        { CSSPropertyId::FlexOrder, "0" },
        { CSSPropertyId::BorderTopWidth, "medium" },
        { CSSPropertyId::BorderRightWidth, "medium" },
        { CSSPropertyId::BorderBottomWidth, "medium" },
        { CSSPropertyId::BorderLeftWidth, "medium" },
        { CSSPropertyId::BorderTopStyle, "none" },
        { CSSPropertyId::BorderRightStyle, "none" },
        { CSSPropertyId::BorderBottomStyle, "none" },
        { CSSPropertyId::BorderLeftStyle, "none" },
        { CSSPropertyId::BorderTopColor, "currentcolor" },
        { CSSPropertyId::BorderRightColor, "currentcolor" },
        { CSSPropertyId::BorderBottomColor, "currentcolor" },
        { CSSPropertyId::BorderLeftColor, "currentcolor" },
        { CSSPropertyId::BorderRadius, "0" },
        { CSSPropertyId::Color, "black" },
        { CSSPropertyId::BackgroundColor, "transparent" },
        { CSSPropertyId::FontSize, "medium" },
        { CSSPropertyId::FontWeight, "normal" },
        { CSSPropertyId::FontFamily, "serif" },
        { CSSPropertyId::FontStyle, "normal" },
        { CSSPropertyId::FontKerning, "auto" },
        { CSSPropertyId::FontFeatureSettings, "normal" },
        { CSSPropertyId::FontVariantLigatures, "normal" },
        { CSSPropertyId::FontVariantCaps, "normal" },
        { CSSPropertyId::FontVariantNumeric, "normal" },
        { CSSPropertyId::FontVariantPosition, "normal" },
        { CSSPropertyId::FontVariantEastAsian, "normal" },
        { CSSPropertyId::LineHeight, "normal" },
        { CSSPropertyId::LetterSpacing, "normal" },
        { CSSPropertyId::WordSpacing, "normal" },
        { CSSPropertyId::TextAlign, "start" },
        { CSSPropertyId::TextAlignLast, "auto" },
        { CSSPropertyId::TextJustify, "auto" },
        { CSSPropertyId::TextIndent, "0" },
        { CSSPropertyId::TextTransform, "none" },
        { CSSPropertyId::TextDecorationLine, "none" },
        { CSSPropertyId::TextDecorationColor, "currentcolor" },
        { CSSPropertyId::TextDecorationStyle, "solid" },
        { CSSPropertyId::TextDecorationThickness, "auto" },
        { CSSPropertyId::TextUnderlineOffset, "auto" },
        { CSSPropertyId::WhiteSpace, "normal" },
        { CSSPropertyId::Direction, "ltr" },
        { CSSPropertyId::WritingMode, "horizontal-tb" },
        { CSSPropertyId::TextOrientation, "mixed" },
        { CSSPropertyId::Hyphens, "manual" },
        { CSSPropertyId::OverflowWrap, "normal" },
        { CSSPropertyId::WordBreak, "normal" },
        { CSSPropertyId::LineBreak, "auto" },
        { CSSPropertyId::TabSize, "8" },
        { CSSPropertyId::OverflowX, "visible" },
        { CSSPropertyId::OverflowY, "visible" }
    };

    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();

    for (const Row& row : rows)
    {
        const CSSPropertyDefinition* definition = registry.Find(row.id);
        ROBO_CHECK(definition != nullptr);

        if (definition == nullptr)
        {
            continue;
        }

        if (std::string(definition->initialValue) != row.initial)
        {
            RoboTest::Fail(std::string(definition->name) + " starts at '" + definition->initialValue +
                               "' where the reference states '" + row.initial + "'",
                           __FILE__, __LINE__);
        }
    }
}
