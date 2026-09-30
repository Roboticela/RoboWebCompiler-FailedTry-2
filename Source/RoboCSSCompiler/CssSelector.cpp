#include "RoboCSSCompiler/CssSelector.h"

#include "RoboCSSCompiler/CssSyntax.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace RoboCSSCompiler
{
    const char* ToString(CSSCombinator combinator)
    {
        switch (combinator)
        {
        case CSSCombinator::None: return "";
        case CSSCombinator::Descendant: return " ";
        case CSSCombinator::Child: return " > ";
        case CSSCombinator::NextSibling: return " + ";
        case CSSCombinator::SubsequentSibling: return " ~ ";
        }

        return "";
    }

    const char* ToString(CSSPseudoClass pseudoClass)
    {
        switch (pseudoClass)
        {
        case CSSPseudoClass::Root: return "root";
        case CSSPseudoClass::FirstChild: return "first-child";
        case CSSPseudoClass::LastChild: return "last-child";
        case CSSPseudoClass::OnlyChild: return "only-child";
        case CSSPseudoClass::NthChild: return "nth-child";
        case CSSPseudoClass::NthLastChild: return "nth-last-child";
        case CSSPseudoClass::FirstOfType: return "first-of-type";
        case CSSPseudoClass::LastOfType: return "last-of-type";
        case CSSPseudoClass::OnlyOfType: return "only-of-type";
        case CSSPseudoClass::NthOfType: return "nth-of-type";
        case CSSPseudoClass::NthLastOfType: return "nth-last-of-type";
        case CSSPseudoClass::Empty: return "empty";
        case CSSPseudoClass::Scope: return "scope";
        case CSSPseudoClass::Not: return "not";
        case CSSPseudoClass::Is: return "is";
        case CSSPseudoClass::Where: return "where";
        case CSSPseudoClass::Has: return "has";
        case CSSPseudoClass::Hover: return "hover";
        case CSSPseudoClass::Active: return "active";
        case CSSPseudoClass::Focus: return "focus";
        case CSSPseudoClass::FocusVisible: return "focus-visible";
        case CSSPseudoClass::FocusWithin: return "focus-within";
        case CSSPseudoClass::Disabled: return "disabled";
        case CSSPseudoClass::Enabled: return "enabled";
        case CSSPseudoClass::Checked: return "checked";
        case CSSPseudoClass::Required: return "required";
        case CSSPseudoClass::Optional: return "optional";
        case CSSPseudoClass::ReadOnly: return "read-only";
        case CSSPseudoClass::ReadWrite: return "read-write";
        case CSSPseudoClass::Open: return "open";
        case CSSPseudoClass::Closed: return "closed";
        case CSSPseudoClass::PlaceholderShown: return "placeholder-shown";
        case CSSPseudoClass::Default: return "default";
        case CSSPseudoClass::Indeterminate: return "indeterminate";
        case CSSPseudoClass::Link: return "link";
        case CSSPseudoClass::Visited: return "visited";
        case CSSPseudoClass::Target: return "target";
        case CSSPseudoClass::AnyLink: return "any-link";
        case CSSPseudoClass::Unknown: break;
        }

        return "unknown";
    }

    // -----------------------------------------------------------------------
    // The element view
    // -----------------------------------------------------------------------

    void CSSElementNode::NumberSiblings(CSSElementNode& node)
    {
        const std::size_t count = node.children.size();

        // A node's own sibling numbers are its parent's business: a child's
        // `siblingCount` is how many siblings it has, which is the parent's child
        // count. The root has no siblings, so it is the only one it counts.
        if (node.parent == nullptr)
        {
            node.siblingIndex = 1;
            node.siblingCount = 1;
        }

        for (std::size_t index = 0; index < count; ++index)
        {
            CSSElementNode& child = *node.children[index];
            child.parent = &node;
            child.siblingIndex = index + 1;
            child.siblingCount = count;
        }

        // `:nth-of-type` needs the position among siblings of the same type, and
        // that needs every sibling first.
        for (std::size_t index = 0; index < count; ++index)
        {
            CSSElementNode& child = *node.children[index];

            std::size_t typeIndex = 0;
            std::size_t typeCount = 0;

            for (const CSSElementNode* sibling : node.children)
            {
                if (sibling->tagName == child.tagName)
                {
                    ++typeCount;
                    if (sibling->siblingIndex <= child.siblingIndex)
                    {
                        ++typeIndex;
                    }
                }
            }

            child.typeIndex = typeIndex == 0 ? 1 : typeIndex;
            child.typeCount = typeCount == 0 ? 1 : typeCount;

            NumberSiblings(child);
        }
    }

    // -----------------------------------------------------------------------
    // an+b
    // -----------------------------------------------------------------------

    bool CSSNthExpression::Matches(std::size_t index) const
    {
        const int value = static_cast<int>(index);

        if (step == 0)
        {
            return value == offset;
        }

        const int difference = value - offset;

        if (step > 0)
        {
            return difference >= 0 && difference % step == 0;
        }

        return difference <= 0 && difference % step == 0;
    }

    std::string CSSNthExpression::ToString() const
    {
        if (step == 0)
        {
            return std::to_string(offset);
        }

        std::string text = std::to_string(step) + "n";

        if (offset != 0)
        {
            text += offset > 0 ? "+" : "";
            text += std::to_string(offset);
        }

        return text;
    }

    // -----------------------------------------------------------------------
    // Selector shapes
    // -----------------------------------------------------------------------

    bool CSSSimpleSelector::IsRuntimeState() const
    {
        if (kind != Kind::PseudoClass)
        {
            return false;
        }

        switch (pseudoClass)
        {
        case CSSPseudoClass::Hover:
        case CSSPseudoClass::Active:
        case CSSPseudoClass::Focus:
        case CSSPseudoClass::FocusVisible:
        case CSSPseudoClass::FocusWithin:
        case CSSPseudoClass::Disabled:
        case CSSPseudoClass::Enabled:
        case CSSPseudoClass::Checked:
        case CSSPseudoClass::Required:
        case CSSPseudoClass::Optional:
        case CSSPseudoClass::ReadOnly:
        case CSSPseudoClass::ReadWrite:
        case CSSPseudoClass::Open:
        case CSSPseudoClass::Closed:
        case CSSPseudoClass::PlaceholderShown:
        case CSSPseudoClass::Indeterminate:
            return true;
        default:
            return false;
        }
    }

    std::string CSSSimpleSelector::ToString() const
    {
        switch (kind)
        {
        case Kind::Universal:
            return "*";
        case Kind::Type:
            return name;
        case Kind::Class:
            return "." + name;
        case Kind::Id:
            return "#" + name;
        case Kind::Attribute:
        {
            std::string text = "[" + name;
            switch (attributeOperator)
            {
            case CSSAttributeOperator::Exists: break;
            case CSSAttributeOperator::Equals: text += "="; break;
            case CSSAttributeOperator::Includes: text += "~="; break;
            case CSSAttributeOperator::DashMatch: text += "|="; break;
            case CSSAttributeOperator::Prefix: text += "^="; break;
            case CSSAttributeOperator::Suffix: text += "$="; break;
            case CSSAttributeOperator::Substring: text += "*="; break;
            }
            if (attributeOperator != CSSAttributeOperator::Exists)
            {
                text += "\"" + value + "\"";
            }
            return text + "]";
        }
        case Kind::PseudoClass:
            return ":" + pseudoName;
        case Kind::PseudoElement:
            return "::" + name;
        }

        return "";
    }

    bool CSSCompoundSelector::NeedsRuntimeState() const
    {
        for (const CSSSimpleSelector& simple : simple)
        {
            if (simple.IsRuntimeState())
            {
                return true;
            }

            for (const std::vector<CSSSimpleSelector>& list : simple.argumentLists)
            {
                for (const CSSSimpleSelector& inner : list)
                {
                    if (inner.IsRuntimeState())
                    {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool CSSCompoundSelector::HasTypeOrUniversal() const
    {
        for (const CSSSimpleSelector& simple : simple)
        {
            if (simple.kind == CSSSimpleSelector::Kind::Type || simple.kind == CSSSimpleSelector::Kind::Universal)
            {
                return true;
            }
        }

        return false;
    }

    std::string CSSCompoundSelector::ToString() const
    {
        std::string text;

        for (const CSSSimpleSelector& simple : simple)
        {
            text += simple.ToString();
        }

        return text;
    }

    std::string CSSComplexSelector::ToString() const
    {
        std::string text;

        for (std::size_t index = 0; index < compounds.size(); ++index)
        {
            if (index > 0 && index - 1 < combinators.size())
            {
                text += RoboCSSCompiler::ToString(combinators[index - 1]);
            }

            text += compounds[index].ToString();
        }

        return text;
    }

    bool CSSSelectorList::AnyUnsupported() const
    {
        for (const CSSComplexSelector& selector : selectors)
        {
            if (!selector.supported)
            {
                return true;
            }
        }

        return false;
    }

    std::string CSSSelectorList::ToString() const
    {
        std::string text;

        for (std::size_t index = 0; index < selectors.size(); ++index)
        {
            if (index > 0)
            {
                text += ", ";
            }

            text += selectors[index].ToString();
        }

        return text;
    }

    // -----------------------------------------------------------------------
    // Parsing
    // -----------------------------------------------------------------------

    namespace
    {
        std::string LowerCase(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](char character) {
                return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            });
            return value;
        }

        struct PseudoClassRecord
        {
            const char* name;
            CSSPseudoClass pseudoClass;
            bool takesArgument;
            bool takesSelectorArgument;
        };

        const PseudoClassRecord PseudoClasses[] = {
            { "root", CSSPseudoClass::Root, false, false },
            { "first-child", CSSPseudoClass::FirstChild, false, false },
            { "last-child", CSSPseudoClass::LastChild, false, false },
            { "only-child", CSSPseudoClass::OnlyChild, false, false },
            // `an+b`, not a selector: the argument is not a selector list.
            { "nth-child", CSSPseudoClass::NthChild, true, false },
            { "nth-last-child", CSSPseudoClass::NthLastChild, true, false },
            { "first-of-type", CSSPseudoClass::FirstOfType, false, false },
            { "last-of-type", CSSPseudoClass::LastOfType, false, false },
            { "only-of-type", CSSPseudoClass::OnlyOfType, false, false },
            { "nth-of-type", CSSPseudoClass::NthOfType, true, false },
            { "nth-last-of-type", CSSPseudoClass::NthLastOfType, true, false },
            { "empty", CSSPseudoClass::Empty, false, false },
            { "scope", CSSPseudoClass::Scope, false, false },
            { "not", CSSPseudoClass::Not, true, true },
            { "is", CSSPseudoClass::Is, true, true },
            { "where", CSSPseudoClass::Where, true, true },
            { "matches", CSSPseudoClass::Is, true, true },
            { "has", CSSPseudoClass::Has, true, true },
            { "hover", CSSPseudoClass::Hover, false, false },
            { "active", CSSPseudoClass::Active, false, false },
            { "focus", CSSPseudoClass::Focus, false, false },
            { "focus-visible", CSSPseudoClass::FocusVisible, false, false },
            { "focus-within", CSSPseudoClass::FocusWithin, false, false },
            { "disabled", CSSPseudoClass::Disabled, false, false },
            { "enabled", CSSPseudoClass::Enabled, false, false },
            { "checked", CSSPseudoClass::Checked, false, false },
            { "required", CSSPseudoClass::Required, false, false },
            { "optional", CSSPseudoClass::Optional, false, false },
            { "read-only", CSSPseudoClass::ReadOnly, false, false },
            { "read-write", CSSPseudoClass::ReadWrite, false, false },
            { "open", CSSPseudoClass::Open, false, false },
            { "closed", CSSPseudoClass::Closed, false, false },
            { "placeholder-shown", CSSPseudoClass::PlaceholderShown, false, false },
            { "default", CSSPseudoClass::Default, false, false },
            { "indeterminate", CSSPseudoClass::Indeterminate, false, false },
            { "link", CSSPseudoClass::Link, false, false },
            { "visited", CSSPseudoClass::Visited, false, false },
            { "target", CSSPseudoClass::Target, false, false },
            { "any-link", CSSPseudoClass::AnyLink, false, false }
        };

        const PseudoClassRecord* FindPseudoClass(const std::string& name)
        {
            for (const PseudoClassRecord& record : PseudoClasses)
            {
                if (name == record.name)
                {
                    return &record;
                }
            }

            return nullptr;
        }

        /// Parses the `an+b` microsyntax, and the `odd`/`even` keywords.
        bool ParseNth(const std::string& text, CSSNthExpression& expression)
        {
            std::string value = LowerCase(text);
            value.erase(std::remove_if(value.begin(), value.end(),
                                       [](char character) { return std::isspace(static_cast<unsigned char>(character)) != 0; }),
                        value.end());

            if (value.empty())
            {
                return false;
            }

            if (value == "odd")
            {
                expression.step = 2;
                expression.offset = 1;
                return true;
            }

            if (value == "even")
            {
                expression.step = 2;
                expression.offset = 0;
                return true;
            }

            const std::size_t nPosition = value.find('n');

            if (nPosition == std::string::npos)
            {
                char* end = nullptr;
                const long offset = std::strtol(value.c_str(), &end, 10);

                if (end == nullptr || *end != '\0')
                {
                    return false;
                }

                expression.step = 0;
                expression.offset = static_cast<int>(offset);
                return true;
            }

            const std::string stepText = value.substr(0, nPosition);
            const std::string offsetText = value.substr(nPosition + 1);

            if (stepText.empty() || stepText == "+")
            {
                expression.step = 1;
            }
            else if (stepText == "-")
            {
                expression.step = -1;
            }
            else
            {
                char* end = nullptr;
                const long step = std::strtol(stepText.c_str(), &end, 10);

                if (end == nullptr || *end != '\0')
                {
                    return false;
                }

                expression.step = static_cast<int>(step);
            }

            if (offsetText.empty())
            {
                expression.offset = 0;
                return true;
            }

            char* end = nullptr;
            const long offset = std::strtol(offsetText.c_str(), &end, 10);

            if (end == nullptr || *end != '\0')
            {
                return false;
            }

            expression.offset = static_cast<int>(offset);
            return true;
        }
    }

    CSSSelectorParser::CSSSelectorParser(CSSDiagnosticSink& diagnosticSink)
        : diagnostics(diagnosticSink)
    {
    }

    namespace
    {
        /// The whole selector parser, as a free function over a token vector, so
        /// that a nested argument list can reuse it without a second object.
        class SelectorReader
        {
        public:
            SelectorReader(std::vector<CSSToken> tokenList, CSSDiagnosticSink& sink, const CSSSourceLocation& at)
                : tokens(std::move(tokenList))
                , diagnostics(sink)
                , location(at)
            {
            }

            CSSSelectorList ParseList()
            {
                CSSSelectorList list;
                index = 0;

                while (true)
                {
                    SkipWhitespace();

                    if (AtEnd() || Current().type == CSSTokenType::Comma)
                    {
                        if (!AtEnd())
                        {
                            ++index;
                            continue;
                        }
                        break;
                    }

                    CSSComplexSelector selector = ParseComplex();

                    if (!selector.compounds.empty())
                    {
                        selector.text = selector.ToString();
                        list.selectors.push_back(std::move(selector));
                    }

                    SkipWhitespace();

                    if (AtEnd())
                    {
                        break;
                    }

                    if (Current().type != CSSTokenType::Comma)
                    {
                        diagnostics.Error(CSSDiagnosticCode::InvalidSelector,
                                          Current().location,
                                          "Unexpected token in selector list.",
                                          std::string("found '") + Current().text + "'",
                                          "Separate selectors with a comma.",
                                          location.file);
                        return list;
                    }

                    ++index;
                }

                return list;
            }

        private:
            bool AtEnd() const { return index >= tokens.size() || tokens[index].type == CSSTokenType::EndOfFile; }
            const CSSToken& Current() const { return tokens[index]; }
            const CSSToken& Peek(std::size_t ahead) const
            {
                const std::size_t position = index + ahead;
                return position < tokens.size() ? tokens[position] : tokens.back();
            }

            void SkipWhitespace()
            {
                while (!AtEnd() && tokens[index].type == CSSTokenType::Whitespace)
                {
                    ++index;
                }
            }

            void AddSpecificity(CSSComplexSelector& selector, const CSSSimpleSelector& simple)
            {
                switch (simple.kind)
                {
                case CSSSimpleSelector::Kind::Id:
                    selector.idCount += 1;
                    break;
                case CSSSimpleSelector::Kind::Class:
                case CSSSimpleSelector::Kind::Attribute:
                    selector.classCount += 1;
                    break;
                case CSSSimpleSelector::Kind::Type:
                    selector.typeCount += 1;
                    break;
                case CSSSimpleSelector::Kind::PseudoElement:
                    selector.typeCount += 1;
                    break;
                case CSSSimpleSelector::Kind::PseudoClass:
                    // :not()/:is()/:has() take the specificity of their most
                    // specific argument; :where() contributes nothing.
                    if (simple.pseudoClass == CSSPseudoClass::Where)
                    {
                        break;
                    }

                    if (simple.pseudoClass == CSSPseudoClass::Not || simple.pseudoClass == CSSPseudoClass::Is ||
                        simple.pseudoClass == CSSPseudoClass::Has)
                    {
                        std::uint32_t best = 0;
                        std::uint32_t bestClass = 0;
                        std::uint32_t bestType = 0;

                        for (const std::vector<CSSSimpleSelector>& argument : simple.argumentLists)
                        {
                            std::uint32_t ids = 0;
                            std::uint32_t classes = 0;
                            std::uint32_t types = 0;

                            for (const CSSSimpleSelector& inner : argument)
                            {
                                switch (inner.kind)
                                {
                                case CSSSimpleSelector::Kind::Id: ++ids; break;
                                case CSSSimpleSelector::Kind::Class:
                                case CSSSimpleSelector::Kind::Attribute: ++classes; break;
                                case CSSSimpleSelector::Kind::Type:
                                case CSSSimpleSelector::Kind::PseudoElement: ++types; break;
                                default: break;
                                }
                            }

                            if (ids > best || (ids == best && classes > bestClass) ||
                                (ids == best && classes == bestClass && types > bestType))
                            {
                                best = ids;
                                bestClass = classes;
                                bestType = types;
                            }
                        }

                        selector.idCount += best;
                        selector.classCount += bestClass;
                        selector.typeCount += bestType;
                        break;
                    }

                    selector.classCount += 1;
                    break;
                default:
                    break;
                }
            }

            void ApplyRuntimeState(CSSComplexSelector& selector, const CSSSimpleSelector& simple)
            {
                switch (simple.pseudoClass)
                {
                case CSSPseudoClass::Hover: selector.needsHover = true; break;
                case CSSPseudoClass::Active: selector.needsActive = true; break;
                case CSSPseudoClass::Focus:
                case CSSPseudoClass::FocusVisible:
                case CSSPseudoClass::FocusWithin: selector.needsFocus = true; break;
                case CSSPseudoClass::Checked: selector.needsChecked = true; break;
                case CSSPseudoClass::Disabled: selector.needsDisabled = true; break;
                case CSSPseudoClass::Open: selector.needsOpen = true; break;
                default: break;
                }
            }

            void MarkUnsupported(CSSComplexSelector& selector, const CSSToken& token, const std::string& what)
            {
                if (!selector.supported)
                {
                    return;
                }

                selector.supported = false;

                diagnostics.Warning(CSSDiagnosticCode::UnsupportedFeature,
                                    token.location,
                                    "Selector uses a feature this compiler does not implement.",
                                    what,
                                    "The rule was parsed and skipped.",
                                    what);
            }

            /// Consumes a function's argument tokens, returning them as text.
            std::string ConsumeFunctionArguments()
            {
                std::string text;
                int depth = 1;
                ++index; // the function token

                while (!AtEnd())
                {
                    const CSSToken& token = tokens[index];

                    if (token.type == CSSTokenType::RightParen)
                    {
                        if (depth == 1)
                        {
                            ++index;
                            break;
                        }
                        --depth;
                    }
                    else if (token.type == CSSTokenType::Function || token.type == CSSTokenType::LeftParen)
                    {
                        ++depth;
                    }

                    if (!token.raw.empty())
                    {
                        text += token.raw;
                    }

                    ++index;
                }

                while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
                {
                    text.pop_back();
                }

                return text;
            }

            CSSSimpleSelector ParsePseudo(bool pseudoElement, CSSComplexSelector& selector)
            {
                CSSSimpleSelector simple;
                simple.kind = pseudoElement ? CSSSimpleSelector::Kind::PseudoElement
                                            : CSSSimpleSelector::Kind::PseudoClass;

                // `:name` or `:name(...)`.
                if (Current().type == CSSTokenType::Colon)
                {
                    ++index;
                    SkipWhitespace();
                }

                if (AtEnd() || Current().type != CSSTokenType::Ident)
                {
                    if (Current().type == CSSTokenType::Function)
                    {
                        const CSSToken functionToken = Current();
                        const std::string name = LowerCase(functionToken.text);
                        const std::string arguments = ConsumeFunctionArguments();
                        simple.pseudoName = name;
                        simple.name = name;

                        const PseudoClassRecord* record = FindPseudoClass(name);
                        if (record == nullptr)
                        {
                            simple.pseudoClass = CSSPseudoClass::Unknown;
                            MarkUnsupported(selector, functionToken, ":" + name);
                            return simple;
                        }

                        simple.pseudoClass = record->pseudoClass;
                        ApplyRuntimeState(selector, simple);

                        if (record->takesSelectorArgument)
                        {
                            SelectorReader reader(SplitTokens(arguments, functionToken), diagnostics, location);
                            const CSSSelectorList list = reader.ParseList();

                            for (const CSSComplexSelector& argument : list.selectors)
                            {
                                std::vector<CSSSimpleSelector> flat;
                                for (const CSSCompoundSelector& compound : argument.compounds)
                                {
                                    for (const CSSSimpleSelector& inner : compound.simple)
                                    {
                                        flat.push_back(inner);
                                    }
                                }

                                simple.argumentLists.push_back(flat);

                                if (!argument.supported)
                                {
                                    selector.supported = false;
                                }
                            }
                        }
                        else if (record->takesArgument)
                        {
                            const std::size_t ofPosition = arguments.find(" of ");
                            const std::string nthText =
                                ofPosition == std::string::npos ? arguments : arguments.substr(0, ofPosition);

                            if (!ParseNth(nthText, simple.nth) &&
                                !ParseNth(LowerCase(arguments), simple.nth))
                            {
                                MarkUnsupported(selector, functionToken, ":" + name + "(" + arguments + ")");
                            }

                            if (ofPosition != std::string::npos)
                            {
                                // `of S` narrows the sibling set to elements that
                                // match S, which needs the matching tree at
                                // runtime; it is reported rather than guessed.
                                MarkUnsupported(selector, functionToken,
                                                ":" + name + "(... of ...)");
                            }
                        }

                        return simple;
                    }

                    return simple;
                }

                const CSSToken nameToken = Current();
                const std::string name = LowerCase(nameToken.text);
                ++index;

                simple.name = name;
                simple.pseudoName = name;

                const PseudoClassRecord* record = FindPseudoClass(name);

                if (record == nullptr)
                {
                    simple.pseudoClass = CSSPseudoClass::Unknown;
                    MarkUnsupported(selector, nameToken, (pseudoElement ? "::" : ":") + name);
                    return simple;
                }

                simple.pseudoClass = record->pseudoClass;
                ApplyRuntimeState(selector, simple);

                if (record->takesArgument)
                {
                    // A functional pseudo-class without parentheses is invalid.
                    diagnostics.Error(CSSDiagnosticCode::InvalidSelector,
                                      nameToken.location,
                                      "Pseudo-class ':" + name + "' requires an argument.",
                                      "the parentheses are missing",
                                      "Write ':" + name + "(...)'.",
                                      name);
                    selector.supported = false;
                }

                return simple;
            }

            std::vector<CSSToken> SplitTokens(const std::string& text, const CSSToken& at)
            {
                CSSTokenizer tokenizer(text, at.location.file, nullptr);
                return tokenizer.Tokenize();
            }

            CSSSimpleSelector ParseAttribute(CSSComplexSelector& selector)
            {
                CSSSimpleSelector simple;
                simple.kind = CSSSimpleSelector::Kind::Attribute;

                const CSSToken opener = Current();
                ++index;

                SkipWhitespace();

                if (AtEnd() || Current().type != CSSTokenType::Ident)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidSelector,
                                      opener.location,
                                      "Attribute selector has no attribute name.",
                                      "expected an identifier after '['",
                                      "Write '[name]', '[name=value]' or similar.",
                                      opener.text);
                    selector.supported = false;
                    return simple;
                }

                simple.name = LowerCase(Current().text);
                ++index;
                SkipWhitespace();

                CSSAttributeOperator attributeOperator = CSSAttributeOperator::Exists;

                if (!AtEnd() && Current().type == CSSTokenType::Delim)
                {
                    const std::string& spelling = Current().text;

                    if (spelling == "~" || spelling == "|" || spelling == "^" || spelling == "$" ||
                        spelling == "*")
                    {
                        if (Peek(1).IsDelim('='))
                        {
                            if (spelling == "~") attributeOperator = CSSAttributeOperator::Includes;
                            else if (spelling == "|") attributeOperator = CSSAttributeOperator::DashMatch;
                            else if (spelling == "^") attributeOperator = CSSAttributeOperator::Prefix;
                            else if (spelling == "$") attributeOperator = CSSAttributeOperator::Suffix;
                            else attributeOperator = CSSAttributeOperator::Substring;

                            index += 2;
                        }
                    }
                }
                else if (!AtEnd() && Current().type == CSSTokenType::Delim && Current().text == "=")
                {
                    attributeOperator = CSSAttributeOperator::Equals;
                    ++index;
                }

                if (attributeOperator == CSSAttributeOperator::Exists && !AtEnd() && Current().IsDelim('='))
                {
                    attributeOperator = CSSAttributeOperator::Equals;
                    ++index;
                }

                simple.attributeOperator = attributeOperator;

                if (attributeOperator != CSSAttributeOperator::Exists)
                {
                    SkipWhitespace();

                    if (!AtEnd() && (Current().type == CSSTokenType::QuotedString ||
                                     Current().type == CSSTokenType::Ident))
                    {
                        simple.value = Current().text;
                        ++index;
                    }
                    else
                    {
                        diagnostics.Error(CSSDiagnosticCode::InvalidSelector,
                                          opener.location,
                                          "Attribute selector has no value.",
                                          "expected a string or identifier after the operator",
                                          "Write '[name=\"value\"]'.",
                                          simple.name);
                        selector.supported = false;
                    }

                    SkipWhitespace();

                    // The case-sensitivity flag.
                    if (!AtEnd() && Current().type == CSSTokenType::Ident)
                    {
                        const std::string flag = LowerCase(Current().text);

                        if (flag == "i")
                        {
                            simple.attributeCaseInsensitive = true;
                            ++index;
                        }
                        else if (flag == "s")
                        {
                            ++index;
                        }
                    }
                }

                SkipWhitespace();

                if (AtEnd() || Current().type != CSSTokenType::RightSquare)
                {
                    diagnostics.Error(CSSDiagnosticCode::InvalidSelector,
                                      opener.location,
                                      "Attribute selector is not closed.",
                                      "expected ']'",
                                      "Add the missing ']'.",
                                      simple.name);
                    selector.supported = false;
                }
                else
                {
                    ++index;
                }

                return simple;
            }

            CSSCompoundSelector ParseCompound(CSSComplexSelector& selector)
            {
                CSSCompoundSelector compound;

                while (!AtEnd())
                {
                    const CSSToken& token = Current();

                    if (token.type == CSSTokenType::Whitespace || token.type == CSSTokenType::Comma)
                    {
                        break;
                    }

                    if (token.IsDelim('>') || token.IsDelim('+') || token.IsDelim('~'))
                    {
                        break;
                    }

                    if (token.IsDelim('|'))
                    {
                        // Namespace prefix, and `||` the column combinator.
                        if (Peek(1).IsDelim('|'))
                        {
                            MarkUnsupported(selector, token, "||");
                            index += 2;
                            continue;
                        }

                        index += 1;
                        continue;
                    }

                    if (token.IsDelim('*'))
                    {
                        CSSSimpleSelector simple;
                        simple.kind = CSSSimpleSelector::Kind::Universal;
                        compound.simple.push_back(simple);
                        ++index;
                        continue;
                    }

                    if (token.type == CSSTokenType::Ident)
                    {
                        CSSSimpleSelector simple;
                        simple.kind = CSSSimpleSelector::Kind::Type;
                        simple.name = LowerCase(token.text);
                        compound.simple.push_back(simple);
                        ++index;
                        continue;
                    }

                    if (token.IsDelim('.'))
                    {
                        ++index;

                        if (AtEnd() || Current().type != CSSTokenType::Ident)
                        {
                            diagnostics.Error(CSSDiagnosticCode::InvalidSelector,
                                              token.location,
                                              "Class selector has no name.",
                                              "expected an identifier after '.'",
                                              "Write '.name'.",
                                              ".");
                            selector.supported = false;
                            continue;
                        }

                        CSSSimpleSelector simple;
                        simple.kind = CSSSimpleSelector::Kind::Class;
                        simple.name = Current().text;
                        compound.simple.push_back(simple);
                        ++index;
                        continue;
                    }

                    if (token.type == CSSTokenType::Hash)
                    {
                        CSSSimpleSelector simple;
                        simple.kind = CSSSimpleSelector::Kind::Id;
                        simple.name = token.text;
                        compound.simple.push_back(simple);
                        ++index;
                        continue;
                    }

                    if (token.type == CSSTokenType::LeftSquare)
                    {
                        compound.simple.push_back(ParseAttribute(selector));
                        continue;
                    }

                    if (token.type == CSSTokenType::Colon)
                    {
                        // `::name` is a pseudo-element, `:name` is a
                        // pseudo-class.
                        const bool isPseudoElement = Peek(1).type == CSSTokenType::Colon;
                        ++index;

                        CSSSimpleSelector simple = ParsePseudo(isPseudoElement, selector);

                        if (simple.kind == CSSSimpleSelector::Kind::PseudoElement)
                        {
                            // Only ::before and ::after have a native
                            // representation, and only as generated content the
                            // paint engine draws.
                            if (simple.name != "before" && simple.name != "after" && simple.name != "marker")
                            {
                                MarkUnsupported(selector, token, "::" + simple.name);
                            }
                            else if (simple.name == "marker")
                            {
                                MarkUnsupported(selector, token, "::marker");
                            }
                        }

                        if (simple.kind == CSSSimpleSelector::Kind::PseudoClass &&
                            simple.pseudoClass == CSSPseudoClass::Unknown)
                        {
                            // Already reported.
                        }

                        if (!simple.name.empty() || simple.pseudoClass == CSSPseudoClass::Unknown)
                        {
                            compound.simple.push_back(simple);
                        }

                        continue;
                    }

                    // Anything else ends the compound.
                    break;
                }

                for (const CSSSimpleSelector& simple : compound.simple)
                {
                    switch (simple.kind)
                    {
                    case CSSSimpleSelector::Kind::Id: compound.idCount += 1; break;
                    case CSSSimpleSelector::Kind::Class:
                    case CSSSimpleSelector::Kind::Attribute: compound.classCount += 1; break;
                    case CSSSimpleSelector::Kind::Type:
                    case CSSSimpleSelector::Kind::PseudoElement: compound.typeCount += 1; break;
                    default: break;
                    }
                }

                return compound;
            }

            CSSComplexSelector ParseComplex()
            {
                CSSComplexSelector selector;

                while (true)
                {
                    SkipWhitespace();

                    if (AtEnd() || Current().type == CSSTokenType::Comma)
                    {
                        break;
                    }

                    const CSSCompoundSelector compound = ParseCompound(selector);

                    if (compound.IsEmpty())
                    {
                        if (!AtEnd() && Current().type != CSSTokenType::Comma)
                        {
                            diagnostics.Error(CSSDiagnosticCode::InvalidSelector,
                                              Current().location,
                                              "Unexpected token in selector.",
                                              std::string("found '") + Current().text + "'",
                                              "Remove the token, or write a valid simple selector.",
                                              location.file);
                            selector.supported = false;
                            ++index;
                            continue;
                        }

                        break;
                    }

                    for (const CSSSimpleSelector& simple : compound.simple)
                    {
                        AddSpecificity(selector, simple);
                    }

                    selector.compounds.push_back(compound);

                    // The combinator between this compound and the next.
                    const std::size_t beforeWhitespace = index;
                    bool sawWhitespace = false;

                    while (!AtEnd() && Current().type == CSSTokenType::Whitespace)
                    {
                        sawWhitespace = true;
                        ++index;
                    }

                    if (AtEnd() || Current().type == CSSTokenType::Comma)
                    {
                        index = beforeWhitespace;
                        break;
                    }

                    if (Current().IsDelim('>'))
                    {
                        selector.combinators.push_back(CSSCombinator::Child);
                        ++index;
                    }
                    else if (Current().IsDelim('+'))
                    {
                        selector.combinators.push_back(CSSCombinator::NextSibling);
                        ++index;
                    }
                    else if (Current().IsDelim('~'))
                    {
                        selector.combinators.push_back(CSSCombinator::SubsequentSibling);
                        ++index;
                    }
                    else if (sawWhitespace)
                    {
                        selector.combinators.push_back(CSSCombinator::Descendant);
                    }
                    else
                    {
                        // Two compounds with nothing between them: a parse error
                        // that the compound parser already stopped at.
                        diagnostics.Error(CSSDiagnosticCode::InvalidSelector,
                                          Current().location,
                                          "Unexpected token between simple selectors.",
                                          std::string("found '") + Current().text + "'",
                                          "Add a combinator, or remove the token.",
                                          location.file);
                        selector.supported = false;
                        ++index;
                    }
                }

                // `combinators` is one shorter than `compounds`.
                if (selector.combinators.size() + 1 != selector.compounds.size() &&
                    !selector.compounds.empty())
                {
                    selector.combinators.resize(selector.compounds.size() > 0 ? selector.compounds.size() - 1 : 0);
                }

                return selector;
            }

            std::vector<CSSToken> tokens;
            CSSDiagnosticSink& diagnostics;
            CSSSourceLocation location;
            std::size_t index = 0;
        };
    }

    CSSSelectorList CSSSelectorParser::Parse(const std::string& selectorText, const CSSSourceLocation& location)
    {
        CSSTokenizer tokenizer(selectorText, location.file, &diagnostics);
        std::vector<CSSToken> tokens = tokenizer.Tokenize();

        SelectorReader reader(std::move(tokens), diagnostics, location);
        return reader.ParseList();
    }

    // -----------------------------------------------------------------------
    // Matching
    // -----------------------------------------------------------------------

    namespace
    {
        bool IsAncestorOf(const CSSElementNode& candidate, const CSSElementNode& node)
        {
            for (const CSSElementNode* parent = node.parent; parent != nullptr; parent = parent->parent)
            {
                if (parent == &candidate)
                {
                    return true;
                }
            }

            return false;
        }

        /// The state that applies to one element while matching a state variant.
        ///
        /// A hovered element's ancestors are hovered too, which is what
        /// `.card:hover .label` needs; the styles are computed for one element at
        /// a time, so the target of the state is the only element the caller
        /// names and the ancestors are derived from it.
        struct MatchContext
        {
            const CSSElementNode* target = nullptr;
            CSSElementState state;
            bool hasOverride = false;

            CSSElementState StateOf(const CSSElementNode& element) const
            {
                if (!hasOverride)
                {
                    return element.state;
                }

                if (&element == target)
                {
                    return state;
                }

                if (IsAncestorOf(element, *target))
                {
                    CSSElementState inherited = element.state;
                    inherited.hovered = inherited.hovered || state.hovered;
                    inherited.active = inherited.active || state.active;
                    inherited.focusWithin = inherited.focusWithin || state.focused;
                    return inherited;
                }

                return element.state;
            }
        };

        bool MatchAttribute(const CSSSimpleSelector& simple, const CSSElementNode& element)
        {
            const std::string* value = element.Attribute(simple.name);

            if (simple.attributeOperator == CSSAttributeOperator::Exists)
            {
                return value != nullptr;
            }

            if (value == nullptr)
            {
                return false;
            }

            std::string actual = *value;
            std::string expected = simple.value;

            if (simple.attributeCaseInsensitive)
            {
                std::transform(actual.begin(), actual.end(), actual.begin(),
                               [](char character) { return static_cast<char>(std::tolower(static_cast<unsigned char>(character))); });
                std::transform(expected.begin(), expected.end(), expected.begin(),
                               [](char character) { return static_cast<char>(std::tolower(static_cast<unsigned char>(character))); });
            }

            switch (simple.attributeOperator)
            {
            case CSSAttributeOperator::Equals:
                return actual == expected;

            case CSSAttributeOperator::Includes:
            {
                std::size_t position = 0;
                while (position <= actual.size())
                {
                    const std::size_t space = actual.find(' ', position);
                    const std::string item = actual.substr(
                        position, (space == std::string::npos ? actual.size() : space) - position);

                    if (item == expected)
                    {
                        return true;
                    }

                    if (space == std::string::npos)
                    {
                        break;
                    }
                    position = space + 1;
                }

                return false;
            }

            case CSSAttributeOperator::DashMatch:
                return actual == expected ||
                       (actual.size() > expected.size() && actual.compare(0, expected.size(), expected) == 0 &&
                        actual[expected.size()] == '-');

            case CSSAttributeOperator::Prefix:
                return !expected.empty() && actual.size() >= expected.size() &&
                       actual.compare(0, expected.size(), expected) == 0;

            case CSSAttributeOperator::Suffix:
                return !expected.empty() && actual.size() >= expected.size() &&
                       actual.compare(actual.size() - expected.size(), expected.size(), expected) == 0;

            case CSSAttributeOperator::Substring:
                return !expected.empty() && actual.find(expected) != std::string::npos;

            case CSSAttributeOperator::Exists:
                break;
            }

            return false;
        }

        bool MatchNthPseudo(const CSSSimpleSelector& simple, const CSSElementNode& element)
        {
            switch (simple.pseudoClass)
            {
            case CSSPseudoClass::NthChild:
                return simple.nth.Matches(element.siblingIndex);
            case CSSPseudoClass::NthLastChild:
                return simple.nth.Matches(element.siblingCount - element.siblingIndex + 1);
            case CSSPseudoClass::NthOfType:
                return simple.nth.Matches(element.typeIndex);
            case CSSPseudoClass::NthLastOfType:
                return simple.nth.Matches(element.typeCount - element.typeIndex + 1);
            default:
                return false;
            }
        }

        /// One simple selector against one element and its state.
        bool MatchSimple(const CSSSimpleSelector& simple,
                         const CSSElementNode& element,
                         const CSSElementState& state)
        {
            switch (simple.kind)
            {
            case CSSSimpleSelector::Kind::Universal:
                return true;

            case CSSSimpleSelector::Kind::Type:
                return element.tagName == simple.name ||
                       (simple.namespaceName == "html" && element.tagName == simple.name);

            case CSSSimpleSelector::Kind::Class:
                return element.HasClass(simple.name);

            case CSSSimpleSelector::Kind::Id:
                return element.id == simple.name;

            case CSSSimpleSelector::Kind::Attribute:
                return MatchAttribute(simple, element);

            case CSSSimpleSelector::Kind::PseudoElement:
                // A pseudo-element selector does not match the element itself:
                // the generated box is a separate box the paint engine draws.
                return true;

            case CSSSimpleSelector::Kind::PseudoClass:
                break;
            }

            switch (simple.pseudoClass)
            {
            case CSSPseudoClass::Root:
                return element.parent == nullptr;

            case CSSPseudoClass::FirstChild:
                return element.siblingIndex == 1;

            case CSSPseudoClass::LastChild:
                return element.siblingIndex == element.siblingCount;

            case CSSPseudoClass::OnlyChild:
                return element.siblingCount == 1;

            case CSSPseudoClass::FirstOfType:
                return element.typeIndex == 1;

            case CSSPseudoClass::LastOfType:
                return element.typeIndex == element.typeCount;

            case CSSPseudoClass::OnlyOfType:
                return element.typeCount == 1;

            case CSSPseudoClass::NthChild:
            case CSSPseudoClass::NthLastChild:
            case CSSPseudoClass::NthOfType:
            case CSSPseudoClass::NthLastOfType:
                return MatchNthPseudo(simple, element);

            case CSSPseudoClass::Empty:
                // No element children and no non-whitespace text.
                if (!element.children.empty())
                {
                    return false;
                }
                for (char character : element.textContent)
                {
                    if (std::isspace(static_cast<unsigned char>(character)) == 0)
                    {
                        return false;
                    }
                }
                return true;

            case CSSPseudoClass::Scope:
                return element.parent == nullptr;

            case CSSPseudoClass::Not:
            case CSSPseudoClass::Is:
            case CSSPseudoClass::Where:
            {
                bool any = false;

                for (const std::vector<CSSSimpleSelector>& argument : simple.argumentLists)
                {
                    bool all = true;

                    for (const CSSSimpleSelector& inner : argument)
                    {
                        if (!MatchSimple(inner, element, state))
                        {
                            all = false;
                            break;
                        }
                    }

                    if (all)
                    {
                        any = true;
                        break;
                    }
                }

                return simple.pseudoClass == CSSPseudoClass::Not ? !any : any;
            }

            case CSSPseudoClass::Has:
                // :has() needs a descendant walk with the same state; the parser
                // already marked it unsupported, and a rule that is unsupported is
                // never applied.
                return false;

            case CSSPseudoClass::Hover:
                return state.hovered;
            case CSSPseudoClass::Active:
                return state.active;
            case CSSPseudoClass::Focus:
            case CSSPseudoClass::FocusVisible:
                return state.focused;
            case CSSPseudoClass::FocusWithin:
                return state.focused || state.focusWithin;
            case CSSPseudoClass::Disabled:
                return state.disabled;
            case CSSPseudoClass::Enabled:
                return state.enabled && !state.disabled;
            case CSSPseudoClass::Checked:
                return state.checked;
            case CSSPseudoClass::Required:
                return state.required;
            case CSSPseudoClass::Optional:
                return !state.required;
            case CSSPseudoClass::ReadOnly:
                return state.readOnly;
            case CSSPseudoClass::ReadWrite:
                return !state.readOnly;
            case CSSPseudoClass::Open:
                return state.open;
            case CSSPseudoClass::Closed:
                return !state.open;
            case CSSPseudoClass::PlaceholderShown:
                return state.placeholderShown;
            case CSSPseudoClass::Default:
                return state.checked;
            case CSSPseudoClass::Indeterminate:
                return false;
            case CSSPseudoClass::Link:
            case CSSPseudoClass::AnyLink:
                return state.link;
            case CSSPseudoClass::Visited:
                return false;
            case CSSPseudoClass::Target:
                return false;
            case CSSPseudoClass::Unknown:
                return false;
            }

            return false;
        }

        bool MatchCompound(const CSSCompoundSelector& compound,
                           const CSSElementNode& element,
                           const CSSElementState& state)
        {
            for (const CSSSimpleSelector& simple : compound.simple)
            {
                if (!MatchSimple(simple, element, state))
                {
                    return false;
                }
            }

            return true;
        }

        bool MatchFrom(const CSSComplexSelector& selector,
                       std::size_t index,
                       const CSSElementNode& element,
                       const MatchContext& context)
        {
            if (!MatchCompound(selector.compounds[index], element, context.StateOf(element)))
            {
                return false;
            }

            if (index == 0)
            {
                return true;
            }

            const CSSCombinator combinator =
                index - 1 < selector.combinators.size() ? selector.combinators[index - 1] : CSSCombinator::Descendant;

            switch (combinator)
            {
            case CSSCombinator::Child:
                return element.parent != nullptr && MatchFrom(selector, index - 1, *element.parent, context);

            case CSSCombinator::Descendant:
                for (const CSSElementNode* parent = element.parent; parent != nullptr; parent = parent->parent)
                {
                    if (MatchFrom(selector, index - 1, *parent, context))
                    {
                        return true;
                    }
                }
                return false;

            case CSSCombinator::NextSibling:
            case CSSCombinator::SubsequentSibling:
            {
                if (element.parent == nullptr || element.siblingIndex < 2)
                {
                    return false;
                }

                const std::vector<CSSElementNode*>& siblings = element.parent->children;

                for (std::size_t sibling = element.siblingIndex - 1; sibling-- > 0;)
                {
                    if (sibling >= siblings.size())
                    {
                        break;
                    }

                    if (MatchFrom(selector, index - 1, *siblings[sibling], context))
                    {
                        return true;
                    }

                    if (combinator == CSSCombinator::NextSibling)
                    {
                        break;
                    }
                }

                return false;
            }

            case CSSCombinator::None:
                break;
            }

            return false;
        }
    }

    bool MatchSelector(const CSSComplexSelector& selector,
                       const CSSElementNode& element,
                       const CSSElementState* runtimeState)
    {
        if (selector.compounds.empty() || !selector.supported)
        {
            return false;
        }

        MatchContext context;

        if (runtimeState != nullptr)
        {
            context.hasOverride = true;
            context.state = *runtimeState;
            context.target = &element;
        }

        return MatchFrom(selector, selector.compounds.size() - 1, element, context);
    }

    CSSElementState StateForSelector(const CSSComplexSelector& selector)
    {
        if (selector.needsHover)
        {
            return CSSElementState::Hover();
        }
        if (selector.needsActive)
        {
            return CSSElementState::Active();
        }
        if (selector.needsFocus)
        {
            return CSSElementState::Focus();
        }
        if (selector.needsChecked)
        {
            return CSSElementState::Checked();
        }
        if (selector.needsDisabled)
        {
            return CSSElementState::Disabled();
        }
        if (selector.needsOpen)
        {
            return CSSElementState::Open();
        }

        return CSSElementState::None();
    }
}
