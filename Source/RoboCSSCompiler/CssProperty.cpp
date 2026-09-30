#include "RoboCSSCompiler/CssProperty.h"

#include "RoboCSSCompiler/GeneratedCssVocabulary.h"

#include <cstring>

namespace RoboCSSCompiler
{
    namespace
    {
        /// The names of properties this compiler reports as unimplemented that
        /// the reference checkout under Reference/CSS does not define, because
        /// they belong to specifications outside it: Pointer Events defines
        /// `touch-action`, MathML Core the `math-*` trio, and `font-stretch` was
        /// renamed to `font-width` by css-fonts-4. Everything else comes from the
        /// generated vocabulary, so this list cannot quietly drift from the
        /// reference the way a long hand-written one does.
        const char* const UnsupportedNamesOutsideTheReference[] = {
            "font-stretch",
            "math-depth",
            "math-shift",
            "math-style",
            "touch-action",
        };

        /// The reference's property table is sorted by name, so this is a binary
        /// search and not a scan: the table has a definition per (module,
        /// property) pair.
        const Generated::ReferenceProperty* FindReferenceProperty(const std::string& name)
        {
            std::size_t low = 0;
            std::size_t high = Generated::ReferencePropertyCount;

            while (low < high)
            {
                const std::size_t middle = low + (high - low) / 2;
                const int order = name.compare(Generated::ReferenceProperties[middle].name);

                if (order == 0)
                {
                    return &Generated::ReferenceProperties[middle];
                }

                if (order < 0)
                {
                    high = middle;
                }
                else
                {
                    low = middle + 1;
                }
            }

            return nullptr;
        }

        const char* FindNameOutsideTheReference(const std::string& name)
        {
            for (const char* candidate : UnsupportedNamesOutsideTheReference)
            {
                if (name == candidate)
                {
                    return candidate;
                }
            }

            return nullptr;
        }
    }

    const char* ToString(CSSPropertyId id)
    {
        const CSSPropertyDefinition* definition = CSSPropertyRegistry::Instance().Find(id);
        return definition != nullptr ? definition->name : "unknown";
    }

    const char* ToString(CSSPropertyCategory category)
    {
        switch (category)
        {
        case CSSPropertyCategory::Unknown: return "unknown";
        case CSSPropertyCategory::BoxModel: return "box-model";
        case CSSPropertyCategory::Border: return "border";
        case CSSPropertyCategory::Background: return "background";
        case CSSPropertyCategory::Color: return "color";
        case CSSPropertyCategory::Font: return "font";
        case CSSPropertyCategory::Text: return "text";
        case CSSPropertyCategory::Overflow: return "overflow";
        case CSSPropertyCategory::Display: return "display";
        case CSSPropertyCategory::Layout: return "layout";
        case CSSPropertyCategory::Position: return "position";
        case CSSPropertyCategory::Transform: return "transform";
        case CSSPropertyCategory::Animation: return "animation";
        case CSSPropertyCategory::Transition: return "transition";
        case CSSPropertyCategory::Filter: return "filter";
        case CSSPropertyCategory::List: return "list";
        case CSSPropertyCategory::Table: return "table";
        case CSSPropertyCategory::Content: return "content";
        case CSSPropertyCategory::Interaction: return "interaction";
        case CSSPropertyCategory::Logical: return "logical";
        case CSSPropertyCategory::Cascade: return "cascade";
        }

        return "unknown";
    }

    namespace
    {
        using Id = CSSPropertyId;
        using Cat = CSSPropertyCategory;
        using Grammar = CSSValueGrammar;

        /// One row of the registry. Written as an aggregate so that a new
        /// property is one line and its metadata sits beside its name.
        struct Row
        {
            Id id;
            const char* name;
            Cat category;
            bool inherited;
            const char* initialValue;
            Grammar grammar;
            const char* keywords;
            bool affectsLayout;
            bool affectsPaint;
            bool affectsText;
            bool affectsHitTesting;
            bool supported;
            const char* spec;
        };

