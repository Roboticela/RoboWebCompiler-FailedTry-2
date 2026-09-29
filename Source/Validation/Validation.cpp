#include "RoboHTMLCompiler/Validation.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace RoboHTMLCompiler
{
    namespace
    {
        bool StartsWith(std::string_view value, std::string_view prefix)
        {
            return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
        }

        bool ContainsNonWhitespace(const std::string& value)
        {
            for (const char character : value)
            {
                if (std::isspace(static_cast<unsigned char>(character)) == 0)
                {
                    return true;
                }
            }
            return false;
        }

        std::string QuoteValue(const AttributeNode& attribute)
        {
            if (!attribute.hasValue)
            {
                return attribute.name;
            }
            return attribute.name + "=\"" + attribute.value + "\"";
        }
    }

    // -----------------------------------------------------------------------
    // HtmlValidator
    // -----------------------------------------------------------------------

    HtmlValidator::HtmlValidator(DiagnosticSink& diagnosticSink,
                                 const Registry& elementRegistry,
                                 ValidationOptions validationOptions)
        : diagnostics(diagnosticSink)
        , registry(elementRegistry)
        , options(validationOptions)
    {
    }

    bool HtmlValidator::Validate(const MarkupDocument& document)
    {
        const std::size_t errorsBefore = diagnostics.ErrorCount();

        if (options.requireDocumentElement)
        {
            const bool hasDocumentElement = document.root != nullptr && document.root->documentElement != nullptr;
            if (!hasDocumentElement)
            {
                SourceLocation location;
                location.file = document.sourceFile;
                diagnostics.Error(DiagnosticCode::MissingDocumentElement,
                                  location,
                                  "The document has no <html> element.",
                                  std::string(),
                                  "Wrap the application content in <html> ... </html>.");
            }
        }

        document.ForEachElement([this](const ElementNode& element) {
            if (element.ns == MarkupNamespace::Html)
            {
                ValidateElement(element);
            }
            else if (element.ns == MarkupNamespace::Svg)
            {
                SvgValidator svgValidator(diagnostics, registry);
                if (element.parent == nullptr || element.parent->ns != MarkupNamespace::Svg)
                {
                    svgValidator.ValidateSubtree(element);
                }
            }
            else if (element.ns == MarkupNamespace::MathMl)
            {
                MathMlValidator mathMlValidator(diagnostics, registry);
                if (element.parent == nullptr || element.parent->ns != MarkupNamespace::MathMl)
                {
                    mathMlValidator.ValidateSubtree(element);
                }
            }
        });

        return diagnostics.ErrorCount() == errorsBefore;
    }

    bool HtmlValidator::ValidateElement(const ElementNode& element)
    {
        const std::size_t errorsBefore = diagnostics.ErrorCount();
        ValidateVocabulary(element);
        ValidateAttributes(element);
        return diagnostics.ErrorCount() == errorsBefore;
    }

    bool HtmlValidator::ValidateVocabulary(const ElementNode& element)
    {
        const std::size_t errorsBefore = diagnostics.ErrorCount();
        const ElementClassification classification = registry.ClassifyElement(element.sourceTag);

        switch (classification)
        {
        case ElementClassification::KnownHtml:
        {
            const ElementDefinition* definition = registry.FindHtmlElement(element.sourceTag);

            if (definition != nullptr && definition->isObsolete)
            {
                if (options.rejectObsoleteElements)
                {
                    diagnostics.Error(DiagnosticCode::ObsoleteElement,
                                      element.location,
                                      "The element <" + element.sourceTag + "> is obsolete.",
                                      std::string(),
                                      "Replace it with the element the reference recommends.",
                                      "<" + element.sourceTag + ">");
                }
                else if (options.strictness != Strictness::Lax)
                {
                    diagnostics.Warning(DiagnosticCode::ObsoleteElement,
                                        element.location,
                                        "The element <" + element.sourceTag + "> is obsolete.");
                }
            }
            break;
        }

        case ElementClassification::KnownSvg:
        case ElementClassification::KnownMathMl:
        {
            // A namespaced element written in HTML content: the namespace
            // frontends handle it once it is inside <svg> or <math>.
            diagnostics.Error(DiagnosticCode::UnknownHtmlElement,
                              element.location,
                              "<" + element.sourceTag + "> belongs to the " +
                                  std::string(classification == ElementClassification::KnownSvg ? "SVG" : "MathML") +
                                  " namespace and is not an HTML element.",
                              std::string(),
                              "Place the element inside <svg> or <math> so the dedicated frontend parses it.",
                              "<" + element.sourceTag + ">");
            break;
        }

        case ElementClassification::CustomElement:
        {
            // Custom elements are allowed provided they follow the naming rules,
            // which ClassifyElement already verified.
            break;
        }

        case ElementClassification::Unknown:
        {
            diagnostics.Error(DiagnosticCode::UnknownHtmlElement,
                              element.location,
                              "Unknown element <" + element.sourceTag + ">.",
                              std::string(),
                              "Check the spelling, or use a registered custom element name "
                              "(lowercase and containing a hyphen).",
                              "<" + element.sourceTag + ">");
            break;
        }
        }

        // Elements that embed a foreign language are rejected regardless of
        // how the source was written.
        if (element.sourceTag == "style")
        {
            diagnostics.Error(DiagnosticCode::InlineStyleElement,
                              element.location,
                              "Inline <style> is not allowed.",
                              std::string(),
                              "Move the rules into an external .css stylesheet.",
                              "<style>",
                              "<link rel=\"stylesheet\" href=\"app.css\">");
        }

        if (element.sourceTag == "script")
        {
            const AttributeNode* source = element.FindAttribute("src");
            if (source == nullptr)
            {
                diagnostics.Error(DiagnosticCode::InlineScriptContent,
                                  element.location,
                                  "<script> must reference an external JavaScript resource.",
                                  std::string(),
                                  "Move the code into a .js file and add a src attribute.",
                                  "<script>...</script>",
                                  "<script src=\"app.js\" defer></script>");
            }
            else if (ContainsNonWhitespace(element.DirectTextContent()))
            {
                diagnostics.Error(DiagnosticCode::InlineScriptContent,
                                  element.location,
                                  "<script> may not contain JavaScript source.",
                                  std::string(),
                                  "Move the code into the referenced .js file.",
                                  "<script src=\"" + source->value + "\">...</script>");
            }
        }

        // Content requirements taken from the reference vocabulary.
        if (element.sourceTag == "img")
        {
            const bool hasSource = element.HasAttribute("src") || element.HasAttribute("srcset");
            if (!hasSource)
            {
                diagnostics.Error(DiagnosticCode::MissingRequiredAttribute,
                                  element.location,
                                  "<img> requires a src or srcset attribute.",
                                  std::string(),
                                  "Add src=\"...\" or srcset=\"...\".",
                                  "<img>",
                                  "<img src=\"image.png\" alt=\"...\">");
            }
        }

        return diagnostics.ErrorCount() == errorsBefore;
    }

    bool HtmlValidator::ValidateAttributes(const ElementNode& element)
    {
        const std::size_t errorsBefore = diagnostics.ErrorCount();

        const bool isCustomElement = registry.ClassifyElement(element.sourceTag) ==
                                     ElementClassification::CustomElement;

        for (const AttributeNode& attribute : element.attributes)
        {
            // 1. CSS source may never appear in markup.
            if (attribute.name == "style")
            {
                diagnostics.Error(DiagnosticCode::InlineCssAttribute,
                                  attribute.location,
                                  "Inline CSS is not allowed.",
                                  std::string(),
                                  "Replace the style attribute with a class reference and define the rule in a "
                                  "CSS/Tailwind stylesheet.",
                                  QuoteValue(attribute),
                                  "class=\"text-red-500\"");
                continue;
            }

            // 2. JavaScript source may never appear in markup. Every 'on*'
            //    attribute is an event handler content attribute, whether or not
            //    the reference still defines it, so it is always an inline
            //    JavaScript violation rather than an unknown attribute.
            const AttributeDefinition* eventHandler =
                registry.FindAttribute(MarkupNamespace::Html, attribute.name, element.sourceTag);
            if ((eventHandler != nullptr && eventHandler->isEventHandler) || StartsWith(attribute.name, "on"))
            {
                diagnostics.Error(DiagnosticCode::InlineEventHandlerAttribute,
                                  attribute.location,
                                  "Inline JavaScript event handlers are not allowed.",
                                  std::string(),
                                  "Bind the behaviour from a JavaScript module.",
                                  QuoteValue(attribute),
                                  "element.addEventListener(\"" + std::string(attribute.name.substr(2)) +
                                      "\", handler)");
                continue;
            }

            // 3. Application metadata is intentionally extensible.
            if (attribute.isDataAttribute || Registry::IsDataAttribute(attribute.name))
            {
                continue;
            }

            // 4. ARIA attributes are validated against the ARIA registry.
            if (StartsWith(attribute.name, "aria-"))
            {
                if (!registry.IsRegisteredAttribute(MarkupNamespace::Html, attribute.name))
                {
                    diagnostics.Error(DiagnosticCode::UnknownAriaAttribute,
                                      attribute.location,
                                      "Unknown ARIA attribute '" + attribute.name + "'.",
                                      std::string(),
                                      "Use an ARIA attribute from the registry, or remove the attribute.",
                                      QuoteValue(attribute));
                }
                continue;
            }

            // 5. Custom elements may carry arbitrary non-standard attributes.
            if (isCustomElement)
            {
                continue;
            }

            const AttributeDefinition* definition =
                registry.FindAttribute(MarkupNamespace::Html, attribute.name, element.sourceTag);

            if (definition == nullptr)
            {
                if (registry.IsRegisteredAttribute(MarkupNamespace::Html, attribute.name))
                {
                    diagnostics.Error(DiagnosticCode::AttributeNotAllowed,
                                      attribute.location,
                                      "The attribute '" + attribute.name + "' is not valid on <" +
                                          element.sourceTag + ">.",
                                      std::string(),
                                      "Remove the attribute, or use an element the reference defines it for.",
                                      QuoteValue(attribute));
                }
                else
                {
                    diagnostics.Error(DiagnosticCode::UnknownAttribute,
                                      attribute.location,
                                      "Unknown attribute '" + attribute.name + "' on <" + element.sourceTag + ">.",
                                      std::string(),
                                      "Remove the attribute, correct the spelling, or use a data-* attribute.",
                                      QuoteValue(attribute));
                }
                continue;
            }

            if (definition->isObsolete && options.rejectObsoleteAttributes)
            {
                diagnostics.Error(DiagnosticCode::ObsoleteAttribute,
                                  attribute.location,
                                  "The attribute '" + attribute.name + "' is obsolete.",
                                  std::string(),
                                  "Remove the attribute and express the intent with current vocabulary.",
                                  QuoteValue(attribute));
                continue;
            }

            // 6. Attribute values with a closed vocabulary are checked.
            if (attribute.name == "type" && element.sourceTag == "input")
            {
                const std::string typeValue = attribute.value.empty() ? "text" : attribute.value;
                if (!registry.IsKnownInputType(typeValue))
                {
                    diagnostics.Error(DiagnosticCode::InvalidInputType,
                                      attribute.location,
                                      "Unknown input type '" + typeValue + "'.",
                                      std::string(),
                                      "Use a registered input type.",
                                      QuoteValue(attribute));
                }
                else if (registry.IsObsoleteInputType(typeValue))
                {
                    diagnostics.Error(DiagnosticCode::ObsoleteAttribute,
                                      attribute.location,
                                      "The input type '" + typeValue + "' is obsolete.",
                                      std::string(),
                                      "Use a current input type such as datetime-local.",
                                      QuoteValue(attribute));
                }
            }
        }

        return diagnostics.ErrorCount() == errorsBefore;
    }

    // -----------------------------------------------------------------------
    // SvgValidator
    // -----------------------------------------------------------------------

    SvgValidator::SvgValidator(DiagnosticSink& diagnosticSink, const Registry& elementRegistry)
        : diagnostics(diagnosticSink)
        , registry(elementRegistry)
    {
    }

    bool SvgValidator::ValidateSubtree(const ElementNode& element)
    {
        const std::size_t errorsBefore = diagnostics.ErrorCount();

        if (element.ns == MarkupNamespace::Svg)
        {
            ValidateElement(element);
        }

        for (const std::unique_ptr<Node>& child : element.children)
        {
            if (child->kind != NodeKind::Element)
            {
                continue;
            }

            const ElementNode& childElement = *static_cast<const ElementNode*>(child.get());

            if (childElement.ns == MarkupNamespace::Svg)
            {
                ValidateSubtree(childElement);
            }
            else if (childElement.ns == MarkupNamespace::Html)
            {
                // HTML integration point (foreignObject).
                HtmlValidator htmlValidator(diagnostics, registry, ValidationOptions{});
                htmlValidator.ValidateElement(childElement);
            }
        }

        return diagnostics.ErrorCount() == errorsBefore;
    }

    bool SvgValidator::ValidateElement(const ElementNode& element)
    {
        const std::size_t errorsBefore = diagnostics.ErrorCount();

        const ElementDefinition* definition = registry.FindElement(MarkupNamespace::Svg, element.sourceTag);
        if (definition == nullptr)
        {
            diagnostics.Error(DiagnosticCode::UnknownSvgElement,
                              element.location,
                              "Unknown SVG element <" + element.sourceTag + ">.",
                              std::string(),
                              "Correct the element name.",
                              "<" + element.sourceTag + ">");
            return false;
        }

        if (element.sourceTag == "style")
        {
            diagnostics.Error(DiagnosticCode::InlineStyleElement,
                              element.location,
                              "Inline <style> is not allowed inside SVG.",
                              std::string(),
                              "Move the rules into an external CSS/Tailwind stylesheet.",
                              "<style>");
        }

        if (element.sourceTag == "script")
        {
            diagnostics.Error(DiagnosticCode::InlineScriptContent,
                              element.location,
                              "Inline <script> is not allowed inside SVG.",
                              std::string(),
                              "Behaviour belongs to the JavaScript module.",
                              "<script>");
        }

        for (const AttributeNode& attribute : element.attributes)
        {
            if (attribute.name == "style")
            {
                diagnostics.Error(DiagnosticCode::InlineCssAttribute,
                                  attribute.location,
                                  "Inline CSS is not allowed.",
                                  std::string(),
                                  "Use a class reference resolved by the CSS/Tailwind compiler.",
                                  QuoteValue(attribute),
                                  "class=\"fill-red-500\"");
                continue;
            }

            if (StartsWith(attribute.name, "on"))
            {
                diagnostics.Error(DiagnosticCode::InlineEventHandlerAttribute,
                                  attribute.location,
                                  "Inline JavaScript event handlers are not allowed.",
                                  std::string(),
                                  "Bind the behaviour from a JavaScript module.",
                                  QuoteValue(attribute));
                continue;
            }

            if (attribute.isDataAttribute || Registry::IsDataAttribute(attribute.name))
            {
                continue;
            }

            if (StartsWith(attribute.name, "aria-"))
            {
                if (!registry.IsRegisteredAttribute(MarkupNamespace::Html, attribute.name))
                {
                    diagnostics.Error(DiagnosticCode::UnknownAriaAttribute,
                                      attribute.location,
                                      "Unknown ARIA attribute '" + attribute.name + "'.",
                                      std::string(),
                                      "Use an ARIA attribute from the registry.",
                                      QuoteValue(attribute));
                }
                continue;
            }

            const AttributeDefinition* attributeDefinition =
                registry.FindAttribute(MarkupNamespace::Svg, attribute.name, element.sourceTag);

            if (attributeDefinition == nullptr)
            {
                diagnostics.Error(DiagnosticCode::InvalidSvgAttribute,
                                  attribute.location,
                                  "The SVG attribute '" + attribute.name + "' is not valid on <" +
                                      element.sourceTag + ">.",
                                  std::string(),
                                  "Remove the attribute, or use it on an element the SVG registry allows.",
                                  QuoteValue(attribute));
                continue;
            }

            if (attributeDefinition->isObsolete)
            {
                diagnostics.Error(DiagnosticCode::ObsoleteAttribute,
                                  attribute.location,
                                  "The SVG attribute '" + attribute.name + "' is obsolete.",
                                  std::string(),
                                  "Remove the attribute.",
                                  QuoteValue(attribute));
            }
        }

        return diagnostics.ErrorCount() == errorsBefore;
    }

    // -----------------------------------------------------------------------
    // MathMlValidator
    // -----------------------------------------------------------------------

    MathMlValidator::MathMlValidator(DiagnosticSink& diagnosticSink, const Registry& elementRegistry)
        : diagnostics(diagnosticSink)
        , registry(elementRegistry)
    {
    }

    bool MathMlValidator::ValidateSubtree(const ElementNode& element)
    {
        const std::size_t errorsBefore = diagnostics.ErrorCount();

        if (element.ns == MarkupNamespace::MathMl)
        {
            ValidateElement(element);
        }

        for (const std::unique_ptr<Node>& child : element.children)
        {
            if (child->kind != NodeKind::Element)
            {
                continue;
            }

            const ElementNode& childElement = *static_cast<const ElementNode*>(child.get());
            if (childElement.ns == MarkupNamespace::MathMl)
            {
                ValidateSubtree(childElement);
            }
            else if (childElement.ns == MarkupNamespace::Html)
            {
                // HTML integration point (annotation-xml with an HTML encoding).
                HtmlValidator htmlValidator(diagnostics, registry, ValidationOptions{});
                htmlValidator.ValidateElement(childElement);
            }
        }

        return diagnostics.ErrorCount() == errorsBefore;
    }

    bool MathMlValidator::ValidateElement(const ElementNode& element)
    {
        const std::size_t errorsBefore = diagnostics.ErrorCount();

        const ElementDefinition* definition = registry.FindElement(MarkupNamespace::MathMl, element.sourceTag);
        if (definition == nullptr)
        {
            diagnostics.Error(DiagnosticCode::UnknownMathMlElement,
                              element.location,
                              "Unknown MathML element <" + element.sourceTag + ">.",
                              std::string(),
                              "Correct the element name.",
                              "<" + element.sourceTag + ">");
            return false;
        }

        for (const AttributeNode& attribute : element.attributes)
        {
            if (attribute.name == "style")
            {
                diagnostics.Error(DiagnosticCode::InlineCssAttribute,
                                  attribute.location,
                                  "Inline CSS is not allowed.",
                                  std::string(),
                                  "Use a class reference resolved by the CSS/Tailwind compiler.",
                                  QuoteValue(attribute));
                continue;
            }

            if (StartsWith(attribute.name, "on"))
            {
                diagnostics.Error(DiagnosticCode::InlineEventHandlerAttribute,
                                  attribute.location,
                                  "Inline JavaScript event handlers are not allowed.",
                                  std::string(),
                                  "Bind the behaviour from a JavaScript module.",
                                  QuoteValue(attribute));
                continue;
            }

            if (attribute.isDataAttribute || Registry::IsDataAttribute(attribute.name))
            {
                continue;
            }

            if (StartsWith(attribute.name, "aria-"))
            {
                if (!registry.IsRegisteredAttribute(MarkupNamespace::Html, attribute.name))
                {
                    diagnostics.Error(DiagnosticCode::UnknownAriaAttribute,
                                      attribute.location,
                                      "Unknown ARIA attribute '" + attribute.name + "'.",
                                      std::string(),
                                      "Use an ARIA attribute from the registry.",
                                      QuoteValue(attribute));
                }
                continue;
            }

            const AttributeDefinition* attributeDefinition =
                registry.FindAttribute(MarkupNamespace::MathMl, attribute.name, element.sourceTag);

            if (attributeDefinition == nullptr)
            {
                diagnostics.Error(DiagnosticCode::InvalidMathMlAttribute,
                                  attribute.location,
                                  "The MathML attribute '" + attribute.name + "' is not valid on <" +
                                      element.sourceTag + ">.",
                                  std::string(),
                                  "Remove the attribute, or use it on an element the MathML registry allows.",
                                  QuoteValue(attribute));
            }
        }

        return diagnostics.ErrorCount() == errorsBefore;
    }
}
