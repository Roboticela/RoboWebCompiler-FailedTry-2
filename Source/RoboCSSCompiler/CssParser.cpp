#include "RoboCSSCompiler/CssParser.h"

#include <algorithm>
#include <cctype>

namespace RoboCSSCompiler
{
    namespace
    {
        /// At-rules whose block is a declaration list rather than a rule list.
        bool AtRuleBlockIsDeclarations(const std::string& name)
        {
            return name == "font-face" || name == "page" || name == "property" || name == "counter-style" ||
                   name == "viewport" || name == "font-feature-values" || name == "font-palette-values" ||
                   name == "color-profile" || name == "position-try";
        }

        /// At-rules the parser knows, whether or not the compiler implements
        /// them. An at-rule the parser does not know is a syntax error; a known
        /// one it does not implement is an unsupported feature.
        bool IsKnownAtRule(const std::string& name)
        {
            return name == "media" || name == "supports" || name == "import" || name == "font-face" ||
                   name == "keyframes" || name == "layer" || name == "container" || name == "namespace" ||
                   name == "page" || name == "property" || name == "scope" || name == "starting-style" ||
                   name == "counter-style" || name == "charset" || name == "font-feature-values" ||
                   name == "viewport" || name == "color-profile" || name == "custom-media" || name == "nest";
        }

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

        /// Splits a selector list on the commas that are not inside a function or
        /// a block, which is what `:is(.a, .b)` needs.
        std::vector<std::string> SplitTopLevel(const std::string& text, char separator)
        {
            std::vector<std::string> parts;
            int depth = 0;
            std::size_t start = 0;
            bool inString = false;
            char quote = '\0';

            for (std::size_t index = 0; index < text.size(); ++index)
            {
                const char character = text[index];

                if (inString)
                {
                    if (character == '\\')
                    {
                        ++index;
                    }
                    else if (character == quote)
                    {
                        inString = false;
                    }
                    continue;
                }

                if (character == '"' || character == '\'')
                {
                    inString = true;
                    quote = character;
                    continue;
                }

                if (character == '(' || character == '[')
                {
                    ++depth;
                }
                else if (character == ')' || character == ']')
                {
                    if (depth > 0)
                    {
                        --depth;
                    }
                }
                else if (character == separator && depth == 0)
                {
                    const std::string part = Trim(text.substr(start, index - start));
                    if (!part.empty())
                    {
                        parts.push_back(part);
                    }
                    start = index + 1;
                }
            }

            const std::string last = Trim(text.substr(start));
            if (!last.empty())
            {
                parts.push_back(last);
            }

            return parts;
        }
    }

    CSSParser::CSSParser(CSSDiagnosticSink& diagnosticSink)
        : diagnostics(diagnosticSink)
    {
    }

    const CSSToken& CSSParser::TokenAt(std::size_t index) const
    {
        static const CSSToken empty;
        return index < tokens.size() ? tokens[index] : empty;
    }

    std::string CSSParser::TextOf(std::size_t begin, std::size_t end) const
    {
        std::string text;

        for (std::size_t index = begin; index < end && index < tokens.size(); ++index)
        {
            const CSSToken& token = tokens[index];

            if (token.type == CSSTokenType::Whitespace)
            {
                if (!text.empty() && text.back() != ' ')
                {
                    text += ' ';
                }
                continue;
            }

            if (!token.raw.empty())
            {
                text += token.raw;
            }
            else
            {
                text += token.text;
            }
        }

        return Trim(text);
    }

    std::size_t CSSParser::FindBlockEnd(std::size_t begin,
                                        std::size_t end,
                                        CSSTokenType opener,
                                        CSSTokenType closer) const
    {
        int depth = 0;

        for (std::size_t index = begin; index < end; ++index)
        {
            const CSSToken& token = tokens[index];

            if (token.type == opener)
            {
                ++depth;
            }
            else if (token.type == closer)
            {
                if (depth == 0)
                {
                    return index;
                }
                --depth;
            }
        }

        return end;
    }