        // (id, name, category, inherited, initial, grammar, keywords,
        //  layout, paint, text, hit, supported, spec)
        const Row PropertyRows[] = {
            // `all` is a shorthand over every property and takes only the
            // CSS-wide keywords, which is why its keyword set is exactly those.
            // The cascade expands it; nothing else has to know about it.
            { Id::All, "all", Cat::Cascade, false, "initial", Grammar::Keyword,
              "initial inherit unset revert revert-layer", true, true, true, true, true, "css-cascade-3" },

            { Id::Display, "display", Cat::Display, false, "inline", Grammar::Display, nullptr, true, true, false, true, true, "css-display-3" },

            { Id::Width, "width", Cat::BoxModel, false, "auto", Grammar::Size, nullptr, true, true, true, true, true, "css-sizing-3" },
            { Id::Height, "height", Cat::BoxModel, false, "auto", Grammar::Size, nullptr, true, true, true, true, true, "css-sizing-3" },
            { Id::MinWidth, "min-width", Cat::BoxModel, false, "0", Grammar::Size, nullptr, true, true, true, true, true, "css-sizing-3" },
            { Id::MinHeight, "min-height", Cat::BoxModel, false, "0", Grammar::Size, nullptr, true, true, true, true, true, "css-sizing-3" },
            { Id::MaxWidth, "max-width", Cat::BoxModel, false, "none", Grammar::Size, nullptr, true, true, true, true, true, "css-sizing-3" },
            { Id::MaxHeight, "max-height", Cat::BoxModel, false, "none", Grammar::Size, nullptr, true, true, true, true, true, "css-sizing-3" },

            { Id::MarginTop, "margin-top", Cat::BoxModel, false, "0", Grammar::LengthPercentageOrAuto, nullptr, true, true, false, true, true, "css-box-3" },
            { Id::MarginRight, "margin-right", Cat::BoxModel, false, "0", Grammar::LengthPercentageOrAuto, nullptr, true, true, false, true, true, "css-box-3" },
            { Id::MarginBottom, "margin-bottom", Cat::BoxModel, false, "0", Grammar::LengthPercentageOrAuto, nullptr, true, true, false, true, true, "css-box-3" },
            { Id::MarginLeft, "margin-left", Cat::BoxModel, false, "0", Grammar::LengthPercentageOrAuto, nullptr, true, true, false, true, true, "css-box-3" },

            { Id::PaddingTop, "padding-top", Cat::BoxModel, false, "0", Grammar::LengthPercentage, nullptr, true, true, false, true, true, "css-box-3" },
            { Id::PaddingRight, "padding-right", Cat::BoxModel, false, "0", Grammar::LengthPercentage, nullptr, true, true, false, true, true, "css-box-3" },
            { Id::PaddingBottom, "padding-bottom", Cat::BoxModel, false, "0", Grammar::LengthPercentage, nullptr, true, true, false, true, true, "css-box-3" },
            { Id::PaddingLeft, "padding-left", Cat::BoxModel, false, "0", Grammar::LengthPercentage, nullptr, true, true, false, true, true, "css-box-3" },

            { Id::BorderTopWidth, "border-top-width", Cat::Border, false, "medium", Grammar::BorderWidth, nullptr, true, true, false, true, true, "css-backgrounds-3" },
            { Id::BorderRightWidth, "border-right-width", Cat::Border, false, "medium", Grammar::BorderWidth, nullptr, true, true, false, true, true, "css-backgrounds-3" },
            { Id::BorderBottomWidth, "border-bottom-width", Cat::Border, false, "medium", Grammar::BorderWidth, nullptr, true, true, false, true, true, "css-backgrounds-3" },
            { Id::BorderLeftWidth, "border-left-width", Cat::Border, false, "medium", Grammar::BorderWidth, nullptr, true, true, false, true, true, "css-backgrounds-3" },

            { Id::BorderTopStyle, "border-top-style", Cat::Border, false, "none", Grammar::BorderStyle, nullptr, true, true, false, false, true, "css-backgrounds-3" },
            { Id::BorderRightStyle, "border-right-style", Cat::Border, false, "none", Grammar::BorderStyle, nullptr, true, true, false, false, true, "css-backgrounds-3" },
            { Id::BorderBottomStyle, "border-bottom-style", Cat::Border, false, "none", Grammar::BorderStyle, nullptr, true, true, false, false, true, "css-backgrounds-3" },
            { Id::BorderLeftStyle, "border-left-style", Cat::Border, false, "none", Grammar::BorderStyle, nullptr, true, true, false, false, true, "css-backgrounds-3" },

            { Id::BorderTopColor, "border-top-color", Cat::Border, false, "currentcolor", Grammar::Color, nullptr, false, true, false, false, true, "css-backgrounds-3" },
            { Id::BorderRightColor, "border-right-color", Cat::Border, false, "currentcolor", Grammar::Color, nullptr, false, true, false, false, true, "css-backgrounds-3" },
            { Id::BorderBottomColor, "border-bottom-color", Cat::Border, false, "currentcolor", Grammar::Color, nullptr, false, true, false, false, true, "css-backgrounds-3" },
            { Id::BorderLeftColor, "border-left-color", Cat::Border, false, "currentcolor", Grammar::Color, nullptr, false, true, false, false, true, "css-backgrounds-3" },

            { Id::BorderRadius, "border-radius", Cat::Border, false, "0", Grammar::BorderRadius, nullptr, true, true, false, true, true, "css-backgrounds-3" },

            // The initial value of `color` is CanvasText, a system color. A
            // native runtime has one theme, and the compiler does not invent a
            // palette, so a system color is reported as unsupported; the initial
            // value therefore has to be one the value parser can actually
            // produce. Black is CSS 2.1's initial value, which is what this
            // runtime's default text color is, so `color: initial` means what
            // it means everywhere else.
            { Id::Color, "color", Cat::Color, true, "black", Grammar::Color, nullptr, false, true, true, false, true, "css-color-4" },
            { Id::BackgroundColor, "background-color", Cat::Background, false, "transparent", Grammar::Color, nullptr, false, true, false, false, true, "css-backgrounds-3" },

            { Id::FontSize, "font-size", Cat::Font, true, "medium", Grammar::FontSize, nullptr, true, true, true, true, true, "css-fonts-3" },
            { Id::FontWeight, "font-weight", Cat::Font, true, "normal", Grammar::FontWeight, nullptr, true, true, true, true, true, "css-fonts-3" },
            { Id::LineHeight, "line-height", Cat::Font, true, "normal", Grammar::LineHeight, nullptr, true, true, true, true, true, "css-inline-3" },
            { Id::LetterSpacing, "letter-spacing", Cat::Text, true, "normal", Grammar::LengthOrNormal, nullptr, true, true, true, true, true, "css-text-3" },
            { Id::TextAlign, "text-align", Cat::Text, true, "start", Grammar::TextAlign, nullptr, false, true, true, false, true, "css-text-3" },

            { Id::OverflowX, "overflow-x", Cat::Overflow, false, "visible", Grammar::Overflow, nullptr, true, true, false, true, true, "css-overflow-3" },
            { Id::OverflowY, "overflow-y", Cat::Overflow, false, "visible", Grammar::Overflow, nullptr, true, true, false, true, true, "css-overflow-3" }
        };

