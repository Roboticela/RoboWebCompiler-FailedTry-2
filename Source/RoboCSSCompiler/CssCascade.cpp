#include "RoboCSSCompiler/CssCascade.h"

#include "RoboCSSCompiler/CssSyntax.h"
#include "RoboCSSCompiler/CssValueParser.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace RoboCSSCompiler
{
    /// The message a mistyped property name gets: the name, and the name the
    /// writer probably meant when one is close. Defined with the coverage report
    /// below and declared here for the cascade that reports it.
    std::string UnknownPropertyMessage(const std::string& name);

    namespace
    {
        const char* UserAgentFileName = "<native-defaults>";

        std::string LowerCase(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](char character) {
                return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            });
            return value;
        }

        std::string Trim(const std::string& value)
        {
            std::size_t begin = 0;
            std::size_t end = value.size();

            while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
            {
                ++begin;
            }

            while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
            {
                --end;
            }

            return value.substr(begin, end - begin);
        }

        /// The significant component values of a list: without the separators and
        /// without the end-of-input marker the parser leaves behind.
        std::vector<CSSComponentValue> SignificantValues(const std::vector<CSSComponentValue>& values)
        {
            std::vector<CSSComponentValue> result;

            for (const CSSComponentValue& value : values)
            {
                if (value.token.type == CSSTokenType::Whitespace || value.token.type == CSSTokenType::Comma ||
                    value.token.type == CSSTokenType::EndOfFile)
                {
                    continue;
                }

                result.push_back(value);
            }

            return result;
        }

        /// The tokens of a value with whitespace, for a shorthand that has to
        /// look at the top-level components one by one.
        std::vector<const CSSComponentValue*> TopLevelItems(const std::vector<CSSComponentValue>& values)
        {
            std::vector<const CSSComponentValue*> items;

            for (const CSSComponentValue& value : values)
            {
                if (value.token.type == CSSTokenType::Whitespace || value.token.type == CSSTokenType::Comma ||
                    value.token.type == CSSTokenType::EndOfFile)
                {
                    continue;
                }

                items.push_back(&value);
            }

            return items;
        }

        /// The one-to-four values of a shorthand that takes single-value
        /// components: `margin: 1rem 2rem` has two, `border-radius: 4px` has one.
        std::vector<std::vector<CSSComponentValue>> ShorthandParts(const std::vector<CSSComponentValue>& values)
        {
            std::vector<std::vector<CSSComponentValue>> parts;

            for (const CSSComponentValue* item : TopLevelItems(values))
            {
                parts.emplace_back(1, *item);
            }

            return parts;
        }

        bool ContainsVarFunction(const std::vector<CSSComponentValue>& values)
        {
            for (const CSSComponentValue& value : values)
            {
                if (value.isFunction && LowerCase(value.token.text) == "var")
                {
                    return true;
                }

                if (ContainsVarFunction(value.children))
                {
                    return true;
                }
            }

            return false;
        }

        // -------------------------------------------------------------------
        // The cascade's ordering
        // -------------------------------------------------------------------

        /// The cascade's origin/importance bucket. Normal declarations rank by
        /// origin, important ones rank by the reverse of it, which is what makes
        /// `!important` in a user stylesheet beat `!important` in a page.
        int OriginRank(CSSOrigin origin, bool important)
        {
            const int position = static_cast<int>(origin);

            return important ? (100 - position) : position;
        }

        // -------------------------------------------------------------------
        // Media and feature queries
        // -------------------------------------------------------------------

        struct MediaEnvironment
        {
            float width = 1024.0f;
            float height = 768.0f;
        };

        /// A media feature comparison: `(min-width: 600px)`.
        enum class MediaComparison
        {
            None = 0,
            LessThan,
            LessThanOrEqual,
            Equal,
            GreaterThanOrEqual,
            GreaterThan
        };

        /// Evaluates one media query, returning false when it does not match.
        /// `decided` is false when the query depends on something the compiler
        /// cannot know, in which case the caller reports it instead of guessing.
        bool EvaluateMediaQuery(const std::string& text,
                                const MediaEnvironment& environment,
                                bool& decided,
                                std::string& undecidedReason)
        {
            decided = true;

            std::vector<std::string> parts;
            std::size_t begin = 0;

            while (begin <= text.size())
            {
                const std::size_t comma = text.find(',', begin);
                parts.push_back(
                    LowerCase(Trim(text.substr(begin, (comma == std::string::npos ? text.size() : comma) - begin))));

                if (comma == std::string::npos)
                {
                    break;
                }

                begin = comma + 1;
            }

            // A comma separated media query list matches when any of its queries
            // does.
            for (const std::string& part : parts)
            {
                if (part.empty())
                {
                    continue;
                }

                bool negation = false;
                std::string body = part;

                if (body.rfind("not ", 0) == 0)
                {
                    negation = true;
                    body = Trim(body.substr(4));
                }
                else if (body.rfind("only ", 0) == 0)
                {
                    body = Trim(body.substr(5));
                }

                // The media type, or the first feature.
                if (body.rfind("all", 0) == 0 && (body.size() == 3 || body[3] == ' '))
                {
                    body = Trim(body.substr(3));
                }
                else if (body.rfind("screen", 0) == 0 && (body.size() == 6 || body[6] == ' '))
                {
                    body = Trim(body.substr(6));
                }
                else if (body.rfind("print", 0) == 0)
                {
                    // The native runtime is a screen.
                    if (negation)
                    {
                        return true;
                    }

                    continue;
                }
                else if (body.empty())
                {
                    if (!negation)
                    {
                        return true;
                    }

                    continue;
                }
                else if (body[0] != '(')
                {
                    // An unknown media type.
                    undecidedReason = "the media type '" + body + "'";
                    decided = false;
                    return false;
                }

                while (!body.empty() && body.back() == ')')
                {
                    body.pop_back();
                }

                bool matches = true;

                while (!body.empty())
                {
                    const std::size_t open = body.find('(');

                    if (open == std::string::npos)
                    {
                        break;
                    }

                    const std::size_t close = body.find(')', open);
                    const std::string inside = Trim(body.substr(open + 1, close - open - 1));

                    const std::size_t colon = inside.find(':');
                    const std::string name = Trim(colon == std::string::npos ? inside : inside.substr(0, colon));
                    const std::string value = colon == std::string::npos ? std::string() : Trim(inside.substr(colon + 1));

                    const auto feature = [&](const char* featureName, float& out) -> bool {
                        if (name == featureName)
                        {
                            out = environment.width;
                            return true;
                        }

                        return false;
                    };

                    (void)feature;

                    if (name == "width" || name == "min-width" || name == "max-width" || name == "height" ||
                        name == "min-height" || name == "max-height")
                    {
                        double number = 0.0;
                        CSSUnit unit = CSSUnit::None;
                        std::string numberText = value;

                        while (!numberText.empty() && !std::isdigit(static_cast<unsigned char>(numberText.back())) &&
                               numberText.back() != '.')
                        {
                            numberText.pop_back();
                        }

                        number = std::atof(numberText.c_str());

                        std::string unitText = Trim(value.substr(numberText.size()));
                        ParseUnit(unitText, unit);

                        if (unit != CSSUnit::None && unit != CSSUnit::Px)
                        {
                            if (UnitFamily(unit) == CSSUnitFamily::Length && IsAbsoluteUnit(unit))
                            {
                                number *= AbsoluteUnitScale(unit);
                            }
                            else
                            {
                                undecidedReason = "the unit '" + unitText + "' in a media feature";
                                decided = false;
                                return false;
                            }
                        }

                        const float context = (name.find("height") != std::string::npos) ? environment.height
                                                                                          : environment.width;

                        if (name == "width" || name == "height")
                        {
                            // A range syntax or a plain value: `(width: 600px)`.
                            matches = matches && static_cast<float>(number) == context;
                        }
                        else if (name.rfind("min-", 0) == 0)
                        {
                            matches = matches && context >= static_cast<float>(number);
                        }
                        else
                        {
                            matches = matches && context <= static_cast<float>(number);
                        }
                    }
                    else if (name == "orientation")
                    {
                        const bool landscape = environment.width >= environment.height;
                        matches = matches && ((value == "landscape") == landscape);
                    }
                    else if (name == "aspect-ratio" || name == "min-aspect-ratio" || name == "max-aspect-ratio" ||
                             name == "resolution" || name == "min-resolution" || name == "max-resolution" ||
                             name == "hover" || name == "any-hover" || name == "pointer" || name == "any-pointer" ||
                             name == "prefers-color-scheme" || name == "prefers-reduced-motion" ||
                             name == "prefers-contrast" || name == "color" || name == "monochrome" ||
                             name == "dynamic-range" || name == "display-mode" || name == "scripting" ||
                             name == "update" || name == "overflow-block" || name == "overflow-inline" ||
                             name == "forced-colors" || name == "inverted-colors")
                    {
                        undecidedReason = "the media feature '" + name + "'";
                        decided = false;
                        return false;
                    }
                    else
                    {
                        undecidedReason = "the media feature '" + name + "'";
                        decided = false;
                        return false;
                    }

                    if (close == std::string::npos)
                    {
                        break;
                    }

                    body = Trim(body.substr(close + 1));
                }

                if (negation ? !matches : matches)
                {
                    return true;
                }
            }

            return false;
        }

        /// Evaluates `@supports` at compile time by asking the same code paths a
        /// declaration uses: a property the registry supports whose value parses
        /// is a feature this compiler supports, and that is exactly the question
        /// the author is asking.
        bool EvaluateSupports(const std::string& text,
                              const CSSPropertyRegistry& registry,
                              bool& decided,
                              std::string& undecidedReason)
        {
            decided = true;

            const std::string body = LowerCase(Trim(text));
            const bool isDisjunction = body.find(" or ") != std::string::npos && body.find(" and ") == std::string::npos;
            const std::string separator = isDisjunction ? " or " : " and ";

            std::vector<std::string> conditions;
            std::size_t begin = 0;

            while (begin <= body.size())
            {
                const std::size_t next = body.find(separator, begin);
                conditions.push_back(
                    Trim(body.substr(begin, (next == std::string::npos ? body.size() : next) - begin)));

                if (next == std::string::npos)
                {
                    break;
                }

                begin = next + separator.size();
            }

            bool result = !isDisjunction;

            for (std::string condition : conditions)
            {
                bool negated = false;

                if (condition.rfind("not ", 0) == 0)
                {
                    negated = true;
                    condition = Trim(condition.substr(4));
                }

                bool supported = false;

                if (condition.size() >= 2 && condition.front() == '(' && condition.back() == ')')
                {
                    const std::string inside = Trim(condition.substr(1, condition.size() - 2));
                    const std::size_t colon = inside.find(':');

                    if (colon != std::string::npos)
                    {
                        const std::string property = Trim(inside.substr(0, colon));
                        const std::string value = Trim(inside.substr(colon + 1));

                        // The question `@supports` asks is exactly the question a
                        // declaration asks, so it is answered by the same code.
                        if (const CSSPropertyDefinition* definition = registry.Find(property))
                        {
                            CSSDiagnosticSink probe;
                            std::vector<CSSDeclaration> declarations =
                                CSSParser(probe).ParseDeclarationList("<supports>", property + ":" + value);

                            if (!declarations.empty())
                            {
                                CSSValue parsed;
                                supported = ParsePropertyValue(*definition, declarations.front().value, parsed,
                                                               declarations.front().location, probe) &&
                                            !probe.HasErrors();
                            }
                        }
                    }
                }
                else
                {
                    // `selector(...)`, `font-tech(...)`: not decided here.
                    undecidedReason = "the condition '" + condition + "'";
                    decided = false;
                    return false;
                }

                if (negated)
                {
                    supported = !supported;
                }

                if (isDisjunction)
                {
                    result = result || supported;
                }
                else
                {
                    result = result && supported;
                }
            }

            return result;
        }

        // -------------------------------------------------------------------
        // Shorthand expansion helpers
        // -------------------------------------------------------------------

        /// The four box longhands a shorthand's values expand into, per the CSS
        /// one-to-four value rule.
        struct BoxValues
        {
            const std::vector<CSSComponentValue>* top = nullptr;
            const std::vector<CSSComponentValue>* right = nullptr;
            const std::vector<CSSComponentValue>* bottom = nullptr;
            const std::vector<CSSComponentValue>* left = nullptr;
        };

        bool AssignBoxValues(const std::vector<std::vector<CSSComponentValue>>& parts, BoxValues& out)
        {
            switch (parts.size())
            {
            case 1:
                out.top = out.right = out.bottom = out.left = &parts[0];
                return true;

            case 2:
                out.top = out.bottom = &parts[0];
                out.right = out.left = &parts[1];
                return true;

            case 3:
                out.top = &parts[0];
                out.right = out.left = &parts[1];
                out.bottom = &parts[2];
                return true;

            case 4:
                out.top = &parts[0];
                out.right = &parts[1];
                out.bottom = &parts[2];
                out.left = &parts[3];
                return true;

            default:
                return false;
            }
        }

        /// The style and colour longhand of a border side, from its width
        /// longhand. Written as a switch rather than arithmetic so that reordering
        /// the property enum cannot silently pair the wrong properties.
        bool BorderSideTriple(CSSPropertyId width,
                              CSSPropertyId& style,
                              CSSPropertyId& color)
        {
            switch (width)
            {
            case CSSPropertyId::BorderTopWidth:
                style = CSSPropertyId::BorderTopStyle;
                color = CSSPropertyId::BorderTopColor;
                return true;

            case CSSPropertyId::BorderRightWidth:
                style = CSSPropertyId::BorderRightStyle;
                color = CSSPropertyId::BorderRightColor;
                return true;

            case CSSPropertyId::BorderBottomWidth:
                style = CSSPropertyId::BorderBottomStyle;
                color = CSSPropertyId::BorderBottomColor;
                return true;

            case CSSPropertyId::BorderLeftWidth:
                style = CSSPropertyId::BorderLeftStyle;
                color = CSSPropertyId::BorderLeftColor;
                return true;

            default:
                return false;
            }
        }

        /// One component value as a declaration component list, so that a
        /// shorthand's parts can be handed to the longhand parsers.
        std::vector<CSSComponentValue> AsValueList(const CSSComponentValue& value)
        {
            return std::vector<CSSComponentValue>{ value };
        }
    }

    const char* ToString(CSSOrigin origin)
    {
        switch (origin)
        {
        case CSSOrigin::UserAgent: return "user-agent";
        case CSSOrigin::User: return "user";
        case CSSOrigin::Author: return "author";
        }

        return "author";
    }

    // -----------------------------------------------------------------------
    // CSSComputedStyle
    // -----------------------------------------------------------------------

    const CSSValue* CSSComputedStyle::Get(CSSPropertyId id) const
    {
        for (const std::pair<CSSPropertyId, CSSValue>& entry : values)
        {
            if (entry.first == id)
            {
                return &entry.second;
            }
        }

        return nullptr;
    }

    namespace
    {
        /// The property id is the ordering key of both `values` and `sources`, so
        /// one comparator keeps them in the same order.
        bool OrderById(CSSPropertyId left, CSSPropertyId right)
        {
            return static_cast<int>(left) < static_cast<int>(right);
        }
    }

    void CSSComputedStyle::Set(CSSPropertyId id, CSSValue value, const CSSSourceLocation& source)
    {
        for (std::pair<CSSPropertyId, CSSValue>& entry : values)
        {
            if (entry.first == id)
            {
                entry.second = std::move(value);
                for (std::pair<CSSPropertyId, CSSSourceLocation>& entrySource : sources)
                {
                    if (entrySource.first == id)
                    {
                        entrySource.second = source;
                        break;
                    }
                }
                return;
            }
        }

        values.emplace_back(id, std::move(value));
        std::sort(values.begin(), values.end(),
                  [](const std::pair<CSSPropertyId, CSSValue>& left,
                     const std::pair<CSSPropertyId, CSSValue>& right) {
                      return OrderById(left.first, right.first);
                  });

        sources.emplace_back(id, source);
        std::sort(sources.begin(), sources.end(),
                  [](const std::pair<CSSPropertyId, CSSSourceLocation>& left,
                     const std::pair<CSSPropertyId, CSSSourceLocation>& right) {
                      return OrderById(left.first, right.first);
                  });
    }

    CSSSourceLocation CSSComputedStyle::SourceLocationOf(CSSPropertyId id) const
    {
        for (const std::pair<CSSPropertyId, CSSSourceLocation>& entry : sources)
        {
            if (entry.first == id)
            {
                return entry.second;
            }
        }

        return CSSSourceLocation{};
    }

    void CSSComputedStyle::Remove(CSSPropertyId id)
    {
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (values[index].first == id)
            {
                values.erase(values.begin() + static_cast<std::ptrdiff_t>(index));
                break;
            }
        }

        for (std::size_t index = 0; index < sources.size(); ++index)
        {
            if (sources[index].first == id)
            {
                sources.erase(sources.begin() + static_cast<std::ptrdiff_t>(index));
                break;
            }
        }
    }

    void CSSComputedStyle::SetCustomProperty(const std::string& name, std::vector<CSSComponentValue> value)
    {
        customProperties[name] = std::move(value);
    }

    const std::vector<CSSComponentValue>* CSSComputedStyle::GetCustomProperty(const std::string& name) const
    {
        const auto found = customProperties.find(name);
        return found != customProperties.end() ? &found->second : nullptr;
    }

    std::string CSSComputedStyle::Key() const
    {
        std::string key;

        for (const std::pair<CSSPropertyId, CSSValue>& entry : values)
        {
            key += std::to_string(static_cast<int>(entry.first));
            key += ':';
            entry.second.AppendKey(key);
            key += ';';
        }

        key += '|';

        for (const auto& entry : customProperties)
        {
            key += entry.first;
            key += ':';
            key += ComponentsToText(entry.second);
            key += ';';
        }

        return key;
    }

    std::string CSSComputedStyle::ToString() const
    {
        std::ostringstream output;

        for (const std::pair<CSSPropertyId, CSSValue>& entry : values)
        {
            const CSSPropertyDefinition* definition = CSSPropertyRegistry::Instance().Find(entry.first);

            output << (definition != nullptr ? definition->name : "unknown") << ": " << entry.second.ToString()
                   << ";\n";
        }

        for (const auto& entry : customProperties)
        {
            output << entry.first << ": " << ComponentsToText(entry.second) << ";\n";
        }

        return output.str();
    }

    // -----------------------------------------------------------------------
    // GenerateUserAgentStyleSheet
    // -----------------------------------------------------------------------

    std::string GenerateUserAgentStyleSheet()
    {
        const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();

        std::ostringstream output;
        output << "/* The native baseline. A native application's user-agent styles\n";
        output << " * are its native defaults, which live in the renderer rather than in\n";
        output << " * a stylesheet; this text states the same values as CSS so that the\n";
        output << " * web target and a coverage report can see them. */\n:root {\n";

        for (const CSSPropertyDefinition& definition : registry.Properties())
        {
            output << "  " << definition.name << ": " << definition.initialValue << ";\n";
        }

        output << "}\n";
        return output.str();
    }

    // -----------------------------------------------------------------------
    // CSSCascade
    // -----------------------------------------------------------------------

    CSSCascade::CSSCascade(CSSDiagnosticSink& diagnosticSink)
        : diagnostics(diagnosticSink)
    {
        const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();
        const std::size_t count = static_cast<std::size_t>(CSSPropertyId::Count);

        initialValues.resize(count);
        hasInitialValue.resize(count, 0);

        // The initial values come from the registry and are parsed with the same
        // parser a declaration uses, so the registry cannot state a value the
        // value parser would reject.
        for (const CSSPropertyDefinition& definition : registry.Properties())
        {
            CSSDiagnosticSink probe;
            const std::string text = std::string("x: ") + definition.initialValue;
            std::vector<CSSDeclaration> declarations =
                CSSParser(probe).ParseDeclarationList(UserAgentFileName, text);

            if (declarations.empty())
            {
                continue;
            }

            CSSValue parsed;
            if (ParsePropertyValue(definition, declarations.front().value, parsed, declarations.front().location,
                                   probe) &&
                !probe.HasErrors())
            {
                initialValues[static_cast<std::size_t>(definition.id)] = std::move(parsed);
                hasInitialValue[static_cast<std::size_t>(definition.id)] = 1;
            }
        }
    }

    void CSSCascade::AddStyleSheet(const CSSStylesheet& stylesheet, CSSOrigin origin)
    {
        StyleSheet entry;
        entry.origin = origin;
        entry.sheet = stylesheet;
        entry.order = sheetOrder++;
        styleSheets.push_back(std::move(entry));
    }

    void CSSCascade::AddUserAgentStyleSheet(const CSSStylesheet& stylesheet)
    {
        AddStyleSheet(stylesheet, CSSOrigin::UserAgent);
    }

    void CSSCascade::BuildRuleCache(const std::vector<CSSRule>& rules,
                                    RuleCache& cache,
                                    bool conditionPasses,
                                    CSSCascadeStatistics& statistics)
    {
        const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();

        for (const CSSRule& rule : rules)
        {
            if (rule.kind == CSSRule::Kind::At)
            {
                const std::string name = LowerCase(rule.at.name);

                if (name == "media")
                {
                    MediaEnvironment environment;
                    environment.width = options.viewportWidth;
                    environment.height = options.viewportHeight;

                    bool decided = true;
                    std::string reason;
                    const bool passes = EvaluateMediaQuery(rule.at.preludeText, environment, decided, reason);

                    if (!decided)
                    {
                        diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                            rule.at.location,
                                            "@media could not be evaluated during compilation.",
                                            reason + " depends on the runtime environment",
                                            "Use a media query over width, height or orientation, which the "
                                            "compiler evaluates against the application's target size.",
                                            "@media " + Trim(rule.at.preludeText));
                        ++statistics.skippedRuleCount;
                    }

                    RuleCacheEntry entry;
                    entry.conditionPasses = conditionPasses && passes;
                    cache[&rule] = entry;
                    BuildRuleCache(rule.at.children, cache, entry.conditionPasses, statistics);
                    continue;
                }

                if (name == "supports")
                {
                    bool decided = true;
                    std::string reason;
                    const bool passes = EvaluateSupports(rule.at.preludeText, registry, decided, reason);

                    if (!decided)
                    {
                        diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                            rule.at.location,
                                            "@supports could not be evaluated during compilation.",
                                            reason + " is not a plain declaration",
                                            "Write the condition as one or more '(property: value)' declarations.",
                                            "@supports " + Trim(rule.at.preludeText));
                        ++statistics.skippedRuleCount;
                    }

                    RuleCacheEntry entry;
                    entry.conditionPasses = conditionPasses && passes;
                    cache[&rule] = entry;
                    BuildRuleCache(rule.at.children, cache, entry.conditionPasses, statistics);
                    continue;
                }

                diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                    rule.at.location,
                                    "@" + rule.at.name + " is not implemented.",
                                    "this compiler compiles @media and @supports; the other at-rules belong to "
                                    "features it does not implement yet",
                                    "Remove the at-rule, or implement the feature it belongs to.",
                                    "@" + rule.at.name);
                ++statistics.skippedRuleCount;

                RuleCacheEntry entry;
                entry.conditionPasses = false;
                cache[&rule] = entry;
                continue;
            }

            RuleCacheEntry entry;
            entry.conditionPasses = conditionPasses;

            if (!conditionPasses)
            {
                ++statistics.skippedRuleCount;
                cache[&rule] = entry;
                continue;
            }

            if (!rule.selectors.empty())
            {
                CSSSelectorParser parser(diagnostics);
                entry.selectors = parser.Parse(rule.selectorText, rule.location);
            }

            // A selector whose state only exists at runtime is reported once,
            // here, rather than once per element it could have matched.
            for (const CSSComplexSelector& selector : entry.selectors.selectors)
            {
                if (!selector.supported)
                {
                    ++statistics.skippedRuleCount;
                    continue;
                }

                if (selector.NeedsRuntimeState())
                {
                    diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                        rule.location,
                                        "A selector depends on runtime state: '" + selector.text + "'.",
                                        "the native runtime selects a compiled style per state, and the "
                                        "state-variant table is not compiled yet",
                                        "Move the declaration into a class the document applies itself, or wait "
                                        "for the state-variant stage.",
                                        selector.text);
                    ++statistics.runtimeStateRuleCount;
                }
            }

            cache[&rule] = std::move(entry);
        }
    }

    void CSSCascade::FlattenRules(const std::vector<CSSRule>& rules, const RuleCache& cache,
                                  std::vector<FlattenedRule>& out)
    {
        for (const CSSRule& rule : rules)
        {
            const auto cached = cache.find(&rule);

            // A rule whose at-rule did not apply takes its whole subtree with it:
            // an `@media` that did not match contributes nothing at all.
            if (cached == cache.end() || !cached->second.conditionPasses)
            {
                continue;
            }

            if (rule.kind == CSSRule::Kind::At)
            {
                FlattenRules(rule.at.children, cache, out);
                continue;
            }

            out.push_back(FlattenedRule{ &rule, &cached->second });
        }
    }

    void CSSCascade::CollectMatchingDeclarations(CSSElementNode& element,
                                                 std::vector<MatchedDeclaration>& out,
                                                 const std::vector<std::vector<FlattenedRule>>& sheetRules,
                                                 CSSCascadeStatistics& statistics)
    {
        for (std::size_t sheetIndex = 0; sheetIndex < styleSheets.size(); ++sheetIndex)
        {
            const StyleSheet& sheet = styleSheets[sheetIndex];

            if (sheet.origin == CSSOrigin::Author && !options.applyAuthorStyles)
            {
                continue;
            }

            // The order counter continues across sheets, because a declaration
            // in a later sheet of the same origin wins at equal specificity. The
            // stride leaves room for every declaration of an earlier sheet.
            std::uint32_t order = static_cast<std::uint32_t>(sheet.order) * 1000000u;

            for (const FlattenedRule& flat : sheetRules[sheetIndex])
            {
                // One selector test per selector, with the specificity of the
                // one that matched kept, which is what the cascade orders by.
                std::uint32_t specificity = 0;
                bool matched = false;

                for (const CSSComplexSelector& selector : flat.entry->selectors.selectors)
                {
                    ++statistics.selectorComparisonCount;

                    if (!selector.supported || selector.NeedsRuntimeState())
                    {
                        continue;
                    }

                    if (MatchSelector(selector, element, &element.state))
                    {
                        matched = true;
                        specificity = std::max(specificity, selector.PackedSpecificity());
                    }
                }

                if (!matched)
                {
                    continue;
                }

                ++statistics.matchedRuleCount;

                for (const CSSDeclaration& declaration : flat.rule->declarations)
                {
                    MatchedDeclaration entry;
                    entry.declaration = &declaration;
                    entry.origin = sheet.origin;
                    entry.specificity = specificity;
                    entry.order = order++;
                    entry.important = declaration.important;
                    out.push_back(entry);
                }
            }
        }
    }

    bool CSSCascade::ExpandShorthand(const CSSShorthandDefinition& shorthand,
                                     const std::vector<CSSComponentValue>& value,
                                     std::vector<std::pair<CSSPropertyId, CSSValue>>& out,
                                     const CSSSourceLocation& location)
    {
        const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();
        const std::string rule = shorthand.rule != nullptr ? shorthand.rule : "";

        const auto parseAgainst = [&](CSSPropertyId id, const std::vector<CSSComponentValue>& components,
                                      CSSValue& parsed, const char* subject) -> bool {
            const CSSPropertyDefinition* definition = registry.Find(id);

            if (definition == nullptr)
            {
                return false;
            }

            CSSDiagnosticSink probe;
            if (!ParsePropertyValue(*definition, components, parsed, location, probe))
            {
                for (const CSSDiagnostic& diagnostic : probe.Entries())
                {
                    diagnostics.Report(diagnostic.code, diagnostic.severity, diagnostic.location, diagnostic.message,
                                       diagnostic.detail, diagnostic.suggestion,
                                       std::string(shorthand.name) + " -> " + subject);
                }

                return false;
            }

            return true;
        };

        if (rule == "box" || rule == "pair")
        {
            const std::vector<std::vector<CSSComponentValue>> items = ShorthandParts(value);

            if (items.empty() || items.size() > 4)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidShorthand,
                                  location,
                                  shorthand.name + std::string(" takes between one and four values."),
                                  std::to_string(items.size()) + " given",
                                  "Write one to four values, for example '1rem 2rem'.",
                                  shorthand.name);
                return false;
            }

            if (rule == "pair")
            {
                if (shorthand.targets.count != 2 || shorthand.targets.top == CSSPropertyId::Unknown ||
                    shorthand.targets.right == CSSPropertyId::Unknown)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidShorthand, location,
                                      "The '" + std::string(shorthand.name) + "' shorthand is not registered correctly.",
                                      "its registry entry does not name a pair of longhands",
                                      "This is a compiler bug.", shorthand.name);
                    return false;
                }

                const std::vector<CSSComponentValue>& first = items[0];
                const std::vector<CSSComponentValue>& second = items.size() > 1 ? items[1] : items[0];

                CSSValue parsed;
                if (!parseAgainst(shorthand.targets.top, first, parsed, "first value"))
                {
                    return false;
                }
                out.emplace_back(shorthand.targets.top, parsed);

                if (!parseAgainst(shorthand.targets.right, second, parsed, "second value"))
                {
                    return false;
                }
                out.emplace_back(shorthand.targets.right, parsed);
                return true;
            }

            BoxValues box;
            if (!AssignBoxValues(items, box))
            {
                return false;
            }

            struct Target
            {
                CSSPropertyId id;
                const std::vector<CSSComponentValue>* value;
            };

            const Target targets[] = { { shorthand.targets.top, box.top },
                                       { shorthand.targets.right, box.right },
                                       { shorthand.targets.bottom, box.bottom },
                                       { shorthand.targets.left, box.left } };

            for (const Target& target : targets)
            {
                if (target.id == CSSPropertyId::Unknown || target.value == nullptr)
                {
                    continue;
                }

                CSSValue parsed;
                if (!parseAgainst(target.id, *target.value, parsed, "a box side"))
                {
                    return false;
                }

                out.emplace_back(target.id, parsed);
            }

            return true;
        }

        if (rule == "radius")
        {
            const std::vector<std::vector<CSSComponentValue>> items = ShorthandParts(value);

            if (items.empty() || items.size() > 4)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidShorthand, location,
                                  "border-radius takes between one and four values.",
                                  std::to_string(items.size()) + " given",
                                  "Write one to four lengths.", shorthand.name);
                return false;
            }

            // The native border radius is circular, so a shorthand that names
            // different corners is reported rather than silently squared off.
            for (std::size_t index = 1; index < items.size(); ++index)
            {
                if (ComponentsToText(items[index]) != ComponentsToText(items[0]))
                {
                    diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                        location,
                                        "border-radius with different corners is not implemented.",
                                        "the native border radius is a single circular radius",
                                        "Use one radius for every corner.",
                                        "border-radius");
                    return false;
                }
            }

            CSSValue parsed;
            if (!parseAgainst(shorthand.targets.top, items[0], parsed, "radius"))
            {
                return false;
            }

            out.emplace_back(shorthand.targets.top, parsed);
            return true;
        }

        if (rule == "border" || rule == "border-side")
        {
            std::vector<CSSPropertyId> widths;

            if (rule == "border")
            {
                widths = { CSSPropertyId::BorderTopWidth, CSSPropertyId::BorderRightWidth,
                           CSSPropertyId::BorderBottomWidth, CSSPropertyId::BorderLeftWidth };
            }
            else if (shorthand.targets.top != CSSPropertyId::Unknown)
            {
                widths = { shorthand.targets.top };
            }

            if (widths.empty())
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidShorthand, location,
                                  "The '" + std::string(shorthand.name) + "' shorthand is not registered correctly.",
                                  "its registry entry does not name a border side",
                                  "This is a compiler bug.", shorthand.name);
                return false;
            }

            // `<border-width> || <border-style> || <border-color>`, in any order.
            const std::vector<const CSSComponentValue*> items = TopLevelItems(value);

            if (items.empty())
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidShorthand, location,
                                  shorthand.name + std::string(" needs at least one value."),
                                  "no value given",
                                  "Write '1px solid black'.", shorthand.name);
                return false;
            }

            CSSValue widthValue;
            CSSValue styleValue;
            CSSValue colorValue;
            bool hasWidth = false;
            bool hasStyle = false;
            bool hasColor = false;

            const CSSPropertyRegistry& propertyRegistry = registry;

            for (const CSSComponentValue* item : items)
            {
                // A color first, because `red` is a colour and not a style.
                if (!hasColor)
                {
                    CSSColor color;
                    std::string subject;

                    CSSDiagnosticSink probe;
                    if (ParseColorComponents(AsValueList(*item), color, location, probe, subject) &&
                        !probe.HasErrors())
                    {
                        colorValue = CSSValue::MakeColor(color);
                        hasColor = true;
                        continue;
                    }
                }

                if (!hasWidth)
                {
                    CSSDiagnosticSink probe;
                    CSSValue parsed;

                    if (ParsePropertyValue(*propertyRegistry.Find(CSSPropertyId::BorderTopWidth),
                                           AsValueList(*item), parsed, location, probe) &&
                        !probe.HasErrors())
                    {
                        widthValue = parsed;
                        hasWidth = true;
                        continue;
                    }
                }

                if (!hasStyle)
                {
                    CSSDiagnosticSink probe;
                    CSSValue parsed;

                    if (ParsePropertyValue(*propertyRegistry.Find(CSSPropertyId::BorderTopStyle),
                                           AsValueList(*item), parsed, location, probe) &&
                        !probe.HasErrors())
                    {
                        styleValue = parsed;
                        hasStyle = true;
                        continue;
                    }
                }

                diagnostics.Error(CSSDiagnosticCode::InvalidShorthand,
                                  location,
                                  "Not a valid component of '" + std::string(shorthand.name) + "'.",
                                  item->token.raw.empty() ? item->token.text : item->token.raw,
                                  "A border is a width, a style and a color, in any order.",
                                  shorthand.name);
                return false;
            }

            // Omitted components take their initial value, which is what makes
            // `border: none` clear a border rather than change only its style.
            for (CSSPropertyId widthId : widths)
            {
                CSSPropertyId styleId = CSSPropertyId::Unknown;
                CSSPropertyId colorId = CSSPropertyId::Unknown;

                if (!BorderSideTriple(widthId, styleId, colorId))
                {
                    continue;
                }

                const auto initialState = [&](CSSPropertyId id, CSSValue& target) {
                    const std::size_t index = static_cast<std::size_t>(id);
                    if (index < hasInitialValue.size() && hasInitialValue[index] != 0)
                    {
                        target = initialValues[index];
                        return true;
                    }

                    return false;
                };

                CSSValue resolvedWidth = widthValue;
                CSSValue resolvedStyle = styleValue;
                CSSValue resolvedColor = colorValue;

                if (!hasWidth && !initialState(widthId, resolvedWidth))
                {
                    return false;
                }

                if (!hasStyle && !initialState(styleId, resolvedStyle))
                {
                    return false;
                }

                if (!hasColor && !initialState(colorId, resolvedColor))
                {
                    return false;
                }

                out.emplace_back(widthId, resolvedWidth);
                out.emplace_back(styleId, resolvedStyle);
                out.emplace_back(colorId, resolvedColor);
            }

            return true;
        }

        if (rule == "background")
        {
            // The native runtime paints a background color. Anything else in the
            // shorthand is reported rather than dropped silently.
            for (const CSSComponentValue& component : value)
            {
                if (component.isFunction)
                {
                    diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                        location,
                                        "background images and gradients are not implemented.",
                                        "the native painter fills a background color and paints images through "
                                        "the resource table, which a CSS background is not wired to yet",
                                        "Use background-color.",
                                        "background");
                    return false;
                }

                if (component.token.type == CSSTokenType::Url)
                {
                    diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature, location,
                                        "background image urls are not implemented.",
                                        "the native painter fills a background color",
                                        "Use background-color and an <img> element.", "background");
                    return false;
                }
            }

            CSSValue parsed;
            if (!parseAgainst(shorthand.targets.top, value, parsed, "color"))
            {
                return false;
            }

            out.emplace_back(shorthand.targets.top, parsed);
            return true;
        }

        if (rule == "font")
        {
            // `[ <font-style> || <font-weight> ]? <font-size> [ / <line-height> ]? <font-family>`
            const std::vector<const CSSComponentValue*> items = TopLevelItems(value);

            if (items.empty())
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidShorthand, location, "font needs a value.",
                                  "no value given", "Write 'bold 1rem sans-serif'.", "font");
                return false;
            }

            const std::vector<std::string> systemFonts = { "caption", "icon",  "menu",      "message-box",
                                                           "small-caption", "status-bar" };

            if (items.size() == 1 && items[0]->token.type == CSSTokenType::Ident)
            {
                for (const std::string& systemFont : systemFonts)
                {
                    if (LowerCase(items[0]->token.text) == systemFont)
                    {
                        diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature, location,
                                            "System fonts are not implemented: '" + items[0]->token.text + "'.",
                                            "the native runtime has one font face",
                                            "Write an explicit font-size.", "font");
                        return false;
                    }
                }
            }

            const CSSPropertyRegistry& propertyRegistry = registry;
            const CSSPropertyDefinition* sizeDefinition = propertyRegistry.Find(CSSPropertyId::FontSize);
            const CSSPropertyDefinition* weightDefinition = propertyRegistry.Find(CSSPropertyId::FontWeight);
            const CSSPropertyDefinition* heightDefinition = propertyRegistry.Find(CSSPropertyId::LineHeight);

            CSSValue sizeValue;
            CSSValue weightValue;
            CSSValue heightValue;
            bool hasSize = false;
            bool hasWeight = false;
            bool hasHeight = false;
            std::string family;

            for (std::size_t index = 0; index < items.size(); ++index)
            {
                const CSSComponentValue* item = items[index];

                if (!hasSize)
                {
                    CSSDiagnosticSink probe;
                    CSSValue parsed;

                    if (ParsePropertyValue(*sizeDefinition, AsValueList(*item), parsed, location, probe) &&
                        !probe.HasErrors())
                    {
                        sizeValue = parsed;
                        hasSize = true;

                        // The optional `/ <line-height>` right after the size.
                        if (index + 1 < items.size() && items[index + 1]->token.IsDelim('/'))
                        {
                            if (index + 2 >= items.size())
                            {
                                diagnostics.Error(CSSDiagnosticCode::InvalidShorthand, location,
                                                  "'/' in 'font' needs a line height.",
                                                  "no value after '/'",
                                                  "Write '1rem/1.4'.", "font");
                                return false;
                            }

                            CSSDiagnosticSink heightProbe;
                            CSSValue heightParsed;

                            if (!ParsePropertyValue(*heightDefinition, AsValueList(*items[index + 2]),
                                                    heightParsed, location, heightProbe) ||
                                heightProbe.HasErrors())
                            {
                                for (const CSSDiagnostic& diagnostic : heightProbe.Entries())
                                {
                                    diagnostics.Report(diagnostic.code, diagnostic.severity, diagnostic.location,
                                                       diagnostic.message, diagnostic.detail,
                                                       diagnostic.suggestion, "font -> line-height");
                                }

                                return false;
                            }

                            heightValue = heightParsed;
                            hasHeight = true;
                            index += 2;
                        }

                        continue;
                    }
                }

                if (!hasWeight && index + 1 <= items.size())
                {
                    CSSDiagnosticSink probe;
                    CSSValue parsed;

                    if (ParsePropertyValue(*weightDefinition, AsValueList(*item), parsed, location, probe) &&
                        !probe.HasErrors())
                    {
                        weightValue = parsed;
                        hasWeight = true;
                        continue;
                    }
                }

                // `italic` and `oblique` are font-style, which the native font
                // registry has no axis for.
                if (item->token.type == CSSTokenType::Ident)
                {
                    const std::string name = LowerCase(item->token.text);

                    if (name == "italic" || name == "oblique")
                    {
                        diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature, location,
                                            "font-style: " + name + " is not implemented.",
                                            "the native font registry has one upright face",
                                            "Remove it from the 'font' shorthand.", "font");
                        continue;
                    }

                    if (name == "normal" && !hasWeight)
                    {
                        weightValue = CSSValue::MakeKeyword("normal");
                        hasWeight = true;
                        continue;
                    }
                }

                if (hasSize)
                {
                    family += (family.empty() ? "" : " ") + ComponentsToText(AsValueList(*item));
                    continue;
                }

                diagnostics.Error(CSSDiagnosticCode::InvalidShorthand,
                                  location,
                                  "Not a valid component of 'font'.",
                                  item->token.raw.empty() ? item->token.text : item->token.raw,
                                  "Write '[ <weight> ]? <size> [ / <line-height> ]? <family>'.",
                                  "font");
                return false;
            }

            if (!hasSize)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidShorthand, location, "'font' needs a font size.",
                                  "no size could be found",
                                  "Write 'bold 1rem sans-serif'.", "font");
                return false;
            }

            if (!family.empty())
            {
                const std::string lowered = LowerCase(family);

                if (lowered.find("sans-serif") == std::string::npos && lowered.find("serif") == std::string::npos &&
                    lowered.find("monospace") == std::string::npos && lowered.find("system-ui") == std::string::npos)
                {
                    diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                        location,
                                        "The 'font' shorthand names a font family: " + family + ".",
                                        "the native runtime has one font face",
                                        "The size, weight and line height were applied; the family was ignored.",
                                        "font");
                }
            }

            out.emplace_back(CSSPropertyId::FontSize, sizeValue);

            if (hasHeight)
            {
                out.emplace_back(CSSPropertyId::LineHeight, heightValue);
            }

            if (hasWeight)
            {
                out.emplace_back(CSSPropertyId::FontWeight, weightValue);
            }

            return true;
        }

        diagnostics.Error(CSSDiagnosticCode::InvalidShorthand, location,
                          "The '" + std::string(shorthand.name) + "' shorthand has no expansion rule.",
                          "the registry does not name one this compiler implements",
                          "This is a compiler bug.", shorthand.name);
        return false;
    }

    void CSSCascade::ApplyLonghand(CSSPropertyId id,
                                   const CSSDeclaration& declaration,
                                   const CSSComputedStyle* parent,
                                   CSSComputedStyle& out,
                                   bool& applied)
    {
        applied = false;

        const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();
        const CSSPropertyDefinition* definition = registry.Find(id);

        if (definition == nullptr)
        {
            diagnostics.Error(CSSDiagnosticCode::UnknownProperty,
                              declaration.location,
                              UnknownPropertyMessage(declaration.property),
                              "CSS has no property with this name, and the compiler will not guess what you "
                              "meant.",
                              "Use the real property name, or delete this line.",
                              declaration.property);
            return;
        }

        const std::size_t index = static_cast<std::size_t>(id);
        const auto applyInitial = [&]() {
            if (index < hasInitialValue.size() && hasInitialValue[index] != 0)
            {
                out.Set(id, initialValues[index]);
                applied = true;
            }
            else
            {
                out.Remove(id);
            }
        };

        // CSS-wide keywords, decided before the grammar sees the value.
        const std::vector<CSSComponentValue> significant = SignificantValues(declaration.value);

        if (significant.size() == 1 && significant[0].token.type == CSSTokenType::Ident)
        {
            const std::string keyword = LowerCase(significant[0].token.text);

            if (keyword == "inherit")
            {
                if (id == CSSPropertyId::Color)
                {
                    if (parent != nullptr)
                    {
                        if (const CSSValue* inherited = parent->Get(id))
                        {
                            out.Set(id, *inherited, parent->SourceLocationOf(id));
                            applied = true;
                        }
                    }
                }
                else if (parent != nullptr)
                {
                    if (const CSSValue* inherited = parent->Get(id))
                    {
                        out.Set(id, *inherited, parent->SourceLocationOf(id));
                        applied = true;
                    }
                    else if (definition->inherited)
                    {
                        applyInitial();
                    }
                    else
                    {
                        diagnostics.Warning(CSSDiagnosticCode::InvalidValue,
                                            declaration.location,
                                            "'inherit' on a non-inherited property with no inherited value.",
                                            std::string(definition->name) + " does not inherit",
                                            "The declaration was dropped; the native default applies.",
                                            declaration.property);
                    }
                }

                return;
            }

            if (keyword == "initial" || keyword == "revert" || keyword == "revert-layer")
            {
                applyInitial();
                return;
            }

            if (keyword == "unset")
            {
                if (definition->inherited && parent != nullptr)
                {
                    if (const CSSValue* inherited = parent->Get(id))
                    {
                        out.Set(id, *inherited, parent->SourceLocationOf(id));
                        applied = true;
                        return;
                    }
                }

                applyInitial();
                return;
            }
        }

        // `var()` is substituted on the component values, before the grammar
        // sees them, which is where the specification puts it.
        std::vector<CSSComponentValue> substituted;

        if (ContainsVarFunction(declaration.value))
        {
            std::string unresolved;

            const auto lookup = [&out](const std::string& name) -> const std::vector<CSSComponentValue>* {
                return out.GetCustomProperty(name);
            };

            if (!SubstituteVariables(declaration.value, lookup, substituted, diagnostics, declaration.location,
                                     unresolved))
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                  declaration.location,
                                  "Custom property " + unresolved + " is not defined.",
                                  "var() references a custom property that neither this element nor an ancestor "
                                  "declares",
                                  "Define it with a '--name: value' declaration.",
                                  declaration.property);
                return;
            }
        }
        else
        {
            substituted = declaration.value;
        }

        CSSValue parsed;

        if (!ParsePropertyValue(*definition, substituted, parsed, declaration.location, diagnostics))
        {
            return;
        }

        // Two values are relative to the inherited value, so the cascade has to
        // resolve them: that is the one place a property's computed value
        // depends on the value it would otherwise have.
        if (id == CSSPropertyId::FontWeight && parsed.kind == CSSValueKind::Keyword)
        {
            if (parsed.text == "bolder" || parsed.text == "lighter")
            {
                const CSSValue* inherited = parent != nullptr ? parent->Get(id) : nullptr;
                double current = 400.0;

                if (inherited != nullptr && !inherited->text.empty())
                {
                    if (inherited->text == "bold")
                    {
                        current = 700.0;
                    }
                    else if (inherited->text == "normal")
                    {
                        current = 400.0;
                    }
                }

                if (inherited != nullptr && inherited->number > 1.0)
                {
                    current = inherited->number;
                }

                const double resolved = parsed.text == "bolder" ? (current < 500.0   ? 700.0
                                                                   : current < 700.0 ? 900.0
                                                                                     : 900.0)
                                                                 : (current > 700.0   ? 400.0
                                                                    : current > 500.0 ? 400.0
                                                                                      : 100.0);

                parsed = resolved >= 600.0 ? CSSValue::MakeKeyword("bold") : CSSValue::MakeKeyword("normal");
                parsed.number = resolved;
            }
        }

        if (id == CSSPropertyId::FontSize && parsed.kind == CSSValueKind::Keyword)
        {
            if (parsed.text == "smaller" || parsed.text == "larger")
            {
                const CSSValue* inherited = parent != nullptr ? parent->Get(id) : nullptr;
                double current = static_cast<double>(options.rootFontSize);

                if (inherited != nullptr)
                {
                    float absolute = 0.0f;

                    if (inherited->IsAbsoluteLength(absolute))
                    {
                        current = absolute;
                    }
                }

                parsed = CSSValue::MakeDimension(parsed.text == "smaller" ? current / 1.2 : current * 1.2,
                                                 CSSUnit::Px);
            }
        }

        if (id == CSSPropertyId::Color && parsed.kind == CSSValueKind::Color &&
            parsed.color.kind == CSSColorKind::CurrentColor)
        {
            // `color: currentColor` means the inherited colour.
            if (parent != nullptr)
            {
                if (const CSSValue* inherited = parent->Get(id))
                {
                    out.Set(id, *inherited, parent->SourceLocationOf(id));
                    applied = true;
                }
            }

            return;
        }

        out.Set(id, std::move(parsed), declaration.location);
        applied = true;
    }

    void CSSCascade::ApplyAll(const CSSDeclaration& declaration, const CSSComputedStyle* parent, CSSComputedStyle& out)
    {
        // `all` takes only the CSS-wide keywords, and the value grammar has
        // already rejected anything else, so this reads the one keyword and
        // gives every property that keyword as if it had been written for that
        // property alone. It reuses the same initial values and the same
        // inherited lookup the longhand path uses, which is what makes
        // `all: inherit` and `all: initial` agree with `color: inherit` and
        // `color: initial` on every property.
        //
        // Custom properties are deliberately untouched: the specification
        // excludes them. So are `direction` and `unicode-bidi`, which the
        // registry does not carry - the exemption is recorded here rather than
        // left implicit if either is ever implemented.
        //
        // The value goes through the property's own grammar, which for `all` is
        // exactly the CSS-wide keywords. That is what keeps `all: 3px` a
        // reported invalid value instead of a declaration that quietly did
        // nothing.
        const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();
        const CSSPropertyDefinition* definition = registry.Find(CSSPropertyId::All);
        CSSValue parsed;

        if (definition == nullptr ||
            !ParsePropertyValue(*definition, declaration.value, parsed, declaration.location, diagnostics))
        {
            return;
        }

        if (parsed.kind != CSSValueKind::Keyword)
        {
            return;
        }

        const std::string keyword = parsed.text;

        for (const CSSPropertyDefinition& definition : registry.Properties())
        {
            if (definition.id == CSSPropertyId::All)
            {
                continue;
            }

            // `inherit`, and `unset` on an inherited property, take the parent's
            // computed value when there is one. Nothing is reported when there is
            // not: `all` is not about one property, and a warning per property
            // would be noise where a diagnosis belongs.
            if (keyword == "inherit" || (keyword == "unset" && definition.inherited))
            {
                const CSSValue* inherited = parent != nullptr ? parent->Get(definition.id) : nullptr;

                if (inherited != nullptr)
                {
                    out.Set(definition.id, *inherited, parent->SourceLocationOf(definition.id));
                    continue;
                }
            }

            const std::size_t index = static_cast<std::size_t>(definition.id);

            if (index < hasInitialValue.size() && hasInitialValue[index] != 0)
            {
                out.Set(definition.id, initialValues[index]);
            }
            else
            {
                out.Remove(definition.id);
            }
        }
    }

    void CSSCascade::ComputeElement(const CSSComputedStyle* parent,
                                    const std::vector<MatchedDeclaration>& matched,
                                    CSSComputedStyle& out)
    {
        const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();

        // Custom properties first: they are inherited as a whole and they have
        // to exist before a `var()` in this element's own declarations can be
        // substituted.
        if (parent != nullptr)
        {
            for (const auto& entry : parent->CustomProperties())
            {
                out.SetCustomProperty(entry.first, entry.second);
            }
        }

        for (const MatchedDeclaration& entry : matched)
        {
            if (entry.declaration->isCustomProperty)
            {
                out.SetCustomProperty(entry.declaration->property, entry.declaration->value);
            }
        }

        // Then the declarations, in cascade order, so that the last one to write
        // a property is its winner. Sorting here is what the cascade *is*: the
        // specificity and source order of every candidate, decided once.
        std::vector<MatchedDeclaration> ordered = matched;
        std::stable_sort(ordered.begin(), ordered.end(),
                         [](const MatchedDeclaration& left, const MatchedDeclaration& right) {
                             const int leftRank = OriginRank(left.origin, left.important);
                             const int rightRank = OriginRank(right.origin, right.important);

                             if (leftRank != rightRank)
                             {
                                 return leftRank < rightRank;
                             }

                             if (left.specificity != right.specificity)
                             {
                                 return left.specificity < right.specificity;
                             }

                             return left.order < right.order;
                         });

        for (const MatchedDeclaration& entry : ordered)
        {
            const CSSDeclaration& declaration = *entry.declaration;

            if (declaration.isCustomProperty)
            {
                continue;
            }

            const std::string name = LowerCase(declaration.property);

            // `all` is the one property whose effect is every property, so it is
            // expanded here rather than resolved to a value of its own.
            if (const CSSPropertyDefinition* definition = registry.Find(name);
                definition != nullptr && definition->id == CSSPropertyId::All)
            {
                ApplyAll(declaration, parent, out);
                continue;
            }

            if (const CSSShorthandDefinition* shorthand = registry.FindShorthand(name))
            {
                if (!shorthand->supported)
                {
                    diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                        declaration.location,
                                        "The '" + name + "' shorthand is not implemented.",
                                        "the registry knows the shorthand but this compiler cannot expand it",
                                        "Write the longhands instead.", name);
                    continue;
                }

                std::vector<std::pair<CSSPropertyId, CSSValue>> expanded;

                if (!ExpandShorthand(*shorthand, declaration.value, expanded, declaration.location))
                {
                    continue;
                }

                for (std::pair<CSSPropertyId, CSSValue>& pair : expanded)
                {
                    out.Set(pair.first, std::move(pair.second), declaration.location);
                }

                continue;
            }

            if (registry.IsKnownUnsupportedName(name))
            {
                // The registry registers a property it does not compile, so the
                // report can say which specification the declaration is from
                // rather than only that support is missing. That is what makes
                // the registration worth having: the name is not merely known,
                // it is placed.
                const CSSPropertyDefinition* registered = registry.FindRegistered(name);
                const std::string origin =
                    registered != nullptr
                        ? " The property is defined by " + std::string(registered->spec) + "."
                        : std::string();

                diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                    declaration.location,
                                    "This compiler does not compile '" + name + "' yet." + origin,
                                    "The CSS rules define this property and support for it is not built yet, "
                                    "so the declaration is reported and left out of the compiled style.",
                                    "Remove the declaration, or write the effect with a property the compiler "
                                    "supports.",
                                    name);
                continue;
            }

            const CSSPropertyDefinition* definition = registry.Find(name);

            if (definition == nullptr)
            {
                diagnostics.Error(CSSDiagnosticCode::UnknownProperty,
                                  declaration.location,
                                  UnknownPropertyMessage(name),
                                  "CSS has no property with this name, and the compiler will not guess what you "
                                  "meant.",
                                  "Use the real property name, or delete this line.",
                                  name);
                continue;
            }

            bool applied = false;
            ApplyLonghand(definition->id, declaration, parent, out, applied);
        }

        // Inheritance, applied where the element itself said nothing.
        if (parent != nullptr)
        {
            for (CSSPropertyId id : registry.InheritedProperties())
            {
                if (out.Has(id))
                {
                    continue;
                }

                if (const CSSValue* inherited = parent->Get(id))
                {
                    // The value came from the ancestor that declared it, so the
                    // location follows it there.
                    out.Set(id, *inherited, parent->SourceLocationOf(id));
                }
            }
        }
    }

    CSSCascadeResult CSSCascade::Compute(CSSElementNode& root)
    {
        CSSCascadeResult result;

        // Every rule is parsed, and every at-rule decided, exactly once - before
        // any element is matched. This is the compile-time work that a browser
        // repeats whenever its style is invalidated.
        RuleCache cache;

        // The rules each element is tested against, flattened once. See
        // FlattenedRule: this is what keeps matching an element a walk rather
        // than a lookup per rule.
        std::vector<std::vector<FlattenedRule>> sheetRules(styleSheets.size());

        for (std::size_t sheetIndex = 0; sheetIndex < styleSheets.size(); ++sheetIndex)
        {
            const StyleSheet& sheet = styleSheets[sheetIndex];

            if (sheet.origin == CSSOrigin::Author && !options.applyAuthorStyles)
            {
                continue;
            }

            result.statistics.ruleCount += sheet.sheet.rules.size();
            BuildRuleCache(sheet.sheet.rules, cache, true, result.statistics);
            FlattenRules(sheet.sheet.rules, cache, sheetRules[sheetIndex]);
        }

        // A pre-order walk, because an element's computation needs its parent's.
        // The parent is held as an index rather than a pointer: the style vector
        // grows as the walk proceeds, and a reference into it would not survive
        // the next element.
        constexpr std::size_t NoParent = static_cast<std::size_t>(-1);
        std::vector<std::pair<CSSElementNode*, std::size_t>> pending;
        pending.emplace_back(&root, NoParent);

        std::vector<MatchedDeclaration> matched;

        while (!pending.empty())
        {
            CSSElementNode* element = pending.back().first;
            const std::size_t parentIndex = pending.back().second;
            pending.pop_back();

            const CSSComputedStyle* parent =
                parentIndex == NoParent ? nullptr : &result.elementStyles[parentIndex];

            matched.clear();
            CollectMatchingDeclarations(*element, matched, sheetRules, result.statistics);

            result.statistics.declarationCount += matched.size();

            CSSComputedStyle style;
            ComputeElement(parent, matched, style);

            result.statistics.elementCount += 1;
            result.statistics.appliedDeclarationCount += style.Values().size();

            result.elements.push_back(element);
            const std::size_t styleIndex = result.elementStyles.size();
            result.elementStyles.push_back(std::move(style));

            // Children, in reverse so the walk visits them in document order.
            for (std::size_t index = element->children.size(); index > 0; --index)
            {
                pending.emplace_back(element->children[index - 1], styleIndex);
            }
        }

        // Interning: elements with the same computed style share one entry, which
        // is what keeps a document of ten thousand identical paragraphs from
        // carrying ten thousand copies of the same values.
        std::map<std::string, std::uint32_t> interned;
        result.internedIndex.assign(result.elementStyles.size(), 0);

        for (std::size_t index = 0; index < result.elementStyles.size(); ++index)
        {
            if (result.elementStyles[index].IsEmpty())
            {
                continue;
            }

            const std::string key = result.elementStyles[index].Key();
            const auto found = interned.find(key);

            if (found != interned.end())
            {
                result.internedIndex[index] = found->second;
                continue;
            }

            const auto id = static_cast<std::uint32_t>(result.internedStyles.size() + 1);
            result.internedStyles.push_back(result.elementStyles[index]);
            interned.emplace(key, id);
            result.internedIndex[index] = id;
        }

        result.statistics.uniqueStyleCount = result.internedStyles.size();
        result.statistics.overriddenDeclarationCount =
            result.statistics.declarationCount > result.statistics.appliedDeclarationCount
                ? result.statistics.declarationCount - result.statistics.appliedDeclarationCount
                : 0;

        return result;
    }

    // -----------------------------------------------------------------------
    // Coverage
    // -----------------------------------------------------------------------

    std::string UnknownPropertyMessage(const std::string& name)
    {
        // `colour: red` is the mistake this message exists for: the answer the
        // reader needs is not "no", it is "color".
        std::string message = "There is no CSS property named '" + name + "'.";

        const char* similar = CSSPropertyRegistry::Instance().SuggestSimilarName(name);
        if (similar != nullptr)
        {
            message += " Did you mean '" + std::string(similar) + "'?";
        }

        return message;
    }

    CSSCoverageReport BuildCoverageReport()
    {
        CSSCoverageReport report;

        const CSSPropertyRegistry& registry = CSSPropertyRegistry::Instance();

        for (const CSSPropertyDefinition& definition : registry.Properties())
        {
            if (definition.supported)
            {
                ++report.implementedPropertyCount;
                report.implemented.push_back(definition.name);
            }
            else
            {
                ++report.unsupportedPropertyCount;
                report.unsupported.push_back(definition.name);
            }
        }

        for (const CSSShorthandDefinition& shorthand : registry.Shorthands())
        {
            ++report.shorthandCount;
            report.shorthands.push_back(shorthand.name);
        }

        for (const char* name : registry.UnsupportedNames())
        {
            ++report.unsupportedPropertyCount;
            report.unsupported.push_back(name);
        }

        const char* units[] = { "px", "cm", "mm", "q", "in", "pt", "pc", "em", "rem", "ex", "ch", "cap", "ic",
                                "lh", "rlh", "vw", "vh", "vmin", "vmax", "vb", "vi", "svw", "svh", "svmin",
                                "svmax", "svb", "svi", "lvw", "lvh", "lvmin", "lvmax", "lvb", "lvi", "dvw",
                                "dvh", "dvmin", "dvmax", "dvb", "dvi", "cqw", "cqh", "cqi", "cqb", "cqmin",
                                "cqmax", "deg", "rad", "grad", "turn", "s", "ms", "hz", "khz", "dpi", "dpcm",
                                "dppx", "fr" };

        report.unitCount = sizeof(units) / sizeof(units[0]);

        const char* pseudoClasses[] = { "root",        "first-child", "last-child",  "only-child",
                                        "nth-child",   "nth-last-child", "first-of-type", "last-of-type",
                                        "only-of-type", "nth-of-type", "nth-last-of-type", "empty",
                                        "not",         "is",          "where",       "has",
                                        "hover",       "active",      "focus",       "focus-visible",
                                        "focus-within", "disabled",   "enabled",     "checked",
                                        "required",    "optional",    "read-only",   "read-write",
                                        "open",        "closed",      "placeholder-shown", "default",
                                        "indeterminate", "link",      "visited",     "target",
                                        "any-link" };

        report.pseudoClassCount = sizeof(pseudoClasses) / sizeof(pseudoClasses[0]);
        report.selectorCount = report.pseudoClassCount + 7; // universal, type, class, id, attribute, ::before, ::after

        return report;
    }

    std::string CSSCoverageReport::ToMarkdown() const
    {
        std::ostringstream output;

        output << "## CSS coverage\n\n";
        output << "| | Count |\n| --- | --- |\n";
        output << "| Properties implemented | " << implementedPropertyCount << " |\n";
        output << "| Shorthands implemented | " << shorthandCount << " |\n";
        output << "| Units typed | " << unitCount << " |\n";
        output << "| Pseudo-classes recognised | " << pseudoClassCount << " |\n";
        output << "| Reference properties still unsupported | " << unsupportedPropertyCount << " |\n\n";

        output << "Implemented properties:\n\n";

        for (const std::string& name : implemented)
        {
            output << "- `" << name << "`\n";
        }

        output << "\nShorthands:\n\n";

        for (const std::string& name : shorthands)
        {
            output << "- `" << name << "`\n";
        }

        output << "\nReference properties this compiler reports as unsupported (RCC010):\n\n";

        for (const std::string& name : unsupported)
        {
            output << "- `" << name << "`\n";
        }

        return output.str();
    }
}
