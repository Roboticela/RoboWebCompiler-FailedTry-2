#include "../TestFramework.h"
#include "../TestHelpers.h"

#include <string>
#include <vector>

using namespace RoboHTMLCompiler;
using namespace RoboTest;

namespace
{
    std::vector<std::string> AllHtmlElements()
    {
        std::vector<std::string> names;
        for (std::size_t index = 0; index < Generated::ElementCount; ++index)
        {
            if (Generated::ElementTable[index].namespaceId == Generated::NamespaceId::Html)
            {
                names.emplace_back(Generated::ElementTable[index].name);
            }
        }
        return names;
    }

    /// Builds a snippet for an element, adding the attributes the reference
    /// requires and honouring void element syntax.
    std::string SnippetFor(const ElementDefinition& definition)
    {
        if (definition.name == "img")
        {
            return "<img src=\"a.png\" alt=\"a\" width=\"8\" height=\"8\">";
        }

        if (definition.name == "script")
        {
            return "<script src=\"app.js\" defer></script>";
        }

        if (definition.isVoid)
        {
            return "<" + definition.name + ">";
        }

        return "<" + definition.name + ">content</" + definition.name + ">";
    }

    /// Attributes the reference requires for an element to carry a useful
    /// meaning. They are added to coverage snippets so that the attribute under
    /// test is the only rule being exercised. Attributes that are under test
    /// themselves are never added twice.
    std::string RequiredAttributesFor(const ElementDefinition& definition, const std::string& testedAttribute)
    {
        if (definition.name != "img")
        {
            return std::string();
        }

        std::string attributes;
        if (testedAttribute != "src")
        {
            attributes += " src=\"a.png\"";
        }
        if (testedAttribute != "alt")
        {
            attributes += " alt=\"a\"";
        }
        return attributes;
    }
}

ROBO_TEST(HtmlCoverage, EveryElementParsesIntoTheAst)
{
    for (const std::string& name : AllHtmlElements())
    {
        const std::string snippet = "<" + name + ">x</" + name + ">";

        DiagnosticSink sink;
        std::unique_ptr<MarkupDocument> document = ParseHtml(WrapBody(snippet), sink);
        ROBO_CHECK(document != nullptr);

        if (document == nullptr)
        {
            continue;
        }

        bool found = false;
        document->ForEachElement([&](const ElementNode& element) {
            if (element.sourceTag == name)
            {
                found = true;
            }
        });
        ROBO_CHECK(found);
    }
}

ROBO_TEST(HtmlCoverage, EveryElementValidatesOrFailsForADocumentedReason)
{
    const Registry& registry = Registry::Instance();

    for (const std::string& name : AllHtmlElements())
    {
        const ElementDefinition* definition = registry.FindHtmlElement(name);
        ROBO_CHECK(definition != nullptr);
        if (definition == nullptr)
        {
            continue;
        }

        const std::string snippet = SnippetFor(*definition);

        ValidationOptions options;
        // Obsolete vocabulary is rejected by design; coverage testing uses the
        // documented lax switch.
        options.rejectObsoleteElements = false;
        options.rejectObsoleteAttributes = false;
        options.strictness = Strictness::Lax;

        DiagnosticSink sink;
        const bool valid = ValidateBody(snippet, sink, options);

        if (name == "style")
        {
            ROBO_CHECK(!valid);
            ROBO_CHECK(HasError(sink, DiagnosticCode::InlineStyleElement));
            continue;
        }

        if (!valid)
        {
            std::cout << "      element <" << name << "> failed validation\n"
                      << FormatDiagnostics(sink) << "\n";
        }

        ROBO_CHECK(valid);
    }
}