        /// True when the name is a property the reference defines that this
        /// compiler does not implement, or one of the few names whose
        /// specification lives outside the reference checkout. Neither is a
        /// typo, and neither is silently ignored: both are reported by name.
        bool IsReferenceOnlyName(const std::string& name)
        {
            return FindReferenceProperty(name) != nullptr || FindNameOutsideTheReference(name) != nullptr;
        }

    }

    CSSPropertyRegistry::CSSPropertyRegistry()
    {
        properties.reserve(sizeof(PropertyRows) / sizeof(PropertyRows[0]));

        for (const Row& row : PropertyRows)
        {
            CSSPropertyDefinition definition{};
            definition.id = row.id;
            definition.name = row.name;
            definition.category = row.category;
            definition.inherited = row.inherited;
            definition.initialValue = row.initialValue;
            definition.grammar = row.grammar;
            definition.keywords = row.keywords;
            definition.affectsLayout = row.affectsLayout;
            definition.affectsPaint = row.affectsPaint;
            definition.affectsText = row.affectsText;
            definition.affectsHitTesting = row.affectsHitTesting;
            definition.supported = row.supported;
            definition.spec = row.spec;

            properties.push_back(definition);

            if (row.inherited)
            {
                inheritedProperties.push_back(row.id);
            }
        }

        // Box shorthands: four longhands, expanded from one to four values.
        const auto box = [](Id top, Id right, Id bottom, Id left) {
            CSSShorthandTargets targets;
            targets.top = top;
            targets.right = right;
            targets.bottom = bottom;
            targets.left = left;
            targets.count = 4;
            return targets;
        };

        const auto pair = [](Id first, Id second) {
            CSSShorthandTargets targets;
            targets.top = first;
            targets.right = second;
            targets.count = 2;
            return targets;
        };

        const auto single = [](Id only) {
            CSSShorthandTargets targets;
            targets.top = only;
            targets.count = 1;
            return targets;
        };

        shorthands.push_back({ "margin", Cat::BoxModel,
                               box(Id::MarginTop, Id::MarginRight, Id::MarginBottom, Id::MarginLeft), "box", true,
                               "css-box-3" });
        shorthands.push_back({ "padding", Cat::BoxModel,
                               box(Id::PaddingTop, Id::PaddingRight, Id::PaddingBottom, Id::PaddingLeft), "box", true,
                               "css-box-3" });
        shorthands.push_back({ "border-width", Cat::Border,
                               box(Id::BorderTopWidth, Id::BorderRightWidth, Id::BorderBottomWidth,
                                   Id::BorderLeftWidth),
                               "box", true, "css-backgrounds-3" });
        shorthands.push_back({ "border-style", Cat::Border,
                               box(Id::BorderTopStyle, Id::BorderRightStyle, Id::BorderBottomStyle,
                                   Id::BorderLeftStyle),
                               "box", true, "css-backgrounds-3" });
        shorthands.push_back({ "border-color", Cat::Border,
                               box(Id::BorderTopColor, Id::BorderRightColor, Id::BorderBottomColor,
                                   Id::BorderLeftColor),
                               "box", true, "css-backgrounds-3" });
        shorthands.push_back({ "border-radius", Cat::Border, single(Id::BorderRadius), "radius", true,
                               "css-backgrounds-3" });
        shorthands.push_back({ "border", Cat::Border,
                               box(Id::BorderTopWidth, Id::BorderRightWidth, Id::BorderBottomWidth,
                                   Id::BorderLeftWidth),
                               "border", true, "css-backgrounds-3" });
        shorthands.push_back({ "border-top", Cat::Border, single(Id::BorderTopWidth), "border-side", true,
                               "css-backgrounds-3" });
        shorthands.push_back({ "border-right", Cat::Border, single(Id::BorderRightWidth), "border-side", true,
                               "css-backgrounds-3" });
        shorthands.push_back({ "border-bottom", Cat::Border, single(Id::BorderBottomWidth), "border-side", true,
                               "css-backgrounds-3" });
        shorthands.push_back({ "border-left", Cat::Border, single(Id::BorderLeftWidth), "border-side", true,
                               "css-backgrounds-3" });
        shorthands.push_back({ "overflow", Cat::Overflow, pair(Id::OverflowX, Id::OverflowY), "pair", true,
                               "css-overflow-3" });
        shorthands.push_back({ "background", Cat::Background, single(Id::BackgroundColor), "background", true,
                               "css-backgrounds-3" });
        shorthands.push_back({ "font", Cat::Font, single(Id::FontSize), "font", true, "css-fonts-3" });

        // Everything the reference defines and this compiler does not compile:
        // derived from the generated vocabulary rather than listed, so a module
        // that adds a property adds it to the report the same day.
        for (std::size_t index = 0; index < Generated::ReferencePropertyCount; ++index)
        {
            const Generated::ReferenceProperty& property = Generated::ReferenceProperties[index];

            if (Find(property.name) == nullptr && FindShorthand(property.name) == nullptr)
            {
                unsupportedNames.push_back(property.name);
            }
        }

        for (const char* name : UnsupportedNamesOutsideTheReference)
        {
            unsupportedNames.push_back(name);
        }
    }

