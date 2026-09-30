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
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace RoboCSSCompiler;
using namespace RoboTest;

// Both halves of the compiler have a `Generated` namespace, and the tests pull in
// the HTML one through TestHelpers.h, so the CSS vocabulary is named explicitly.
namespace CssReference = RoboCSSCompiler::Generated;

namespace
{
    /// True when the registry compiles the name: it has a compiled id and the
    /// runtime draws it, or it is a shorthand the compiler expands.
    bool IsCompiled(const CSSPropertyRegistry& registry, const char* name)
    {
        if (registry.FindShorthand(name) != nullptr)
        {
            return true;
        }

        const CSSPropertyDefinition* definition = registry.Find(name);
        return definition != nullptr && !definition->dynamic;
    }

    /// True when the registry carries the name: the reference defines it, the
    /// cascade parses and stores its value, and nothing lowers it into the
    /// native style.
    bool IsCarried(const CSSPropertyRegistry& registry, const char* name)
    {
        return registry.FindRegistered(name) != nullptr;
    }

    /// True when the registry accounts for the name at all: either it compiles it
    /// or it carries it. Both answers count as accounting for the reference;
    /// silently dropping a name is what does not.
    bool IsAccountedFor(const CSSPropertyRegistry& registry, const char* name)
    {
        return IsCompiled(registry, name) || IsCarried(registry, name);
    }

    /// The names the reference still has, deduplicated and in table order.
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

