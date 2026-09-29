#include "../TestFramework.h"
#include "../TestHelpers.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace RoboHTMLCompiler;
using namespace RoboTest;

namespace
{
    const std::vector<std::string>& HtmlElementNames()
    {
        static std::vector<std::string> names = [] {
            std::vector<std::string> collected;
            for (std::size_t index = 0; index < Generated::ElementCount; ++index)
            {
                if (Generated::ElementTable[index].namespaceId == Generated::NamespaceId::Html)
                {
                    collected.emplace_back(Generated::ElementTable[index].name);
                }
            }
            return collected;
        }();
        return names;
    }

    const std::vector<std::string>& SvgElementNames()
    {
        static std::vector<std::string> names = [] {
            std::vector<std::string> collected;
            for (std::size_t index = 0; index < Generated::ElementCount; ++index)
            {
                if (Generated::ElementTable[index].namespaceId == Generated::NamespaceId::Svg)
                {
                    collected.emplace_back(Generated::ElementTable[index].name);
                }
            }
            return collected;
        }();
        return names;
    }

    const std::vector<std::string>& MathMlElementNames()
    {
        static std::vector<std::string> names = [] {
            std::vector<std::string> collected;
            for (std::size_t index = 0; index < Generated::ElementCount; ++index)
            {
                if (Generated::ElementTable[index].namespaceId == Generated::NamespaceId::MathMl)
                {
                    collected.emplace_back(Generated::ElementTable[index].name);
                }
            }
            return collected;
        }();
        return names;
    }
}

ROBO_TEST(Registry, EveryHtmlElementHasANativeCategory)
{
    const Registry& registry = Registry::Instance();

    ROBO_CHECK(HtmlElementNames().size() >= 100);

    for (const std::string& name : HtmlElementNames())
    {
        const ElementDefinition* definition = registry.FindHtmlElement(name);
        ROBO_CHECK(definition != nullptr);
        if (definition == nullptr)
        {
            continue;
        }

        if (definition->isObsolete)
        {
            ROBO_CHECK_EQ(static_cast<int>(definition->kind), static_cast<int>(ElementKind::ObsoleteElement));
        }
        else
        {
            ROBO_CHECK(static_cast<int>(definition->kind) != static_cast<int>(ElementKind::None));
        }
    }
}

ROBO_TEST(Registry, EveryHtmlElementIsClassifiable)
{
    const Registry& registry = Registry::Instance();

    for (const std::string& name : HtmlElementNames())
    {
        const ElementClassification classification = registry.ClassifyElement(name);
        ROBO_CHECK_EQ(static_cast<int>(classification), static_cast<int>(ElementClassification::KnownHtml));
    }
}

ROBO_TEST(Registry, EveryDeclaredElementAttributeIsRegistered)
{
    const Registry& registry = Registry::Instance();

    for (const std::string& name : HtmlElementNames())
    {
        const ElementDefinition* definition = registry.FindHtmlElement(name);
        if (definition == nullptr)
        {
            continue;
        }

        for (const std::string& attribute : definition->declaredAttributes)
        {
            const AttributeDefinition* attributeDefinition =
                registry.FindAttribute(MarkupNamespace::Html, attribute, name);
            ROBO_CHECK(attributeDefinition != nullptr);
        }
    }
}

ROBO_TEST(Registry, EveryAttributeRecordHasANativeProperty)
{
    for (std::size_t index = 0; index < Generated::AttributeCount; ++index)
    {
        const Generated::AttributeRecord& record = Generated::AttributeTable[index];
        const std::string name = record.name;
        ROBO_CHECK(!name.empty());
        ROBO_CHECK(record.category != nullptr);
    }
}

ROBO_TEST(Registry, ObsoleteElementsAreRegisteredAsDeprecated)
{
    const Registry& registry = Registry::Instance();

    static const char* const obsolete[] = {
        "applet", "acronym", "bgsound", "basefont", "big", "blink", "center", "dir", "font",
        "frame", "frameset", "isindex", "keygen", "listing", "marquee", "menuitem", "multicol",
        "nextid", "nobr", "noembed", "noframes", "param", "plaintext", "rb", "rtc", "spacer",
        "strike", "tt", "xmp"
    };

    for (const char* name : obsolete)
    {
        ROBO_CHECK(registry.IsObsoleteElement(MarkupNamespace::Html, name));
    }
}

ROBO_TEST(Registry, GlobalAttributesApplyEverywhere)
{
    const Registry& registry = Registry::Instance();

    static const char* const globals[] = {
        "accesskey", "autocapitalize", "autocorrect", "autofocus", "class", "contenteditable",
        "dir", "draggable", "enterkeyhint", "hidden", "id", "inert", "inputmode", "is",
        "itemid", "itemprop", "itemref", "itemscope", "itemtype", "lang", "nonce", "popover",
        "slot", "spellcheck", "tabindex", "title", "translate"
    };

    for (const char* name : globals)
    {
        ROBO_CHECK(registry.FindAttribute(MarkupNamespace::Html, name, "div") != nullptr);
        ROBO_CHECK(registry.FindAttribute(MarkupNamespace::Html, name, "button") != nullptr);
    }
}