    const CSSPropertyRegistry& CSSPropertyRegistry::Instance()
    {
        static const CSSPropertyRegistry registry;
        return registry;
    }

    const CSSPropertyDefinition* CSSPropertyRegistry::Find(const std::string& name) const
    {
        for (const CSSPropertyDefinition& definition : properties)
        {
            if (name.size() == std::strlen(definition.name) && name == definition.name)
            {
                return &definition;
            }
        }

        return nullptr;
    }

    const CSSPropertyDefinition* CSSPropertyRegistry::Find(CSSPropertyId id) const
    {
        for (const CSSPropertyDefinition& definition : properties)
        {
            if (definition.id == id)
            {
                return &definition;
            }
        }

        return nullptr;
    }

    const CSSShorthandDefinition* CSSPropertyRegistry::FindShorthand(const std::string& name) const
    {
        for (const CSSShorthandDefinition& definition : shorthands)
        {
            if (name.size() == std::strlen(definition.name) && name == definition.name)
            {
                return &definition;
            }
        }

        return nullptr;
    }

    const char* CSSPropertyRegistry::SuggestSimilarName(const std::string& name) const
    {
        // Only near misses: a reader who wrote `colour` wants `color`, and a
        // reader who wrote `grid` wants a different answer than a guess.
        const auto distanceWithin = [](const std::string& left, const std::string& right, std::size_t limit) {
            if (left.size() > right.size() + limit || right.size() > left.size() + limit)
            {
                return limit + 1;
            }

            std::vector<std::size_t> previous(right.size() + 1);
            std::vector<std::size_t> current(right.size() + 1);

            for (std::size_t index = 0; index <= right.size(); ++index)
            {
                previous[index] = index;
            }

            for (std::size_t row = 1; row <= left.size(); ++row)
            {
                current[0] = row;
                std::size_t rowMinimum = current[0];

                for (std::size_t column = 1; column <= right.size(); ++column)
                {
                    const std::size_t substitution =
                        previous[column - 1] + (left[row - 1] == right[column - 1] ? 0 : 1);
                    const std::size_t deletion = previous[column] + 1;
                    const std::size_t insertion = current[column - 1] + 1;
                    current[column] = std::min(substitution, std::min(deletion, insertion));
                    rowMinimum = std::min(rowMinimum, current[column]);
                }

                if (rowMinimum > limit)
                {
                    return limit + 1;
                }

                previous.swap(current);
            }

            return previous[right.size()];
        };

        const char* best = nullptr;
        std::size_t bestDistance = 3;

        for (const CSSPropertyDefinition& definition : Properties())
        {
            const std::size_t distance = distanceWithin(name, definition.name, 2);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = definition.name;
            }
        }

        for (const CSSShorthandDefinition& shorthand : Shorthands())
        {
            const std::size_t distance = distanceWithin(name, shorthand.name, 2);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = shorthand.name;
            }
        }

        return best;
    }

    bool CSSPropertyRegistry::IsKnownUnsupportedName(const std::string& name) const
    {
        // A property this compiler compiles is never reported as unimplemented,
        // and neither is a shorthand it expands.
        if (Find(name) != nullptr || FindShorthand(name) != nullptr)
        {
            return false;
        }

        return IsReferenceOnlyName(name);
    }

    const char* CSSPropertyRegistry::DescribeUnsupported(const std::string& name) const
    {
        if (Find(name) != nullptr || FindShorthand(name) != nullptr)
        {
            return nullptr;
        }

        // The returned pointer is stable: it is either the generated table's own
        // spelling or a literal, never the argument.
        if (const Generated::ReferenceProperty* property = FindReferenceProperty(name))
        {
            return property->name;
        }

        return FindNameOutsideTheReference(name);
    }
}