ROBO_TEST(HtmlCoverage, EveryDeclaredElementAttributeValidatesOnItsElement)
{
    const Registry& registry = Registry::Instance();

    // A representative attribute value per attribute name. Attributes without an
    // entry are exercised as valueless.
    const std::vector<std::pair<std::string, std::string>> sampleValues = {
        { "accept", "image/*" },          { "action", "/submit" },
        { "alt", "text" },                { "as", "script" },
        { "charset", "utf-8" },           { "cite", "/source" },
        { "class", "px-4" },              { "cols", "20" },
        { "colspan", "2" },               { "content", "text/html" },
        { "coords", "0,0,1,1" },          { "crossorigin", "anonymous" },
        { "data", "a.svg" },              { "datetime", "2026-09-29" },
        { "decoding", "async" },          { "dir", "ltr" },
        { "dirname", "task.dir" },        { "download", "file.txt" },
        { "enctype", "text/plain" },      { "for", "task" },
        { "form", "f" },                  { "formaction", "/x" },
        { "formenctype", "text/plain" },  { "formmethod", "post" },
        { "formtarget", "_self" },        { "headers", "h1" },
        { "height", "8" },                { "high", "8" },
        { "href", "/next" },              { "hreflang", "en" },
        { "http-equiv", "refresh" },      { "imagesizes", "100vw" },
        { "imagesrcset", "a.avif 1x" },   { "integrity", "sha256-abc" },
        { "kind", "captions" },           { "label", "Group" },
        { "list", "options" },            { "loading", "lazy" },
        { "low", "1" },                   { "max", "10" },
        { "maxlength", "10" },            { "media", "screen" },
        { "method", "post" },             { "min", "0" },
        { "minlength", "1" },             { "name", "task" },
        { "optimum", "5" },               { "pattern", "[a-z]+" },
        { "ping", "/p" },                 { "placeholder", "text" },
        { "popovertarget", "pop" },       { "poster", "p.png" },
        { "preload", "metadata" },        { "referrerpolicy", "no-referrer" },
        { "rel", "noopener" },            { "rows", "4" },
        { "rowspan", "2" },               { "scope", "col" },
        { "shadowrootmode", "open" },     { "shape", "rect" },
        { "size", "4" },                  { "sizes", "100vw" },
        { "span", "2" },                  { "src", "a.png" },
        { "srcdoc", "<p>x</p>" },         { "srclang", "en" },
        { "srcset", "a.png 1x" },         { "start", "1" },
        { "step", "2" },                  { "target", "_blank" },
        { "title", "caption" },           { "type", "text" },
        { "usemap", "#m" },               { "value", "1" },
        { "wrap", "soft" },               { "width", "8" },
        { "abbr", "Hdr" },                { "color", "#fff" },
        { "allow", "fullscreen" },        { "sandbox", "allow-scripts" },
        { "kind", "subtitles" }
    };

    auto sampleFor = [&sampleValues](const std::string& name) -> std::string {
        for (const auto& entry : sampleValues)
        {
            if (entry.first == name)
            {
                return entry.second;
            }
        }
        return std::string();
    };

    for (std::size_t index = 0; index < Generated::ElementCount; ++index)
    {
        const Generated::ElementRecord& record = Generated::ElementTable[index];
        if (record.namespaceId != Generated::NamespaceId::Html)
        {
            continue;
        }

        const ElementDefinition* definition = registry.FindHtmlElement(record.name);
        if (definition == nullptr || definition->isObsolete || definition->name == "style" ||
            definition->name == "script")
        {
            continue;
        }

        ValidationOptions options;
        options.rejectObsoleteElements = false;
        options.rejectObsoleteAttributes = false;
        options.strictness = Strictness::Lax;

        for (const std::string& attribute : definition->declaredAttributes)
        {
            std::string opening = "<" + definition->name + RequiredAttributesFor(*definition, attribute);
            const std::string value = sampleFor(attribute);
            opening += " " + attribute + "=\"" + value + "\"";
            opening += ">";

            const std::string fullSnippet = definition->isVoid
                                                ? opening
                                                : opening + "</" + definition->name + ">";

            DiagnosticSink sink;
            const bool valid = ValidateBody(fullSnippet, sink, options);
            if (!valid)
            {
                std::cout << "      attribute " << attribute << " on <" << definition->name
                          << "> failed validation\n"
                          << FormatDiagnostics(sink) << "\n";
            }
            ROBO_CHECK(valid);
        }
    }
}

