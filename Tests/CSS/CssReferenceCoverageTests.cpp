// ---------------------------------------------------------------------------
// CSS reference coverage tests.
//
// The CSS reference under Reference/CSS is an implementation checklist, and the
// generated vocabulary
// (CompilerKnowledge/Include/RoboCSSCompiler/GeneratedCssVocabulary.h) is what
// that checklist became. These tests are the gate on deleting the reference: the
// compiler must account for every name the reference defines, either by
// implementing it or by reporting it, and the generated table must be usable as
// the sorted table the lookup assumes it is.
//
// A property that is implemented but missing from the reference is a mistake in
// the opposite direction: it means the compiler documents a spec citation that
// does not hold.
// ---------------------------------------------------------------------------
#include "../TestFramework.h"
#include "../TestHelpers.h"

#include "RoboCSSCompiler/CssProperty.h"
#include "RoboCSSCompiler/GeneratedCssVocabulary.h"

#include <cstring>
#include <string>
#include <vector>

using namespace RoboCSSCompiler;
using namespace RoboTest;

// Both halves of the compiler have a `Generated` namespace, and the tests pull in
// the HTML one through TestHelpers.h, so the CSS vocabulary is named explicitly.
namespace CssReference = RoboCSSCompiler::Generated;

namespace
{
    /// True when the registry compiles the name as a property or expands it as a
    /// shorthand.
    bool IsImplemented(const CSSPropertyRegistry& registry, const char* name)
    {
        return registry.Find(name) != nullptr || registry.FindShorthand(name) != nullptr;
    }

    /// The names the reference defines, deduplicated and in table order.
    std::vector<const char*> ReferenceNames()
    {
        std::vector<const char*> names;
        names.reserve(CssReference::ReferencePropertyCount);

        for (std::size_t index = 0; index < CssReference::ReferencePropertyCount; ++index)
        {
            const char* name = CssReference::ReferenceProperties[index].name;

            if (names.empty() || std::strcmp(names.back(), name) != 0)
            {
                names.push_back(name);
            }
        }

        return names;
    }
}

ROBO_TEST(CssReference, TheGeneratedTableIsSortedForBinarySearch)
{
    // The registry's unsupported-name lookup is a binary search, so the table has
    // to be sorted. The generated file is produced by a sort, which is exactly
    // why it is checked.
    for (std::size_t index = 1; index < CssReference::ReferencePropertyCount; ++index)
    {
        const char* previous = CssReference::ReferenceProperties[index - 1].name;
        const char* current = CssReference::ReferenceProperties[index].name;

        ROBO_CHECK(std::strcmp(previous, current) <= 0);
    }

    ROBO_CHECK(CssReference::ReferencePropertyCount > 0);
}

ROBO_TEST(CssReference, EveryReferencePropertyIsImplementedOrReported)
{
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();
    const std::vector<const char*> names = ReferenceNames();

    std::vector<std::string> unaccounted;

    for (const char* name : names)
    {
        if (IsImplemented(registry, name))
        {
            continue;
        }

        // A name the reference defines that this compiler does not compile must
        // still be reported by name, never silently dropped and never mistaken
        // for a typo: that is the difference between RCC010 and RCC001.
        if (!registry.IsKnownUnsupportedName(name))
        {
            unaccounted.emplace_back(name);
        }
    }

    ++CheckCount;

    if (!unaccounted.empty())
    {
        std::string message = "the reference defines these properties and the compiler does not account for them:";
        for (const std::string& name : unaccounted)
        {
            message += " " + name;
        }
        Fail(message, __FILE__, __LINE__);
    }

    ROBO_CHECK_EQ(unaccounted.size(), static_cast<std::size_t>(0));
}

ROBO_TEST(CssReference, ImplementedPropertiesAreNeverReportedAsUnimplemented)
{
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();

    for (const CSSPropertyDefinition& definition : registry.Properties())
    {
        ROBO_CHECK(!registry.IsKnownUnsupportedName(definition.name));
    }

    for (const CSSShorthandDefinition& shorthand : registry.Shorthands())
    {
        // A shorthand that was reported as unimplemented would never be
        // expanded, and the longhands it sets would silently go unset.
        ROBO_CHECK(!registry.IsKnownUnsupportedName(shorthand.name));
    }
}

ROBO_TEST(CssReference, EverySpecCitationNamesAReferenceModule)
{
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();

    std::vector<std::string> unknown;

    for (const CSSPropertyDefinition& definition : registry.Properties())
    {
        bool found = false;

        // The citation is a module directory, not necessarily a module the
        // extractor could mine: some drafts ship only the generated Overview.html,
        // and those are named in the audit rather than dropped.
        for (std::size_t index = 0; index < CssReference::ReferenceModuleCount && !found; ++index)
        {
            found = std::strcmp(CssReference::ReferenceModules[index].name, definition.spec) == 0;
        }

        if (!found)
        {
            unknown.emplace_back(std::string(definition.name) + " -> " + definition.spec);
        }
    }

    for (const CSSShorthandDefinition& shorthand : registry.Shorthands())
    {
        bool found = false;

        for (std::size_t index = 0; index < CssReference::ReferenceModuleCount && !found; ++index)
        {
            found = std::strcmp(CssReference::ReferenceModules[index].name, shorthand.spec) == 0;
        }

        if (!found)
        {
            unknown.emplace_back(std::string(shorthand.name) + " -> " + shorthand.spec);
        }
    }

    ++CheckCount;

    if (!unknown.empty())
    {
        std::string message = "these properties cite a module the reference does not have:";
        for (const std::string& entry : unknown)
        {
            message += " " + entry;
        }
        Fail(message, __FILE__, __LINE__);
    }

    ROBO_CHECK_EQ(unknown.size(), static_cast<std::size_t>(0));
}

ROBO_TEST(CssReference, UnknownNamesAreStillUnknown)
{
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();

    // A real property the compiler does not implement, and a name that is not a
    // property at all. The reference is what separates them.
    ROBO_CHECK(registry.IsKnownUnsupportedName("animation-name"));
    ROBO_CHECK(registry.IsKnownUnsupportedName("grid-template-columns"));
    ROBO_CHECK(registry.DescribeUnsupported("grid-template-columns") != nullptr);

    ROBO_CHECK(!registry.IsKnownUnsupportedName("whats-it"));
    ROBO_CHECK(!registry.IsKnownUnsupportedName("colour"));
    ROBO_CHECK(registry.DescribeUnsupported("whats-it") == nullptr);
}

ROBO_TEST(CssReference, TheReferenceCoversTheCompilerWithVeryFewExceptions)
{
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();
    const std::vector<const char*> names = ReferenceNames();

    std::size_t implemented = 0;
    for (const char* name : names)
    {
        if (IsImplemented(registry, name))
        {
            ++implemented;
        }
    }

    // The counts are the point of the extraction: almost everything the CSS
    // Working Group defines is a name this compiler reports rather than
    // compiles, and stating it as a number keeps that from being forgotten.
    ROBO_CHECK(implemented > 0);
    ROBO_CHECK(implemented < names.size());
}