ROBO_TEST(Registry, AttributeRoutingIsTyped)
{
    const Registry& registry = Registry::Instance();

    const AttributeDefinition* identifier = registry.FindAttribute(MarkupNamespace::Html, "id", "button");
    ROBO_CHECK(identifier != nullptr);
    ROBO_CHECK_EQ(static_cast<int>(identifier->nativeProperty), static_cast<int>(NativeProperty::IdentityId));

    const AttributeDefinition* classes = registry.FindAttribute(MarkupNamespace::Html, "class", "button");
    ROBO_CHECK(classes != nullptr);
    ROBO_CHECK_EQ(static_cast<int>(classes->nativeProperty), static_cast<int>(NativeProperty::StyleReference));

    const AttributeDefinition* disabled = registry.FindAttribute(MarkupNamespace::Html, "disabled", "button");
    ROBO_CHECK(disabled != nullptr);
    ROBO_CHECK_EQ(static_cast<int>(disabled->nativeProperty), static_cast<int>(NativeProperty::StateDisabled));

    const AttributeDefinition* width = registry.FindAttribute(MarkupNamespace::Html, "width", "img");
    ROBO_CHECK(width != nullptr);
    ROBO_CHECK_EQ(static_cast<int>(width->nativeProperty), static_cast<int>(NativeProperty::LayoutWidth));
}

ROBO_TEST(Registry, ElementScopedRoutesOverrideDefaults)
{
    const Registry& registry = Registry::Instance();

    const AttributeDefinition* inputType = registry.FindAttribute(MarkupNamespace::Html, "type", "input");
    ROBO_CHECK(inputType != nullptr);
    ROBO_CHECK_EQ(static_cast<int>(inputType->nativeProperty), static_cast<int>(NativeProperty::InputType));

    const AttributeDefinition* scriptType = registry.FindAttribute(MarkupNamespace::Html, "type", "script");
    ROBO_CHECK(scriptType != nullptr);
    ROBO_CHECK_EQ(static_cast<int>(scriptType->nativeProperty), static_cast<int>(NativeProperty::ResourceMimeType));
}

ROBO_TEST(Registry, AttributeNotValidForElementIsNotRegisteredOnIt)
{
    const Registry& registry = Registry::Instance();

    // disabled is a form-control state, not a container state.
    ROBO_CHECK(registry.FindAttribute(MarkupNamespace::Html, "disabled", "div") == nullptr);
    ROBO_CHECK(registry.IsRegisteredAttribute(MarkupNamespace::Html, "disabled"));
    ROBO_CHECK(registry.FindAttribute(MarkupNamespace::Html, "disabled", "button") != nullptr);
}

ROBO_TEST(Registry, EveryInputTypeResolvesToANativeKind)
{
    const Registry& registry = Registry::Instance();

    static const char* const types[] = {
        "text", "email", "password", "number", "date", "datetime-local", "month", "week", "time",
        "file", "checkbox", "radio", "range", "color", "search", "url", "tel", "hidden",
        "submit", "reset", "button", "image"
    };

    for (const char* type : types)
    {
        ROBO_CHECK(registry.IsKnownInputType(type));
        ROBO_CHECK(static_cast<int>(registry.ResolveInputKind(type)) != static_cast<int>(ElementKind::None));
    }

    ROBO_CHECK_EQ(static_cast<int>(registry.ResolveInputKind("checkbox")),
                  static_cast<int>(ElementKind::Checkbox));
    ROBO_CHECK_EQ(static_cast<int>(registry.ResolveInputKind("datetime-local")),
                  static_cast<int>(ElementKind::DateTimeInput));
    ROBO_CHECK_EQ(static_cast<int>(registry.ResolveInputKind("color")),
                  static_cast<int>(ElementKind::ColorInput));
    ROBO_CHECK_EQ(static_cast<int>(registry.ResolveInputKind("")),
                  static_cast<int>(ElementKind::Input));
    ROBO_CHECK(!registry.IsKnownInputType("nope"));
}

ROBO_TEST(Registry, CustomElementNameRules)
{
    ROBO_CHECK(Registry::IsValidCustomElementName("my-component"));
    ROBO_CHECK(Registry::IsValidCustomElementName("x-app-shell"));
    ROBO_CHECK(!Registry::IsValidCustomElementName("mycomponent"));
    ROBO_CHECK(!Registry::IsValidCustomElementName("MyComponent"));
    ROBO_CHECK(!Registry::IsValidCustomElementName("-component"));
    ROBO_CHECK(!Registry::IsValidCustomElementName("1-component"));
}

ROBO_TEST(Registry, DataAttributeRules)
{
    ROBO_CHECK(Registry::IsDataAttribute("data-user-id"));
    ROBO_CHECK(Registry::IsDataAttribute("data-x"));
    ROBO_CHECK(!Registry::IsDataAttribute("data-"));
    ROBO_CHECK(!Registry::IsDataAttribute("dataUser"));
    ROBO_CHECK(!Registry::IsDataAttribute("data-User"));
}

