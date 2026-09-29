#include "../TestFramework.h"
#include "../TestHelpers.h"

#include <string>
#include <vector>

using namespace RoboHTMLCompiler;
using namespace RoboTest;

namespace
{
    std::vector<std::string> AllSvgElements()
    {
        std::vector<std::string> names;
        for (std::size_t index = 0; index < Generated::ElementCount; ++index)
        {
            if (Generated::ElementTable[index].namespaceId == Generated::NamespaceId::Svg)
            {
                names.emplace_back(Generated::ElementTable[index].name);
            }
        }
        return names;
    }

    /// Elements that must not appear in a standalone coverage document.
    bool IsRejectedByDesign(const std::string& name)
    {
        return name == "style" || name == "script";
    }
}

ROBO_TEST(Svg, EveryRegisteredSvgElementParsesInTheSvgNamespace)
{
    for (const std::string& name : AllSvgElements())
    {
        const std::string snippet =
            "<svg viewBox=\"0 0 4 4\"><" + name + "></" + name + "></svg>";

        DiagnosticSink sink;
        std::unique_ptr<MarkupDocument> document = ParseHtml(WrapBody(snippet), sink);
        ROBO_CHECK(document != nullptr);
        if (document == nullptr)
        {
            continue;
        }

        bool foundInSvgNamespace = false;
        document->ForEachElement([&](const ElementNode& element) {
            if (element.sourceTag == name && element.ns == MarkupNamespace::Svg)
            {
                foundInSvgNamespace = true;
            }
        });

        if (name == "svg")
        {
            // The nested <svg> is parsed by the SVG frontend as an SVG element.
            ROBO_CHECK(foundInSvgNamespace);
        }
        else
        {
            ROBO_CHECK(foundInSvgNamespace);
        }
    }
}

ROBO_TEST(Svg, EveryRegisteredSvgElementValidates)
{
    for (const std::string& name : AllSvgElements())
    {
        if (IsRejectedByDesign(name))
        {
            continue;
        }

        const std::string snippet =
            "<svg viewBox=\"0 0 4 4\"><" + name + " id=\"" + name + "\"></" + name + "></svg>";

        DiagnosticSink sink;
        const bool valid = ValidateBody(snippet, sink);
        if (!valid)
        {
            std::cout << "      svg element <" << name << "> failed validation\n"
                      << FormatDiagnostics(sink) << "\n";
        }
        ROBO_CHECK(valid);
    }
}

ROBO_TEST(Svg, UnknownSvgElementIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<svg viewBox=\"0 0 4 4\"><blob x=\"1\"></blob></svg>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::UnknownSvgElement));
}

ROBO_TEST(Svg, InvalidSvgAttributeIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<svg viewBox=\"0 0 4 4\"><circle cx=\"1\" cy=\"1\" r=\"1\" bogus=\"2\"></circle></svg>",
                             sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::InvalidSvgAttribute));
}

ROBO_TEST(Svg, AttributeScopedToTheWrongSvgElementIsRejected)
{
    DiagnosticSink sink;
    // 'd' is only valid on <path>.
    ROBO_CHECK(!ValidateBody("<svg viewBox=\"0 0 4 4\"><circle cx=\"1\" cy=\"1\" r=\"1\" d=\"M0 0\"></circle></svg>",
                             sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::InvalidSvgAttribute));

    DiagnosticSink second;
    // 'cx' is only valid on circles, ellipses and radial gradients.
    ROBO_CHECK(!ValidateBody("<svg viewBox=\"0 0 4 4\"><rect x=\"1\" y=\"1\" width=\"1\" height=\"1\" cx=\"2\"></rect></svg>",
                             second));
    ROBO_CHECK(HasError(second, DiagnosticCode::InvalidSvgAttribute));
}

