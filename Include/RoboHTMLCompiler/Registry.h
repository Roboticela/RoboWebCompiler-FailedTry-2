// ---------------------------------------------------------------------------
// RoboHTMLCompiler - the compiler's structured knowledge.
//
// The registries produced by RoboHTMLCompilerTools are compiled into the binary
// as strongly typed tables (see CompilerKnowledge/Include/RoboHTMLCompiler). This header
// presents them as queryable, immutable definitions. The compiler never parses
// JSON, and the reference documentation is an engineering input rather than a
// runtime dependency.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/GeneratedEnums.h"
#include "RoboHTMLCompiler/GeneratedRegistries.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace RoboHTMLCompiler
{
    enum class MarkupNamespace
    {
        Html,
        Svg,
        MathMl
    };

    const char* ToString(MarkupNamespace value);

    /// One entry of the element registry.
    struct ElementDefinition
    {
        std::string name;
        MarkupNamespace ns = MarkupNamespace::Html;
        ElementKind kind = ElementKind::None;
        int headingLevel = 0;
        bool isVoid = false;
        bool isObsolete = false;
        bool acceptsGlobalAttributes = false;
        bool isOrderedList = false;
        bool isRawText = false;
        bool isPhrasing = false;
        bool isFormAssociated = false;
        bool isInteractive = false;

        /// Attributes the reference declares for this element specifically.
        std::vector<std::string> declaredAttributes;

        std::string sectionId;
        std::string domInterface;
    };

    /// One entry of the attribute registry. A single attribute may contribute
    /// several definitions: a namespace-wide default plus per-element overrides.
    struct AttributeDefinition
    {
        std::string name;
        MarkupNamespace ns = MarkupNamespace::Html;
        NativeProperty nativeProperty = NativeProperty::Unmapped;
        bool namespaceDefault = true;
        bool isGlobal = false;
        bool isEventHandler = false;
        bool isForbidden = false;
        bool isObsolete = false;
        bool isAria = false;
        bool compileTime = true;
        bool runtimeMutable = false;
        bool browserPreserved = true;
        std::string valueType;
        std::string category;

        /// Empty means "any element in the namespace".
        std::vector<std::string> allowedElements;
    };

    /// Result of classifying a source element name.
    enum class ElementClassification
    {
        KnownHtml,
        KnownSvg,
        KnownMathMl,
        CustomElement,
        Unknown
    };

    class Registry
    {
    public:
        /// Process-wide registry, built from the generated tables on first use.
        static const Registry& Instance();

        const ElementDefinition* FindElement(MarkupNamespace ns, std::string_view name) const;
        const ElementDefinition* FindHtmlElement(std::string_view name) const;

        /// Finds the attribute definition that applies on a specific element.
        /// Element-scoped definitions win over namespace-wide defaults.
        const AttributeDefinition* FindAttribute(MarkupNamespace ns,
                                                 std::string_view attributeName,
                                                 std::string_view elementName) const;

        /// True when the attribute is registered at all in the namespace, even
        /// if it is not permitted on the element in question.
        bool IsRegisteredAttribute(MarkupNamespace ns, std::string_view attributeName) const;

        bool IsObsoleteElement(MarkupNamespace ns, std::string_view name) const;
        bool IsObsoleteAttribute(std::string_view attributeName) const;

        /// Specialises an input element from its type content attribute.
        ElementKind ResolveInputKind(std::string_view typeValue) const;
        bool IsKnownInputType(std::string_view typeValue) const;
        bool IsObsoleteInputType(std::string_view typeValue) const;

        /// Classifies a source tag name across all namespaces.
        ElementClassification ClassifyElement(std::string_view name) const;

        /// True when the name follows the custom element naming rules
        /// (lowercase, contains a hyphen, starts with an ASCII letter).
        static bool IsValidCustomElementName(std::string_view name);

        /// True when the name is a valid data-* extension attribute.
        static bool IsDataAttribute(std::string_view name);

        std::size_t GetElementCount() const { return elementCount; }
        std::size_t GetAttributeCount() const { return attributeRecordCount; }

    private:
        Registry();

        struct ElementKey
        {
            MarkupNamespace ns;
            std::string name;
        };

        struct AttributeKey
        {
            MarkupNamespace ns;
            std::string name;
        };

        struct ElementKeyHash
        {
            std::size_t operator()(const ElementKey& key) const;
        };
        struct ElementKeyEqual
        {
            bool operator()(const ElementKey& left, const ElementKey& right) const;
        };
        struct AttributeKeyHash
        {
            std::size_t operator()(const AttributeKey& key) const;
        };
        struct AttributeKeyEqual
        {
            bool operator()(const AttributeKey& left, const AttributeKey& right) const;
        };

        std::unordered_map<ElementKey, ElementDefinition, ElementKeyHash, ElementKeyEqual> elements;
        std::unordered_map<AttributeKey, std::vector<AttributeDefinition>, AttributeKeyHash, AttributeKeyEqual> attributes;
        std::unordered_map<std::string, Generated::InputTypeRecord> inputTypes;

        std::size_t elementCount = 0;
        std::size_t attributeRecordCount = 0;
    };
}
