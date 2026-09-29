// ---------------------------------------------------------------------------
// GENERATED FILE - do not edit by hand.
// Produced by the RoboHTMLCompiler RegistryCompiler target.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/GeneratedEnums.h"

#include <cstdint>

namespace RoboHTMLCompiler::Generated
{
    enum class NamespaceId : std::uint8_t
    {
        None = 0,
        Html,
        Svg,
        MathMl
    };

    enum ElementFlags : std::uint32_t
    {
        ElementFlagNone = 0,
        ElementFlagVoid = 1u << 0,
        ElementFlagObsolete = 1u << 1,
        ElementFlagGlobalAttributes = 1u << 2,
        ElementFlagOrdered = 1u << 3,
        ElementFlagRawText = 1u << 4,
        ElementFlagPhrasing = 1u << 5,
        ElementFlagFormAssociated = 1u << 6,
        ElementFlagInteractive = 1u << 7
    };

    enum AttributeFlags : std::uint32_t
    {
        AttributeFlagNone = 0,
        AttributeFlagNamespaceDefault = 1u << 0,
        AttributeFlagGlobal = 1u << 1,
        AttributeFlagEventHandler = 1u << 2,
        AttributeFlagForbidden = 1u << 3,
        AttributeFlagObsolete = 1u << 4,
        AttributeFlagCompileTime = 1u << 5,
        AttributeFlagRuntimeMutable = 1u << 6,
        AttributeFlagBrowserPreserved = 1u << 7,
        AttributeFlagAria = 1u << 8,
        AttributeFlagSvg = 1u << 9,
        AttributeFlagMathMl = 1u << 10
    };

    struct ElementRecord
    {
        const char* name;
        NamespaceId namespaceId;
        ElementKind kind;
        std::uint8_t headingLevel;
        std::uint32_t flags;
        const char* const* attributeNames;
        std::uint32_t attributeNameCount;
        const char* sectionId;
        const char* domInterface;
    };

    struct AttributeRecord
    {
        const char* name;
        NamespaceId namespaceId;
        NativeProperty property;
        std::uint32_t flags;
        const char* valueType;
        const char* category;
        const char* const* allowedElements;
        std::uint32_t allowedElementCount;
    };

    struct InputTypeRecord
    {
        const char* typeValue;
        ElementKind kind;
        std::uint32_t flags;
    };

    extern const ElementRecord ElementTable[];
    extern const std::uint32_t ElementCount;
    extern const AttributeRecord AttributeTable[];
    extern const std::uint32_t AttributeCount;
    extern const InputTypeRecord InputTypeTable[];
    extern const std::uint32_t InputTypeCount;
}