ROBO_TEST(Svg, GeometryAndPaintAreAccepted)
{
    DiagnosticSink sink;
    ROBO_CHECK(ValidateBody(
        "<svg viewBox=\"0 0 100 100\" width=\"100\" height=\"100\" preserveAspectRatio=\"xMidYMid meet\">"
        "<defs><linearGradient id=\"g\" gradientUnits=\"userSpaceOnUse\" spreadMethod=\"pad\">"
        "<stop offset=\"0\" stop-color=\"#fff\" stop-opacity=\"1\"></stop>"
        "<stop offset=\"1\" stop-color=\"#000\" stop-opacity=\"1\"></stop>"
        "</linearGradient></defs>"
        "<g transform=\"translate(1 2)\" opacity=\"0.5\">"
        "<rect x=\"1\" y=\"1\" width=\"10\" height=\"10\" rx=\"2\" ry=\"2\" fill=\"url(#g)\" stroke=\"#000\" "
        "stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"bevel\" stroke-dasharray=\"1 2\"></rect>"
        "<circle cx=\"50\" cy=\"50\" r=\"10\" fill=\"#f00\" fill-opacity=\"0.5\" fill-rule=\"evenodd\"></circle>"
        "<ellipse cx=\"10\" cy=\"10\" rx=\"5\" ry=\"3\"></ellipse>"
        "<line x1=\"0\" y1=\"0\" x2=\"10\" y2=\"10\"></line>"
        "<polyline points=\"0,0 1,1\"></polyline>"
        "<polygon points=\"0,0 1,1 2,0\"></polygon>"
        "<path d=\"M0 0 L1 1 Z\" pathLength=\"10\"></path>"
        "<text x=\"1\" y=\"1\" text-anchor=\"middle\" font-family=\"serif\" font-size=\"12\" "
        "font-weight=\"bold\" letter-spacing=\"1\" word-spacing=\"1\">label</text>"
        "<image href=\"a.png\" x=\"1\" y=\"1\" width=\"4\" height=\"4\"></image>"
        "<clipPath id=\"c\"><rect x=\"0\" y=\"0\" width=\"1\" height=\"1\"></rect></clipPath>"
        "<mask id=\"m\"></mask>"
        "<marker id=\"k\" markerWidth=\"4\" markerHeight=\"4\" markerUnits=\"strokeWidth\" orient=\"auto\">"
        "</marker>"
        "<pattern id=\"p\" patternUnits=\"userSpaceOnUse\"></pattern>"
        "<filter id=\"f\" filterUnits=\"objectBoundingBox\">"
        "<feGaussianBlur stdDeviation=\"2\"></feGaussianBlur>"
        "<feBlend mode=\"multiply\" in=\"SourceGraphic\"></feBlend>"
        "<feOffset dx=\"1\" dy=\"1\"></feOffset>"
        "</filter>"
        "<animate attributeName=\"opacity\" from=\"0\" to=\"1\" dur=\"1s\" repeatCount=\"indefinite\">"
        "</animate>"
        "<use href=\"#k\"></use>"
        "<symbol id=\"s\" viewBox=\"0 0 1 1\"></symbol>"
        "<switch><g></g></switch>"
        "<view id=\"v\" viewBox=\"0 0 1 1\"></view>"
        "</g></svg>",
        sink));
}

ROBO_TEST(Svg, ForeignObjectHostsHtmlContent)
{
    DiagnosticSink sink;
    ROBO_CHECK(ValidateBody(
        "<svg viewBox=\"0 0 10 10\"><foreignObject x=\"0\" y=\"0\" width=\"10\" height=\"10\" "
        "xmlns=\"http://www.w3.org/1999/xhtml\"><p class=\"text-sm\">HTML inside SVG</p></foreignObject></svg>",
        sink));
}

ROBO_TEST(Svg, InlineScriptAndStyleInsideSvgAreRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<svg viewBox=\"0 0 4 4\"><style>circle { fill: red; }</style></svg>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::InlineStyleElement));

    DiagnosticSink second;
    ROBO_CHECK(!ValidateBody("<svg viewBox=\"0 0 4 4\"><script>const x = 1;</script></svg>", second));
    ROBO_CHECK(HasError(second, DiagnosticCode::InlineScriptContent));
}

ROBO_TEST(Svg, SvgRootOnlyStartsInHtmlContent)
{
    // A nested <svg> inside SVG stays in the SVG namespace.
    DiagnosticSink sink;
    const std::string document = WrapBody("<svg viewBox=\"0 0 4 4\"><svg viewBox=\"0 0 1 1\"></svg></svg>");
    std::unique_ptr<MarkupDocument> parsed = ParseHtml(document, sink);
    ROBO_CHECK(parsed != nullptr);

    if (parsed == nullptr)
    {
        return;
    }

    std::size_t svgCount = 0;
    parsed->ForEachElement([&](const ElementNode& element) {
        if (element.ns == MarkupNamespace::Svg)
        {
            ++svgCount;
        }
    });
    ROBO_CHECK_EQ(svgCount, static_cast<std::size_t>(2));
}