    CSSComponentValue CSSParser::ConsumeComponentValue(std::size_t& index, std::size_t end)
    {
        CSSComponentValue value;
        value.token = TokenAt(index);
        value.location = value.token.location;
        ++index;

        CSSBlockKind block = CSSBlockKind::None;
        CSSTokenType closer = CSSTokenType::EndOfFile;
        bool function = false;

        switch (value.token.type)
        {
        case CSSTokenType::Function:
            block = CSSBlockKind::Parentheses;
            closer = CSSTokenType::RightParen;
            function = true;
            break;
        case CSSTokenType::LeftParen:
            block = CSSBlockKind::Parentheses;
            closer = CSSTokenType::RightParen;
            break;
        case CSSTokenType::LeftSquare:
            block = CSSBlockKind::Square;
            closer = CSSTokenType::RightSquare;
            break;
        case CSSTokenType::LeftCurly:
            block = CSSBlockKind::Curly;
            closer = CSSTokenType::RightCurly;
            break;
        default:
            return value;
        }

        value.block = block;
        value.isFunction = function;

        int depth = 0;

        while (index < end)
        {
            const CSSToken& token = tokens[index];

            if (token.type == closer && depth == 0)
            {
                ++index;
                return value;
            }

            if (token.type == CSSTokenType::LeftParen || token.type == CSSTokenType::LeftSquare ||
                token.type == CSSTokenType::LeftCurly || token.type == CSSTokenType::Function)
            {
                ++depth;
            }
            else if (token.type == CSSTokenType::RightParen || token.type == CSSTokenType::RightSquare ||
                     token.type == CSSTokenType::RightCurly)
            {
                if (depth > 0)
                {
                    --depth;
                }
            }

            value.children.push_back(ConsumeComponentValue(index, end));
        }

        diagnostics.Error(CSSDiagnosticCode::MalformedStylesheet,
                          value.location,
                          std::string("Unclosed ") + ToString(value.token.type) + " block.",
                          "the block is never closed before the end of the stylesheet",
                          "Add the missing closing token.");

        return value;
    }

    std::vector<CSSDeclaration> CSSParser::ParseDeclarations(std::size_t begin, std::size_t end)
    {
        std::vector<CSSDeclaration> declarations;
        std::size_t index = begin;

        while (index < end)
        {
            const CSSToken& token = tokens[index];

            if (token.type == CSSTokenType::Whitespace || token.type == CSSTokenType::Semicolon ||
                token.type == CSSTokenType::EndOfFile)
            {
                ++index;
                continue;
            }

            const CSSSourceLocation declarationLocation = token.location;

            if (token.type != CSSTokenType::Ident)
            {
                // Recover: skip to the next declaration.
                diagnostics.Error(CSSDiagnosticCode::InvalidDeclaration,
                                  declarationLocation,
                                  "A declaration must start with a property name.",
                                  std::string("found ") + ToString(token.type) + " '" + token.text + "'",
                                  "Remove the text, or write it as `property: value;`.");

                while (index < end && tokens[index].type != CSSTokenType::Semicolon)
                {
                    ++index;
                }
                continue;
            }

            CSSDeclaration declaration;
            declaration.property = token.text;
            declaration.location = declarationLocation;
            declaration.isCustomProperty = declaration.property.rfind("--", 0) == 0;

            if (!declaration.isCustomProperty)
            {
                declaration.property = LowerCase(declaration.property);
            }

            ++index;

            // Custom properties keep their name's case and their value's tokens,
            // so the check for a colon is shared but nothing else is.
            while (index < end && tokens[index].type == CSSTokenType::Whitespace)
            {
                ++index;
            }

            if (index >= end || tokens[index].type != CSSTokenType::Colon)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidDeclaration,
                                  declarationLocation,
                                  "Missing ':' after the property name.",
                                  std::string("property '") + declaration.property + "'",
                                  "Write the declaration as `property: value;`.",
                                  declaration.property);

                while (index < end && tokens[index].type != CSSTokenType::Semicolon)
                {
                    ++index;
                }
                continue;
            }

            ++index;

            const std::size_t valueBegin = index;

            while (index < end && tokens[index].type != CSSTokenType::Semicolon &&
                   tokens[index].type != CSSTokenType::EndOfFile)
            {
                ++index;
            }

            const std::size_t valueEnd = index;

            // "Has a value" is asked once per declaration, so it is answered
            // without building the value's text: a whitespace-only range is an
            // empty value, and the text itself is recoverable from the component
            // values that are about to be parsed out of the same tokens.
            bool hasValue = false;
            for (std::size_t scan = valueBegin; scan < valueEnd; ++scan)
            {
                if (tokens[scan].type != CSSTokenType::Whitespace && !tokens[scan].raw.empty())
                {
                    hasValue = true;
                    break;
                }
            }