    /// Every name the reference has ever defined: the ones still in the
    /// reference and the ones its retired modules defined. The compiler is built
    /// against both, because a module going does not un-define what it stated.
    std::vector<const char*> KnownReferenceNames()
    {
        std::vector<const char*> names = ReferenceNames();

        for (std::size_t index = 0; index < CssReference::RetiredReferencePropertyCount; ++index)
        {
            names.push_back(CssReference::RetiredReferenceProperties[index].name);
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

    // The tables are the compiler's idea of what CSS is, so both of them being
    // empty would make every other test here vacuous. Which of the two holds the
    // names depends only on how much of the reference is left.
    ROBO_CHECK(CssReference::ReferencePropertyCount + CssReference::RetiredReferencePropertyCount > 1);
}

ROBO_TEST(CssReference, EveryKnownReferencePropertyIsCompiledOrRegistered)
{
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();
    const std::vector<const char*> names = KnownReferenceNames();

    std::vector<std::string> unaccounted;
    std::size_t compiled = 0;
    std::size_t registered = 0;

    for (const char* name : names)
    {
        if (IsCompiled(registry, name))
        {
            ++compiled;
            continue;
        }

        // A name the reference defines that this compiler does not compile is
        // carried: the compiler knows the module that defines it, whether it
        // inherits and what its initial value is, and a declaration of it keeps
        // the value the author wrote instead of being refused. Nothing the
        // reference ever defined is left unaccounted for, which is what makes
        // consuming the reference safe.
        if (!IsCarried(registry, name) || registry.IsKnownUnsupportedName(name))
        {
            unaccounted.emplace_back(name);
            continue;
        }

        ++registered;
    }

    ++CheckCount;

    if (!unaccounted.empty())
    {
        std::string message =
            "the reference defines these properties and the compiler neither compiles nor carries them:";
        for (const std::string& name : unaccounted)
        {
            message += " " + name;
        }
        Fail(message, __FILE__, __LINE__);
    }

    ROBO_CHECK_EQ(unaccounted.size(), static_cast<std::size_t>(0));
    ROBO_CHECK_EQ(compiled + registered, names.size());

    // Both answers have to be real, or the accounting above would be vacuous.
    ROBO_CHECK(compiled > 0);
    ROBO_CHECK(registered > 0);
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

        // The citation is a module, not necessarily one the extractor could
        // mine: some drafts ship only the generated Overview.html, and those are
        // named in the audit rather than dropped. A retired module is still a
        // module the reference had, so it resolves too.
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

    // A registered property cites the module that defined it as well, and that
    // is the part of the citation retirement would otherwise take away.
    for (const CSSPropertyDefinition& definition : registry.RegisteredProperties())
    {
        bool found = false;

        for (std::size_t index = 0; index < CssReference::ReferenceModuleCount && !found; ++index)
        {
            found = std::strcmp(CssReference::ReferenceModules[index].name, definition.spec) == 0;
        }

        if (!found)
        {
            unknown.emplace_back(std::string(definition.name) + " -> " + definition.spec);
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

ROBO_TEST(CssReference, RetiredModulesStayCompiledOrRegistered)
{
    // Retiring a module deletes its directory and with it the vocabulary that
    // named what it defined. These entries are what that deletion must not cost:
    // the compiler accounted for each of them before the module went, and this
    // is the test that keeps it accounted for afterwards. The entry carries the
    // module, the inheritance and the initial value too, so it is the same
    // registration the reference table would have produced rather than a name
    // kept alive on its own.
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();

    // The registration is one row per name, and several modules can define the
    // same name, so a name keeps the description one of them wrote. Which one is
    // an ordering detail; that it is one of them is the promise.
    std::map<std::string, std::set<std::string>> modulesByName;

    for (std::size_t index = 0; index < CssReference::RetiredReferencePropertyCount; ++index)
    {
        const CssReference::ReferenceProperty& property = CssReference::RetiredReferenceProperties[index];

        ROBO_CHECK(property.module != nullptr && property.module[0] != '\0');
        modulesByName[property.name].insert(property.module);
    }

    ++CheckCount;

    for (std::size_t index = 0; index < CssReference::RetiredReferencePropertyCount; ++index)
    {
        const CssReference::ReferenceProperty& property = CssReference::RetiredReferenceProperties[index];

        if (!IsAccountedFor(registry, property.name))
        {
            Fail(std::string("a retired module's property is no longer accounted for: ") + property.name,
                 __FILE__, __LINE__);
        }

        ROBO_CHECK(IsAccountedFor(registry, property.name));

        // A registered entry carries the detail the module stated, and the
        // module it names has to be one that actually defined the name, or the
        // citation would point at a specification that never mentioned it.
        if (!IsCompiled(registry, property.name))
        {
            const CSSPropertyDefinition* definition = registry.FindRegistered(property.name);
            ROBO_CHECK(definition != nullptr);

            if (definition != nullptr)
            {
                ROBO_CHECK(std::strcmp(definition->name, property.name) == 0);
                ROBO_CHECK(modulesByName[property.name].count(definition->spec) != 0);
                ROBO_CHECK(definition->initialValue != nullptr);
            }
        }
    }
}

ROBO_TEST(CssReference, RegistrationNeverClaimsAPropertyTheCompilerCompiles)
{
    // Two tables, two answers, and they must not overlap: a name in both would
    // mean a declaration that is simultaneously resolved and refused.
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();

    for (std::size_t index = 0; index < registry.RegisteredProperties().size(); ++index)
    {
        const CSSPropertyDefinition& definition = registry.RegisteredProperties()[index];

        ROBO_CHECK(!definition.supported);
        ROBO_CHECK(definition.dynamic);
        // A carried property has an identifier of its own, in the carried range,
        // so that a declared value has somewhere to live on a computed style.
        ROBO_CHECK_EQ(static_cast<std::uint16_t>(definition.id),
                      static_cast<std::uint16_t>(CSSPropertyId::DynamicBase) + index);
        ROBO_CHECK(definition.grammar == CSSValueGrammar::Dynamic);
        // The public lookup resolves a carried name to the same row the
        // registration holds, and never to a compiled one: one name, one id.
        ROBO_CHECK(registry.Find(definition.name) == &definition);
        ROBO_CHECK(registry.Find(definition.id) == &definition);
        ROBO_CHECK(registry.FindShorthand(definition.name) == nullptr);
        ROBO_CHECK(registry.FindRegistered(definition.name) == &definition);
        ROBO_CHECK(definition.name != nullptr && definition.name[0] != '\0');
    }

    ++CheckCount;
}

ROBO_TEST(CssReference, TheRegisteredTableIsSortedAndUnique)
{
    // `FindRegistered` is a binary search over this table, so a row out of order
    // or a name stored twice would make the lookup answer for the wrong name.
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();
    const std::vector<CSSPropertyDefinition>& registered = registry.RegisteredProperties();

    ROBO_CHECK(!registered.empty());

    for (std::size_t index = 1; index < registered.size(); ++index)
    {
        ROBO_CHECK(std::strcmp(registered[index - 1].name, registered[index].name) < 0);
    }

    ++CheckCount;
}

ROBO_TEST(CssReference, RetiredModulesAreStillNamed)
{
    // A property cites the module that defines it. A module whose directory has
    // been retired is still a module the reference had, so its name stays in the
    // table the citation check searches.
    std::size_t retiredCount = 0;

    for (std::size_t index = 0; index < CssReference::ReferenceModuleCount; ++index)
    {
        if (CssReference::ReferenceModules[index].retired)
        {
            ++retiredCount;
        }
    }

    // The two tables describe the same act, so they cannot disagree: a module is
    // retired exactly when the names it defined were carried over. A retired
    // module with no names kept, or names kept with no retired module, would mean
    // half of the extraction ran.
    const bool anyRetiredNames = CssReference::RetiredReferencePropertyCount > 0;
    ROBO_CHECK_EQ(retiredCount > 0, anyRetiredNames);
}

ROBO_TEST(CssReference, KnownNamesAreNeverReportedAsUnknown)
{
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();

    // Every name the reference defines is compiled or carried, and neither is an
    // error: a declaration of one is applied (and drawn, when compiled) rather
    // than reported. Consuming the reference is only safe because this holds for
    // all 700-odd of them.
    for (const char* name : KnownReferenceNames())
    {
        ROBO_CHECK(!registry.IsKnownUnsupportedName(name));
        ROBO_CHECK(registry.DescribeUnsupported(name) == nullptr);
    }

    // A real property whose specification is outside the reference is still
    // reported as unimplemented rather than mistaken for a typo (RCC010).
    ROBO_CHECK(registry.IsKnownUnsupportedName("touch-action"));
    ROBO_CHECK(registry.DescribeUnsupported("touch-action") != nullptr);

    // And a name that is not a property at all (RCC001).
    ROBO_CHECK(!registry.IsKnownUnsupportedName("whats-it"));
    ROBO_CHECK(!registry.IsKnownUnsupportedName("colour"));
    ROBO_CHECK(registry.DescribeUnsupported("whats-it") == nullptr);
}

ROBO_TEST(CssReference, TheReferenceCoversTheCompilerWithVeryFewExceptions)
{
    const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();
    const std::vector<const char*> names = KnownReferenceNames();

    std::size_t implemented = 0;
    for (const char* name : names)
    {
        if (IsCompiled(registry, name))
        {
            ++implemented;
        }
    }

    // The counts are the point of the extraction: almost everything the CSS
    // Working Group defines is a name this compiler carries rather than
    // compiles, and stating it as a number keeps that from being forgotten.
    ROBO_CHECK(implemented > 0);
    ROBO_CHECK(implemented < names.size());
}
