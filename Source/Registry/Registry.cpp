#include "RoboHTMLCompiler/Registry.h"

#include "RoboHTMLCompiler/Ast.h"

#include <algorithm>
#include <cctype>

namespace RoboHTMLCompiler
{
    const char* ToString(MarkupNamespace value)
    {
        switch (value)
        {
        case MarkupNamespace::Html: return "HTML";
        case MarkupNamespace::Svg: return "SVG";
        case MarkupNamespace::MathMl: return "MathML";
        }
        return "HTML";
    }

    std::size_t Registry::ElementKeyHash::operator()(const ElementKey& key) const
    {
        return std::hash<std::string>()(key.name) * 31u + static_cast<std::size_t>(key.ns);
    }

    bool Registry::ElementKeyEqual::operator()(const ElementKey& left, const ElementKey& right) const
    {
        return left.ns == right.ns && left.name == right.name;
    }

    std::size_t Registry::AttributeKeyHash::operator()(const AttributeKey& key) const
    {
        return std::hash<std::string>()(key.name) * 31u + static_cast<std::size_t>(key.ns);
    }

    bool Registry::AttributeKeyEqual::operator()(const AttributeKey& left, const AttributeKey& right) const
    {
        return left.ns == right.ns && left.name == right.name;
    }

    namespace
    {
        MarkupNamespace ToMarkupNamespace(Generated::NamespaceId id)
        {
            switch (id)
            {
            case Generated::NamespaceId::Html: return MarkupNamespace::Html;
            case Generated::NamespaceId::Svg: return MarkupNamespace::Svg;
            case Generated::NamespaceId::MathMl: return MarkupNamespace::MathMl;
            default: return MarkupNamespace::Html;
            }
        }
    }

    Registry::Registry()
    {
        for (std::size_t index = 0; index < Generated::ElementCount; ++index)
        {
            const Generated::ElementRecord& record = Generated::ElementTable[index];

            ElementDefinition definition;
            definition.name = record.name;
            definition.ns = ToMarkupNamespace(record.namespaceId);
            definition.kind = record.kind;
            definition.headingLevel = record.headingLevel;
            definition.isVoid = (record.flags & Generated::ElementFlagVoid) != 0;
            definition.isObsolete = (record.flags & Generated::ElementFlagObsolete) != 0;
            definition.acceptsGlobalAttributes = (record.flags & Generated::ElementFlagGlobalAttributes) != 0;
            definition.isOrderedList = (record.flags & Generated::ElementFlagOrdered) != 0;
            definition.isRawText = (record.flags & Generated::ElementFlagRawText) != 0;
            definition.isPhrasing = (record.flags & Generated::ElementFlagPhrasing) != 0;
            definition.isFormAssociated = (record.flags & Generated::ElementFlagFormAssociated) != 0;
            definition.isInteractive = (record.flags & Generated::ElementFlagInteractive) != 0;
            definition.sectionId = record.sectionId;
            definition.domInterface = record.domInterface;

            for (std::uint32_t attributeIndex = 0; attributeIndex < record.attributeNameCount; ++attributeIndex)
            {
                definition.declaredAttributes.push_back(record.attributeNames[attributeIndex]);
            }

            // Lookup is case-insensitive: the markup lexer lower-cases tag and
            // attribute names, while SVG and MathML declare camel-cased
            // spellings such as 'viewBox' or 'linearGradient'. The record keeps
            // the canonical spelling so diagnostics stay source-accurate.
            elements[ElementKey{ definition.ns, ToLowerAscii(definition.name) }] = std::move(definition);
        }

        for (std::size_t index = 0; index < Generated::AttributeCount; ++index)
        {
            const Generated::AttributeRecord& record = Generated::AttributeTable[index];

            AttributeDefinition definition;
            definition.name = record.name;
            definition.ns = ToMarkupNamespace(record.namespaceId);
            definition.nativeProperty = record.property;
            definition.namespaceDefault = (record.flags & Generated::AttributeFlagNamespaceDefault) != 0;
            definition.isGlobal = (record.flags & Generated::AttributeFlagGlobal) != 0;
            definition.isEventHandler = (record.flags & Generated::AttributeFlagEventHandler) != 0;
            definition.isForbidden = (record.flags & Generated::AttributeFlagForbidden) != 0;
            definition.isObsolete = (record.flags & Generated::AttributeFlagObsolete) != 0;
            definition.isAria = (record.flags & Generated::AttributeFlagAria) != 0;
            definition.compileTime = (record.flags & Generated::AttributeFlagCompileTime) != 0;
            definition.runtimeMutable = (record.flags & Generated::AttributeFlagRuntimeMutable) != 0;
            definition.browserPreserved = (record.flags & Generated::AttributeFlagBrowserPreserved) != 0;
            definition.valueType = record.valueType;
            definition.category = record.category;

            for (std::uint32_t allowedIndex = 0; allowedIndex < record.allowedElementCount; ++allowedIndex)
            {
                definition.allowedElements.push_back(record.allowedElements[allowedIndex]);
            }

            attributes[AttributeKey{ definition.ns, ToLowerAscii(definition.name) }].push_back(std::move(definition));
        }

        for (std::size_t index = 0; index < Generated::InputTypeCount; ++index)
        {
            inputTypes[Generated::InputTypeTable[index].typeValue] = Generated::InputTypeTable[index];
        }

        elementCount = Generated::ElementCount;
        attributeRecordCount = Generated::AttributeCount;
    }