            if (!hasValue)
            {
                diagnostics.Error(CSSDiagnosticCode::InvalidValue,
                                  declarationLocation,
                                  "The declaration has no value.",
                                  "expected a value after ':'",
                                  "Remove the declaration, or write a value.",
                                  declaration.property);
                continue;
            }

            // `!important` is the last thing in a declaration, and only ever
            // appears once.
            std::size_t effectiveBegin = valueBegin;
            std::size_t effectiveEnd = valueEnd;
            std::size_t scan = valueEnd;

            while (scan > valueBegin && tokens[scan - 1].type == CSSTokenType::Whitespace)
            {
                --scan;
            }

            if (scan > valueBegin && tokens[scan - 1].type == CSSTokenType::Ident &&
                LowerCase(tokens[scan - 1].text) == "important")
            {
                std::size_t bang = scan - 1;
                while (bang > valueBegin && tokens[bang - 1].type == CSSTokenType::Whitespace)
                {
                    --bang;
                }

                if (bang > valueBegin && tokens[bang - 1].IsDelim('!'))
                {
                    declaration.important = true;
                    effectiveEnd = bang - 1;
                }
            }

            std::size_t consume = effectiveBegin;
            while (consume < effectiveEnd)
            {
                declaration.value.push_back(ConsumeComponentValue(consume, effectiveEnd));
            }