ROBO_TEST(Registry, AriaAttributesAreRegisteredAndTyped)
{
    const Registry& registry = Registry::Instance();

    static const std::pair<const char*, NativeProperty> expected[] = {
        { "aria-label", NativeProperty::AccessibilityLabel },
        { "aria-labelledby", NativeProperty::AccessibilityLabelledBy },
        { "aria-describedby", NativeProperty::AccessibilityDescribedBy },
        { "aria-hidden", NativeProperty::AccessibilityHidden },
        { "aria-disabled", NativeProperty::AccessibilityDisabled },
        { "aria-live", NativeProperty::AccessibilityLive },
        { "aria-expanded", NativeProperty::AccessibilityExpanded },
        { "aria-valuenow", NativeProperty::AccessibilityValueNow },
        { "role", NativeProperty::AccessibilityRole }
    };

    for (const auto& entry : expected)
    {
        const AttributeDefinition* definition =
            registry.FindAttribute(MarkupNamespace::Html, entry.first, "div");
        ROBO_CHECK(definition != nullptr);
        if (definition != nullptr)
        {
            ROBO_CHECK_EQ(static_cast<int>(definition->nativeProperty), static_cast<int>(entry.second));
        }
    }

    ROBO_CHECK(!registry.IsRegisteredAttribute(MarkupNamespace::Html, "aria-nonsense"));
}

ROBO_TEST(Registry, SvgVocabularyIsRecognised)
{
    const Registry& registry = Registry::Instance();

    ROBO_CHECK(SvgElementNames().size() >= 50);

    for (const std::string& name : SvgElementNames())
    {
        const ElementDefinition* definition = registry.FindElement(MarkupNamespace::Svg, name);
        ROBO_CHECK(definition != nullptr);
        if (definition != nullptr)
        {
            ROBO_CHECK(static_cast<int>(definition->kind) != static_cast<int>(ElementKind::None));
        }
    }

    ROBO_CHECK_EQ(static_cast<int>(registry.FindElement(MarkupNamespace::Svg, "path")->kind),
                  static_cast<int>(ElementKind::SvgPath));
    ROBO_CHECK_EQ(static_cast<int>(registry.FindElement(MarkupNamespace::Svg, "feGaussianBlur")->kind),
                  static_cast<int>(ElementKind::SvgFilterPrimitive));
}

ROBO_TEST(Registry, SvgAttributesAreScopedToTheirElements)
{
    const Registry& registry = Registry::Instance();

    ROBO_CHECK(registry.FindAttribute(MarkupNamespace::Svg, "d", "path") != nullptr);
    ROBO_CHECK(registry.FindAttribute(MarkupNamespace::Svg, "d", "circle") == nullptr);

    ROBO_CHECK(registry.FindAttribute(MarkupNamespace::Svg, "cx", "circle") != nullptr);
    ROBO_CHECK(registry.FindAttribute(MarkupNamespace::Svg, "cx", "rect") == nullptr);

    ROBO_CHECK(registry.FindAttribute(MarkupNamespace::Svg, "fill", "circle") != nullptr);
    ROBO_CHECK(!registry.IsRegisteredAttribute(MarkupNamespace::Svg, "bogus-svg-attribute"));
}

ROBO_TEST(Registry, MathMlVocabularyIsRecognised)
{
    const Registry& registry = Registry::Instance();

    ROBO_CHECK(MathMlElementNames().size() >= 25);

    for (const std::string& name : MathMlElementNames())
    {
        ROBO_CHECK(registry.FindElement(MarkupNamespace::MathMl, name) != nullptr);
    }

    ROBO_CHECK_EQ(static_cast<int>(registry.FindElement(MarkupNamespace::MathMl, "mfrac")->kind),
                  static_cast<int>(ElementKind::MathFraction));
    ROBO_CHECK_EQ(static_cast<int>(registry.FindElement(MarkupNamespace::MathMl, "msup")->kind),
                  static_cast<int>(ElementKind::MathScript));
}

ROBO_TEST(Registry, MathMlAttributesAreScopedToTheirElements)
{
    const Registry& registry = Registry::Instance();

    ROBO_CHECK(registry.FindAttribute(MarkupNamespace::MathMl, "display", "math") != nullptr);
    ROBO_CHECK(registry.FindAttribute(MarkupNamespace::MathMl, "display", "mi") == nullptr);
    ROBO_CHECK(registry.FindAttribute(MarkupNamespace::MathMl, "linethickness", "mfrac") != nullptr);
    ROBO_CHECK(!registry.IsRegisteredAttribute(MarkupNamespace::MathMl, "bogus-math-attribute"));
}

ROBO_TEST(Registry, ObsoleteAttributesAreMarked)
{
    const Registry& registry = Registry::Instance();
    ROBO_CHECK(registry.IsObsoleteAttribute("align"));
}