ROBO_TEST(HtmlCoverage, ElementCategoriesArePreserved)
{
    const Registry& registry = Registry::Instance();

    static const std::pair<const char*, ElementKind> expected[] = {
        { "html", ElementKind::Document },       { "head", ElementKind::Metadata },
        { "body", ElementKind::Container },      { "div", ElementKind::Container },
        { "section", ElementKind::Container },   { "nav", ElementKind::Container },
        { "h1", ElementKind::Heading },          { "h6", ElementKind::Heading },
        { "p", ElementKind::Text },              { "span", ElementKind::Text },
        { "pre", ElementKind::PreformattedText },{ "blockquote", ElementKind::Quote },
        { "br", ElementKind::LineBreak },        { "hr", ElementKind::Separator },
        { "a", ElementKind::Link },              { "ul", ElementKind::List },
        { "ol", ElementKind::List },             { "li", ElementKind::ListItem },
        { "dl", ElementKind::DescriptionList },  { "dt", ElementKind::DescriptionTerm },
        { "dd", ElementKind::DescriptionDescription },
        { "img", ElementKind::Image },           { "picture", ElementKind::ResponsiveImage },
        { "source", ElementKind::MediaSource },  { "map", ElementKind::ImageMap },
        { "area", ElementKind::ImageMapLink },   { "audio", ElementKind::Audio },
        { "video", ElementKind::Video },         { "track", ElementKind::MediaTrack },
        { "canvas", ElementKind::Canvas },       { "form", ElementKind::Form },
        { "fieldset", ElementKind::FieldSet },   { "legend", ElementKind::Legend },
        { "label", ElementKind::Label },         { "button", ElementKind::Button },
        { "input", ElementKind::Input },         { "textarea", ElementKind::TextArea },
        { "select", ElementKind::Select },       { "option", ElementKind::Option },
        { "optgroup", ElementKind::OptionGroup },{ "datalist", ElementKind::DataList },
        { "output", ElementKind::Output },       { "progress", ElementKind::Progress },
        { "meter", ElementKind::Meter },         { "table", ElementKind::Table },
        { "caption", ElementKind::TableCaption },{ "colgroup", ElementKind::TableColumnGroup },
        { "col", ElementKind::TableColumn },     { "thead", ElementKind::TableSection },
        { "tbody", ElementKind::TableSection },  { "tfoot", ElementKind::TableSection },
        { "tr", ElementKind::TableRow },         { "td", ElementKind::TableCell },
        { "th", ElementKind::TableHeaderCell },  { "details", ElementKind::Disclosure },
        { "summary", ElementKind::DisclosureSummary }, { "dialog", ElementKind::Dialog },
        { "iframe", ElementKind::Frame },        { "embed", ElementKind::EmbeddedContent },
        { "object", ElementKind::EmbeddedObject },{ "template", ElementKind::Template },
        { "slot", ElementKind::Slot },           { "figure", ElementKind::Figure },
        { "figcaption", ElementKind::FigureCaption }
    };

    for (const auto& entry : expected)
    {
        const ElementDefinition* definition = registry.FindHtmlElement(entry.first);
        ROBO_CHECK(definition != nullptr);
        if (definition != nullptr)
        {
            ROBO_CHECK_EQ(static_cast<int>(definition->kind), static_cast<int>(entry.second));
        }
    }

    const ElementDefinition* h3 = registry.FindHtmlElement("h3");
    ROBO_CHECK(h3 != nullptr);
    if (h3 != nullptr)
    {
        ROBO_CHECK_EQ(h3->headingLevel, 3);
    }

    const ElementDefinition* ol = registry.FindHtmlElement("ol");
    ROBO_CHECK(ol != nullptr);
    if (ol != nullptr)
    {
        ROBO_CHECK(ol->isOrderedList);
    }

    const ElementDefinition* ul = registry.FindHtmlElement("ul");
    ROBO_CHECK(ul != nullptr);
    if (ul != nullptr)
    {
        ROBO_CHECK(!ul->isOrderedList);
    }
}

ROBO_TEST(HtmlCoverage, EveryElementMapsToACompactCategory)
{
    // Many source elements intentionally share one native category: the
    // registry must not degenerate into one class per HTML tag.
    const Registry& registry = Registry::Instance();

    std::size_t htmlElementCount = 0;
    const ElementDefinition* div = registry.FindHtmlElement("div");

    for (std::size_t index = 0; index < Generated::ElementCount; ++index)
    {
        if (Generated::ElementTable[index].namespaceId == Generated::NamespaceId::Html)
        {
            ++htmlElementCount;
        }
    }

    ROBO_CHECK(div != nullptr);
    ROBO_CHECK_EQ(static_cast<int>(div->kind), static_cast<int>(ElementKind::Container));

    static const char* const containers[] = {
        "address", "article", "aside", "footer", "header", "main", "nav", "section", "search", "div", "hgroup"
    };

    for (const char* name : containers)
    {
        const ElementDefinition* definition = registry.FindHtmlElement(name);
        ROBO_CHECK(definition != nullptr);
        if (definition != nullptr)
        {
            ROBO_CHECK_EQ(static_cast<int>(definition->kind), static_cast<int>(ElementKind::Container));
        }
    }

    ROBO_CHECK(htmlElementCount > 100);
}

ROBO_TEST(HtmlCoverage, NestedNamespacesParse)
{
    DiagnosticSink sink;
    const std::string document = WrapBody(
        "<div><svg viewBox=\"0 0 4 4\"><g><rect x=\"1\" y=\"1\" width=\"2\" height=\"2\" fill=\"#000\"></rect></g></svg>"
        "<math><mrow><mi>a</mi><mo>+</mo><mi>b</mi></mrow></math></div>");

    std::unique_ptr<MarkupDocument> parsed = ParseHtml(document, sink);
    ROBO_CHECK(parsed != nullptr);

    if (parsed == nullptr)
    {
        return;
    }

    std::size_t svgCount = 0;
    std::size_t mathCount = 0;
    std::size_t htmlCount = 0;

    parsed->ForEachElement([&](const ElementNode& element) {
        if (element.ns == MarkupNamespace::Svg)
        {
            ++svgCount;
        }
        else if (element.ns == MarkupNamespace::MathMl)
        {
            ++mathCount;
        }
        else
        {
            ++htmlCount;
        }
    });

    ROBO_CHECK(svgCount >= 3);
    ROBO_CHECK(mathCount >= 4);
    ROBO_CHECK(htmlCount >= 3);

    ROBO_CHECK(ValidateDocument(document, sink));
}