    const Registry& Registry::Instance()
    {
        static const Registry instance;
        return instance;
    }

    const ElementDefinition* Registry::FindElement(MarkupNamespace ns, std::string_view name) const
    {
        if (name.empty())
        {
            return nullptr;
        }

        const auto iterator = elements.find(ElementKey{ ns, ToLowerAscii(name) });
        return iterator != elements.end() ? &iterator->second : nullptr;
    }

    const ElementDefinition* Registry::FindHtmlElement(std::string_view name) const
    {
        return FindElement(MarkupNamespace::Html, name);
    }

    const AttributeDefinition* Registry::FindAttribute(MarkupNamespace ns,
                                                       std::string_view attributeName,
                                                       std::string_view elementName) const
    {
        if (attributeName.empty())
        {
            return nullptr;
        }

        const auto iterator = attributes.find(AttributeKey{ ns, ToLowerAscii(attributeName) });
        if (iterator == attributes.end())
        {
            return nullptr;
        }

        const std::string loweredElement = ToLowerAscii(elementName);

        const AttributeDefinition* fallback = nullptr;
        for (const AttributeDefinition& definition : iterator->second)
        {
            if (!definition.allowedElements.empty())
            {
                const bool allowed = std::any_of(
                    definition.allowedElements.begin(),
                    definition.allowedElements.end(),
                    [&](const std::string& candidate) { return ToLowerAscii(candidate) == loweredElement; });
                if (allowed)
                {
                    return &definition;
                }
                continue;
            }

            fallback = &definition;
        }

        return fallback;
    }

    bool Registry::IsRegisteredAttribute(MarkupNamespace ns, std::string_view attributeName) const
    {
        return attributes.find(AttributeKey{ ns, ToLowerAscii(attributeName) }) != attributes.end();
    }

    bool Registry::IsObsoleteElement(MarkupNamespace ns, std::string_view name) const
    {
        const ElementDefinition* definition = FindElement(ns, name);
        return definition != nullptr && definition->isObsolete;
    }

    bool Registry::IsObsoleteAttribute(std::string_view attributeName) const
    {
        const auto iterator = attributes.find(AttributeKey{ MarkupNamespace::Html, ToLowerAscii(attributeName) });
        if (iterator == attributes.end())
        {
            return false;
        }

        // Obsolete-ness is per element: an attribute is only obsolete when every
        // element it is declared on is obsolete. 'name' on <embed> is obsolete
        // while 'name' on <input> is not, and the attribute stays current.
        for (const AttributeDefinition& definition : iterator->second)
        {
            if (!definition.isObsolete)
            {
                return false;
            }
        }
        return true;
    }

    ElementKind Registry::ResolveInputKind(std::string_view typeValue) const
    {
        const std::string key = typeValue.empty() ? std::string("text") : ToLowerAscii(typeValue);
        const auto iterator = inputTypes.find(key);
        if (iterator != inputTypes.end())
        {
            return iterator->second.kind;
        }

        return ElementKind::Input;
    }

    bool Registry::IsKnownInputType(std::string_view typeValue) const
    {
        return inputTypes.find(ToLowerAscii(typeValue)) != inputTypes.end();
    }

    bool Registry::IsObsoleteInputType(std::string_view typeValue) const
    {
        const auto iterator = inputTypes.find(ToLowerAscii(typeValue));
        if (iterator == inputTypes.end())
        {
            return false;
        }
        return (iterator->second.flags & Generated::AttributeFlagObsolete) != 0;
    }

    ElementClassification Registry::ClassifyElement(std::string_view name) const
    {
        if (FindElement(MarkupNamespace::Html, name) != nullptr)
        {
            return ElementClassification::KnownHtml;
        }
        if (FindElement(MarkupNamespace::Svg, name) != nullptr)
        {
            return ElementClassification::KnownSvg;
        }
        if (FindElement(MarkupNamespace::MathMl, name) != nullptr)
        {
            return ElementClassification::KnownMathMl;
        }
        if (IsValidCustomElementName(name))
        {
            return ElementClassification::CustomElement;
        }
        return ElementClassification::Unknown;
    }

    bool Registry::IsValidCustomElementName(std::string_view name)
    {
        if (name.empty())
        {
            return false;
        }

        if (std::isalpha(static_cast<unsigned char>(name[0])) == 0)
        {
            return false;
        }

        bool hasHyphen = false;
        for (const char character : name)
        {
            if (std::isupper(static_cast<unsigned char>(character)) != 0)
            {
                return false;
            }

            if (std::islower(static_cast<unsigned char>(character)) == 0 &&
                std::isdigit(static_cast<unsigned char>(character)) == 0 && character != '-')
            {
                return false;
            }

            if (character == '-')
            {
                hasHyphen = true;
            }
        }

        return hasHyphen;
    }

    bool Registry::IsDataAttribute(std::string_view name)
    {
        if (name.size() <= 5)
        {
            return false;
        }

        if (name.compare(0, 5, "data-") != 0)
        {
            return false;
        }

        for (std::size_t index = 5; index < name.size(); ++index)
        {
            const char character = name[index];
            if (std::islower(static_cast<unsigned char>(character)) == 0 &&
                std::isdigit(static_cast<unsigned char>(character)) == 0 && character != '-' &&
                character != '_')
            {
                return false;
            }
        }

        return true;
    }
}