            declarations.push_back(std::move(declaration));
        }

        return declarations;
    }

    std::vector<CSSRule> CSSParser::ParseRuleList(std::size_t begin, std::size_t end, bool declarationBlockExpected)
    {
        std::vector<CSSRule> rules;
        std::size_t index = begin;

        while (index < end)
        {
            const CSSToken& token = tokens[index];

            // End of input is where the rule list stops, not text a rule is
            // waiting to close: an unterminated `p` at the end of a file is a
            // truncated rule, and one at the end of a parsed prefix is not a
            // rule at all.
            if (token.type == CSSTokenType::Whitespace || token.type == CSSTokenType::Semicolon ||
                token.type == CSSTokenType::Cdo || token.type == CSSTokenType::Cdc ||
                token.type == CSSTokenType::EndOfFile)
            {
                ++index;
                continue;
            }

            // --- at-rule -----------------------------------------------------
            if (token.type == CSSTokenType::AtKeyword)
            {
                CSSRule rule;
                rule.kind = CSSRule::Kind::At;
                rule.location = token.location;
                rule.at.location = token.location;
                rule.at.name = LowerCase(token.text);
                ++index;

                const std::size_t preludeBegin = index;
                int depth = 0;
                std::size_t preludeEnd = end;

                while (index < end)
                {
                    const CSSTokenType type = tokens[index].type;

                    if (type == CSSTokenType::LeftParen || type == CSSTokenType::LeftSquare ||
                        type == CSSTokenType::Function)
                    {
                        ++depth;
                    }
                    else if (type == CSSTokenType::RightParen || type == CSSTokenType::RightSquare)
                    {
                        if (depth > 0)
                        {
                            --depth;
                        }
                    }
                    else if (depth == 0 && (type == CSSTokenType::Semicolon || type == CSSTokenType::LeftCurly))
                    {
                        break;
                    }

                    ++index;
                }

                preludeEnd = index;
                rule.at.preludeText = TextOf(preludeBegin, preludeEnd);

                std::size_t consume = preludeBegin;
                while (consume < preludeEnd)
                {
                    rule.at.prelude.push_back(ConsumeComponentValue(consume, preludeEnd));
                }

                if (index < end && tokens[index].type == CSSTokenType::LeftCurly)
                {
                    rule.at.hasBlock = true;
                    const std::size_t blockEnd = FindBlockEnd(index + 1, end, CSSTokenType::LeftCurly,
                                                              CSSTokenType::RightCurly);

                    if (blockEnd == end)
                    {
                        diagnostics.Error(CSSDiagnosticCode::MalformedStylesheet,
                                          rule.location,
                                          "Unterminated at-rule block.",
                                          std::string("'@") + rule.at.name + "' is never closed",
                                          "Add the missing '}'.");
                        index = end;
                    }
                    else
                    {
                        rule.at.blockIsDeclarationList = AtRuleBlockIsDeclarations(rule.at.name);

                        if (rule.at.blockIsDeclarationList)
                        {
                            rule.at.declarations = ParseDeclarations(index + 1, blockEnd);
                        }
                        else
                        {
                            rule.at.children = ParseRuleList(index + 1, blockEnd, false);
                        }

                        index = blockEnd + 1;
                    }
                }
                else if (index < end && tokens[index].type == CSSTokenType::Semicolon)
                {
                    ++index;
                }

                if (!IsKnownAtRule(rule.at.name))
                {
                    diagnostics.Warning(CSSDiagnosticCode::InvalidAtRule,
                                        rule.location,
                                        "Unknown at-rule '@" + rule.at.name + "'.",
                                        "the CSS reference does not define this at-rule",
                                        "Remove the at-rule.",
                                        "@" + rule.at.name);
                }

                rules.push_back(std::move(rule));
                continue;
            }

            // --- qualified rule ---------------------------------------------
            const std::size_t preludeBegin = index;
            int depth = 0;
            std::size_t preludeEnd = end;

            while (index < end)
            {
                const CSSTokenType type = tokens[index].type;

                if (type == CSSTokenType::LeftParen || type == CSSTokenType::LeftSquare ||
                    type == CSSTokenType::Function)
                {
                    ++depth;
                }
                else if (type == CSSTokenType::RightParen || type == CSSTokenType::RightSquare)
                {
                    if (depth > 0)
                    {
                        --depth;
                    }
                }
                else if (depth == 0 && type == CSSTokenType::LeftCurly)
                {
                    break;
                }
                else if (depth == 0 && type == CSSTokenType::RightCurly)
                {
                    // A stray closer: the caller's block ended early.
                    break;
                }

                ++index;
            }

            preludeEnd = index;

            if (index >= end || tokens[index].type != CSSTokenType::LeftCurly)
            {
                if (preludeEnd > preludeBegin)
                {
                    diagnostics.Warning(CSSDiagnosticCode::IgnoredRule,
                                        token.location,
                                        "Rule has no declaration block.",
                                        std::string("'") + TextOf(preludeBegin, preludeEnd) + "' is never followed by '{'",
                                        "Add a declaration block, or remove the text.");
                }
                index = preludeEnd;
                continue;
            }

            const std::size_t blockEnd =
                FindBlockEnd(index + 1, end, CSSTokenType::LeftCurly, CSSTokenType::RightCurly);

            if (blockEnd == end)
            {
                diagnostics.Error(CSSDiagnosticCode::MalformedStylesheet,
                                  token.location,
                                  "Unterminated rule block.",
                                  std::string("'") + TextOf(preludeBegin, preludeEnd) + "' is never closed",
                                  "Add the missing '}'.");
                break;
            }

            CSSRule rule;
            rule.kind = CSSRule::Kind::Qualified;
            rule.location = token.location;
            rule.selectorText = TextOf(preludeBegin, preludeEnd);
            rule.selectors = rule.selectorText.empty() ? std::vector<std::string>()
                                                       : SplitTopLevel(rule.selectorText, ',');
            rule.declarations = ParseDeclarations(index + 1, blockEnd);

            if (rule.selectors.empty())
            {
                diagnostics.Warning(CSSDiagnosticCode::IgnoredRule,
                                    rule.location,
                                    "Empty selector.",
                                    "a rule must name what it applies to",
                                    "Remove the rule, or write a selector.");
            }
            else
            {
                rules.push_back(std::move(rule));
            }

            index = blockEnd + 1;
        }

        (void)declarationBlockExpected;
        return rules;
    }

    CSSStylesheet CSSParser::ParseStylesheet(const std::string& fileName, const std::string& text)
    {
        currentFile = fileName;

        CSSTokenizer tokenizer(text, fileName, &diagnostics);
        tokens = tokenizer.Tokenize();

        CSSStylesheet sheet;
        sheet.fileName = fileName;
        sheet.rules = ParseRuleList(0, tokens.size(), false);

        tokens.clear();
        currentFile.clear();
        return sheet;
    }

    std::vector<CSSDeclaration> CSSParser::ParseDeclarationList(const std::string& fileName, const std::string& text)
    {
        currentFile = fileName;

        CSSTokenizer tokenizer(text, fileName, &diagnostics);
        tokens = tokenizer.Tokenize();

        std::vector<CSSDeclaration> declarations = ParseDeclarations(0, tokens.size());

        tokens.clear();
        currentFile.clear();
        return declarations;
    }
}
