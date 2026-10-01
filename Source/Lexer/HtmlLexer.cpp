#include "RoboHTMLCompiler/HtmlLexer.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string_view>

namespace RoboHTMLCompiler
{
    namespace
    {
        bool IsHexDigit(char character)
        {
            return std::isxdigit(static_cast<unsigned char>(character)) != 0;
        }

        int HexValue(char character)
        {
            if (character >= '0' && character <= '9')
            {
                return character - '0';
            }
            const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            return lower - 'a' + 10;
        }

        void AppendUtf8(std::string& out, std::uint32_t codepoint)
        {
            if (codepoint < 0x80)
            {
                out.push_back(static_cast<char>(codepoint));
            }
            else if (codepoint < 0x800)
            {
                out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else if (codepoint < 0x10000)
            {
                out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else
            {
                out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
                out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
        }

        /// The named character references documents actually write. Numeric
        /// references are handled for the whole Unicode range, so this table
        /// only has to cover the handful of Latin-1 and punctuation names.
        const char* NamedReference(std::string_view name)
        {
            struct Entry
            {
                const char* name;
                const char* utf8;
            };

            static const Entry kTable[] = {
                { "amp", "&" },      { "lt", "<" },       { "gt", ">" },
                { "quot", "\"" },     { "apos", "'" },      { "nbsp", "\xC2\xA0" },
                { "iexcl", "\xC2\xA1" }, { "cent", "\xC2\xA2" },   { "pound", "\xC2\xA3" },
                { "curren", "\xC2\xA4" }, { "yen", "\xC2\xA5" },    { "brvbar", "\xC2\xA6" },
                { "sect", "\xC2\xA7" },  { "uml", "\xC2\xA8" },    { "copy", "\xC2\xA9" },
                { "ordf", "\xC2\xAA" },  { "laquo", "\xC2\xAB" },  { "not", "\xC2\xAC" },
                { "shy", "\xC2\xAD" },   { "reg", "\xC2\xAE" },    { "macr", "\xC2\xAF" },
                { "deg", "\xC2\xB0" },   { "plusmn", "\xC2\xB1" }, { "sup2", "\xC2\xB2" },
                { "sup3", "\xC2\xB3" },  { "acute", "\xC2\xB4" },  { "micro", "\xC2\xB5" },
                { "para", "\xC2\xB6" },  { "middot", "\xC2\xB7" }, { "cedil", "\xC2\xB8" },
                { "sup1", "\xC2\xB9" },  { "ordm", "\xC2\xBA" },   { "raquo", "\xC2\xBB" },
                { "frac14", "\xC2\xBC" },{ "frac12", "\xC2\xBD" }, { "frac34", "\xC2\xBE" },
                { "iquest", "\xC2\xBF" },
                { "Agrave", "\xC3\x80" },{ "Aacute", "\xC3\x81" },{ "Acirc", "\xC3\x82" },
                { "Atilde", "\xC3\x83" },{ "Auml", "\xC3\x84" },  { "Aring", "\xC3\x85" },
                { "AElig", "\xC3\x86" }, { "Ccedil", "\xC3\x87" },{ "Egrave", "\xC3\x88" },
                { "Eacute", "\xC3\x89" },{ "Ecirc", "\xC3\x8A" },  { "Euml", "\xC3\x8B" },
                { "Igrave", "\xC3\x8C" },{ "Iacute", "\xC3\x8D" },{ "Icirc", "\xC3\x8E" },
                { "Iuml", "\xC3\x8F" },  { "ETH", "\xC3\x90" },    { "Ntilde", "\xC3\x91" },
                { "Ograve", "\xC3\x92" },{ "Oacute", "\xC3\x93" },{ "Ocirc", "\xC3\x94" },
                { "Otilde", "\xC3\x95" },{ "Ouml", "\xC3\x96" },  { "times", "\xC3\x97" },
                { "Oslash", "\xC3\x98" },{ "Ugrave", "\xC3\x99" },{ "Uacute", "\xC3\x9A" },
                { "Ucirc", "\xC3\x9B" }, { "Uuml", "\xC3\x9C" },  { "Yacute", "\xC3\x9D" },
                { "THORN", "\xC3\x9E" }, { "szlig", "\xC3\x9F" },
                { "agrave", "\xC3\xA0" },{ "aacute", "\xC3\xA1" },{ "acirc", "\xC3\xA2" },
                { "atilde", "\xC3\xA3" },{ "auml", "\xC3\xA4" },  { "aring", "\xC3\xA5" },
                { "aelig", "\xC3\xA6" }, { "ccedil", "\xC3\xA7" },{ "egrave", "\xC3\xA8" },
                { "eacute", "\xC3\xA9" },{ "ecirc", "\xC3\xAA" },  { "euml", "\xC3\xAB" },
                { "igrave", "\xC3\xAC" },{ "iacute", "\xC3\xAD" },{ "icirc", "\xC3\xAE" },
                { "iuml", "\xC3\xAF" },  { "eth", "\xC3\xB0" },    { "ntilde", "\xC3\xB1" },
                { "ograve", "\xC3\xB2" },{ "oacute", "\xC3\xB3" },{ "ocirc", "\xC3\xB4" },
                { "otilde", "\xC3\xB5" },{ "ouml", "\xC3\xB6" },  { "divide", "\xC3\xB7" },
                { "oslash", "\xC3\xB8" },{ "ugrave", "\xC3\xB9" },{ "uacute", "\xC3\xBA" },
                { "ucirc", "\xC3\xBB" }, { "uuml", "\xC3\xBC" },  { "yacute", "\xC3\xBD" },
                { "thorn", "\xC3\xBE" }, { "yuml", "\xC3\xBF" },
                { "OElig", "\xC5\x92" }, { "oelig", "\xC5\x93" }, { "Scaron", "\xC5\xA0" },
                { "scaron", "\xC5\xA1" },{ "Yuml", "\xC5\xB8" },  { "fnof", "\xC6\x92" },
                { "ndash", "\xE2\x80\x93" },{ "mdash", "\xE2\x80\x94" },
                { "lsquo", "\xE2\x80\x98" },{ "rsquo", "\xE2\x80\x99" },
                { "sbquo", "\xE2\x80\x9A" },{ "ldquo", "\xE2\x80\x9C" },
                { "rdquo", "\xE2\x80\x9D" },{ "bdquo", "\xE2\x80\x9E" },
                { "dagger", "\xE2\x80\xA0" },{ "Dagger", "\xE2\x80\xA1" },
                { "bull", "\xE2\x80\xA2" }, { "hellip", "\xE2\x80\xA6" },
                { "permil", "\xE2\x80\xB0" },{ "prime", "\xE2\x80\xB2" },
                { "Prime", "\xE2\x80\xB3" },{ "lsaquo", "\xE2\x80\xB9" },
                { "rsaquo", "\xE2\x80\xBA" },{ "oline", "\xE2\x80\xBE" },
                { "frasl", "\xE2\x81\x84" },{ "euro", "\xE2\x82\xAC" },
                { "trade", "\xE2\x84\xA2" },{ "larr", "\xE2\x86\x90" },
                { "uarr", "\xE2\x86\x91" },{ "rarr", "\xE2\x86\x92" },
                { "darr", "\xE2\x86\x93" },{ "harr", "\xE2\x86\x94" },
                { "minus", "\xE2\x88\x92" },{ "lowast", "\xE2\x88\x97" },
                { "radic", "\xE2\x88\x9A" },{ "infin", "\xE2\x88\x9E" },
                { "le", "\xE2\x89\xA4" },  { "ge", "\xE2\x89\xA5" },
                { "ne", "\xE2\x89\xA0" },  { "sum", "\xE2\x88\x91" },
                { "prod", "\xE2\x88\x8F" },{ "int", "\xE2\x88\xAB" },
                { "minus", "\xE2\x88\x92" },{ "part", "\xE2\x88\x82" },
                { "nabla", "\xE2\x88\x87" },{ "isin", "\xE2\x88\x88" },
                { "notin", "\xE2\x88\x89" },{ "empty", "\xE2\x88\x85" },
                { "and", "\xE2\x88\xA7" }, { "or", "\xE2\x88\xA8" },
                { "cap", "\xE2\x88\xA9" }, { "cup", "\xE2\x88\xAA" },
                { "sigma", "\xCF\x83" }, { "Sigma", "\xCE\xA3" },
                { "pi", "\xCF\x80" },    { "Pi", "\xCE\xA0" },
                { "mu", "\xCE\xBC" },    { "alpha", "\xCE\xB1" },
                { "beta", "\xCE\xB2" },  { "gamma", "\xCE\xB3" },
                { "Delta", "\xCE\x94" }, { "lambda", "\xCE\xBB" },
                { "omega", "\xCF\x89" }, { "Omega", "\xCE\xA9" },
                { "spades", "\xE2\x99\xA0" },{ "clubs", "\xE2\x99\xA3" },
                { "hearts", "\xE2\x99\xA5" },{ "diams", "\xE2\x99\xA6" },
                { "loz", "\xE2\x97\x8A" }, { "ensp", "\xE2\x80\x82" },
                { "emsp", "\xE2\x80\x83" },{ "thinsp", "\xE2\x80\x89" },
                { "zwnj", "\xE2\x80\x8C" },{ "zwj", "\xE2\x80\x8D" },
                { "lrm", "\xE2\x80\x8E" }, { "rlm", "\xE2\x80\x8F" },
            };

            for (const Entry& entry : kTable)
            {
                if (name == entry.name)
                {
                    return entry.utf8;
                }
            }
            return nullptr;
        }

        /// Decodes HTML character references: numeric (`&#769;`, `&#x1F600;`)
        /// across the whole Unicode range, and the common named set above. A
        /// reference that is not well formed is left exactly as written.
        std::string DecodeCharacterReferences(std::string_view input)
        {
            if (input.find('&') == std::string_view::npos)
            {
                return std::string(input);
            }

            std::string out;
            out.reserve(input.size());

            for (std::size_t index = 0; index < input.size();)
            {
                if (input[index] != '&')
                {
                    out.push_back(input[index++]);
                    continue;
                }

                // Numeric: `&#dd;` or `&#xhh;`.
                if (index + 1 < input.size() && input[index + 1] == '#')
                {
                    std::size_t cursor = index + 2;
                    const bool hex = cursor < input.size() &&
                                     (input[cursor] == 'x' || input[cursor] == 'X');
                    if (hex)
                    {
                        ++cursor;
                    }

                    const std::size_t digitsBegin = cursor;
                    std::uint32_t code = 0;
                    while (cursor < input.size() &&
                           (hex ? IsHexDigit(input[cursor])
                                : std::isdigit(static_cast<unsigned char>(input[cursor])) != 0))
                    {
                        code = code * (hex ? 16u : 10u) +
                               static_cast<std::uint32_t>(hex ? HexValue(input[cursor])
                                                              : input[cursor] - '0');
                        ++cursor;
                    }

                    if (cursor > digitsBegin && cursor < input.size() && input[cursor] == ';' &&
                        code != 0 && code <= 0x10FFFF)
                    {
                        AppendUtf8(out, code);
                        index = cursor + 1;
                        continue;
                    }

                    out.push_back(input[index++]);
                    continue;
                }

                // Named: `&name;`.
                const std::size_t semicolon = input.find(';', index + 1);
                if (semicolon != std::string_view::npos && semicolon - (index + 1) <= 32)
                {
                    const char* decoded = NamedReference(input.substr(index + 1, semicolon - index - 1));
                    if (decoded != nullptr)
                    {
                        out += decoded;
                        index = semicolon + 1;
                        continue;
                    }
                }

                out.push_back(input[index++]);
            }

            return out;
        }
    }

    HtmlLexer::HtmlLexer(std::string sourceText, std::string fileNameValue, DiagnosticSink& diagnosticSink)
        : source(std::move(sourceText))
        , fileName(std::move(fileNameValue))
        , diagnostics(diagnosticSink)
    {
        lineStarts.push_back(0);
        for (std::size_t index = 0; index < source.size(); ++index)
        {
            if (source[index] == '\n')
            {
                lineStarts.push_back(index + 1);
            }
        }
    }

    char HtmlLexer::Peek(std::size_t lookahead) const
    {
        const std::size_t index = position + lookahead;
        return index < source.size() ? source[index] : '\0';
    }

    bool HtmlLexer::MatchAhead(std::string_view text) const
    {
        return source.compare(position, text.size(), text) == 0;
    }

    void HtmlLexer::Advance(std::size_t count)
    {
        position = std::min(position + count, source.size());
    }

    SourceLocation HtmlLexer::LocationAt(std::size_t offset) const
    {
        SourceLocation location;
        location.file = fileName;
        location.offset = offset;

        const auto iterator = std::upper_bound(lineStarts.begin(), lineStarts.end(), offset);
        const std::size_t lineIndex = static_cast<std::size_t>(iterator - lineStarts.begin());
        location.line = static_cast<int>(lineIndex == 0 ? 1 : lineIndex);
        const std::size_t lineStart = lineStarts[lineIndex == 0 ? 0 : lineIndex - 1];
        location.column = static_cast<int>(offset - lineStart + 1);
        return location;
    }

    void HtmlLexer::Reset()
    {
        position = 0;
    }

    bool HtmlLexer::IsTagNameCharacter(char character)
    {
        return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '-' ||
               character == '_' || character == ':' || character == '.';
    }

    bool HtmlLexer::IsAttributeNameCharacter(char character)
    {
        return std::isspace(static_cast<unsigned char>(character)) == 0 && character != '=' &&
               character != '>' && character != '/' && character != '<' && character != '"' &&
               character != '\'';
    }

    bool HtmlLexer::TryReadComment(Token& token)
    {
        if (!MatchAhead("<!--"))
        {
            return false;
        }

        token.kind = TokenKind::Comment;
        token.location = CurrentLocation();

        const std::size_t terminator = source.find("-->", position + 4);
        const std::size_t contentBegin = position + 4;

        if (terminator == std::string::npos)
        {
            token.text = source.substr(contentBegin);
            Advance(source.size() - position);
            return true;
        }

        token.text = source.substr(contentBegin, terminator - contentBegin);
        Advance(terminator + 3 - position);
        return true;
    }

    bool HtmlLexer::TryReadDoctype(Token& token)
    {
        if (!MatchAhead("<!") && !MatchAhead("<?"))
        {
            return false;
        }

        token.kind = TokenKind::Doctype;
        token.location = CurrentLocation();

        const std::size_t terminator = source.find('>', position);
        if (terminator == std::string::npos)
        {
            token.text = source.substr(position + 2);
            Advance(source.size() - position);
            return true;
        }

        token.text = source.substr(position + 2, terminator - position - 2);
        Advance(terminator + 1 - position);

        std::size_t nameBegin = 0;
        while (nameBegin < token.text.size() &&
               std::isspace(static_cast<unsigned char>(token.text[nameBegin])) != 0)
        {
            ++nameBegin;
        }

        std::size_t nameEnd = nameBegin;
        while (nameEnd < token.text.size() && IsTagNameCharacter(token.text[nameEnd]))
        {
            ++nameEnd;
        }

        token.name = ToLowerAscii(token.text.substr(nameBegin, nameEnd - nameBegin));
        return true;
    }

    bool HtmlLexer::TryReadEndTag(Token& token)
    {
        if (!MatchAhead("</"))
        {
            return false;
        }

        token.kind = TokenKind::EndTag;
        token.location = CurrentLocation();
        Advance(2);

        std::string name;
        while (!IsAtEnd() && IsTagNameCharacter(Peek()))
        {
            name.push_back(Peek());
            Advance();
        }

        token.originalName = name;
        token.name = ToLowerAscii(name);

        ReadUntilTagEnd();

        if (name.empty())
        {
            Diagnostic diagnostic;
            diagnostic.code = DiagnosticCode::MalformedMarkup;
            diagnostic.location = token.location;
            diagnostic.message = "End tag without a name.";
            diagnostic.found = source.substr(token.location.offset, 2);
            diagnostics.Report(std::move(diagnostic));
            return true;
        }

        return true;
    }

    void HtmlLexer::ReadAttribute(Token& token)
    {
        AttributeNode attribute;
        attribute.location = CurrentLocation();

        while (!IsAtEnd() && IsAttributeNameCharacter(Peek()))
        {
            attribute.name.push_back(Peek());
            Advance();
        }

        if (attribute.name.empty())
        {
            // Defensive: skip a character that cannot start an attribute.
            Advance();
            return;
        }

        attribute.name = ToLowerAscii(attribute.name);

        while (!IsAtEnd() && std::isspace(static_cast<unsigned char>(Peek())) != 0)
        {
            Advance();
        }

        if (!IsAtEnd() && Peek() == '=')
        {
            Advance();
            while (!IsAtEnd() && std::isspace(static_cast<unsigned char>(Peek())) != 0)
            {
                Advance();
            }

            if (!IsAtEnd() && (Peek() == '"' || Peek() == '\''))
            {
                const char quote = Peek();
                Advance();

                while (!IsAtEnd() && Peek() != quote)
                {
                    attribute.value.push_back(Peek());
                    Advance();
                }

                if (!IsAtEnd())
                {
                    Advance();
                }
                attribute.hasValue = true;
            }
            else
            {
                while (!IsAtEnd() && std::isspace(static_cast<unsigned char>(Peek())) == 0 &&
                       Peek() != '>' && Peek() != '/')
                {
                    attribute.value.push_back(Peek());
                    Advance();
                }
                attribute.hasValue = true;
            }
        }
        else
        {
            // Valueless attribute, e.g. <button disabled>.
            attribute.value.clear();
            attribute.hasValue = false;
        }

        if (attribute.hasValue)
        {
            // Character references are decoded in attribute values too, so a
            // `title="Tom &amp; Jerry"` reads as written.
            attribute.value = DecodeCharacterReferences(attribute.value);
        }

        token.attributes.push_back(std::move(attribute));
    }

    bool HtmlLexer::TryReadStartTag(Token& token)
    {
        if (Peek() != '<' || !IsTagNameCharacter(Peek(1)))
        {
            return false;
        }

        token.kind = TokenKind::StartTag;
        token.location = CurrentLocation();
        Advance();

        std::string name;
        while (!IsAtEnd() && IsTagNameCharacter(Peek()))
        {
            name.push_back(Peek());
            Advance();
        }

        token.originalName = name;
        token.name = ToLowerAscii(name);

        while (!IsAtEnd())
        {
            while (!IsAtEnd() && std::isspace(static_cast<unsigned char>(Peek())) != 0)
            {
                Advance();
            }

            if (IsAtEnd())
            {
                break;
            }

            if (Peek() == '>')
            {
                Advance();
                break;
            }

            if (Peek() == '/')
            {
                Advance();
                if (!IsAtEnd() && Peek() == '>')
                {
                    token.selfClosing = true;
                    Advance();
                    break;
                }
                continue;
            }

            ReadAttribute(token);
        }

        if (token.attributes.empty() == false)
        {
            std::vector<std::string> seen;
            for (const AttributeNode& attribute : token.attributes)
            {
                if (std::find(seen.begin(), seen.end(), attribute.name) != seen.end())
                {
                    diagnostics.Error(DiagnosticCode::DuplicateAttribute,
                                      attribute.location,
                                      "Duplicate attribute '" + attribute.name + "' on <" + token.name + ">.",
                                      std::string(),
                                      "Remove the duplicate attribute.",
                                      attribute.name + "=\"...\"");
                }
                else
                {
                    seen.push_back(attribute.name);
                }
            }
        }

        return true;
    }

    void HtmlLexer::ReadUntilTagEnd()
    {
        while (!IsAtEnd() && Peek() != '>')
        {
            Advance();
        }

        if (!IsAtEnd())
        {
            Advance();
        }
    }

    void HtmlLexer::EnterRawTextMode(const std::string& tagName)
    {
        rawTextTag = ToLowerAscii(tagName);
    }

    Token HtmlLexer::Next()
    {
        Token token;

        if (IsAtEnd())
        {
            token.kind = TokenKind::EndOfFile;
            token.location = CurrentLocation();
            return token;
        }

        if (!rawTextTag.empty())
        {
            const std::string closing = "</" + rawTextTag;
            const std::size_t textBegin = position;

            std::size_t searchFrom = position;
            std::size_t terminator = std::string::npos;
            while (searchFrom < source.size())
            {
                const std::size_t candidate = source.find('<', searchFrom);
                if (candidate == std::string::npos)
                {
                    break;
                }

                if (EqualsIgnoreCase(source.substr(candidate, closing.size()), closing))
                {
                    terminator = candidate;
                    break;
                }

                searchFrom = candidate + 1;
            }

            token.kind = TokenKind::Text;
            token.location = CurrentLocation();

            if (terminator == std::string::npos)
            {
                token.text = source.substr(textBegin);
                Advance(source.size() - position);
            }
            else
            {
                token.text = source.substr(textBegin, terminator - textBegin);
                Advance(terminator - position);
            }

            rawTextTag.clear();
            return token;
        }

        if (Peek() == '<')
        {
            if (TryReadComment(token))
            {
                return token;
            }
            if (TryReadDoctype(token))
            {
                return token;
            }
            if (TryReadEndTag(token))
            {
                return token;
            }
            if (TryReadStartTag(token))
            {
                return token;
            }
        }

        token.kind = TokenKind::Text;
        token.location = CurrentLocation();

        const std::size_t textBegin = position;
        while (!IsAtEnd() && Peek() != '<')
        {
            Advance();
        }

        token.text = DecodeCharacterReferences(source.substr(textBegin, position - textBegin));
        return token;
    }

    std::vector<Token> HtmlLexer::Tokenize()
    {
        std::vector<Token> tokens;
        while (true)
        {
            Token token = Next();
            const bool finished = token.kind == TokenKind::EndOfFile;
            tokens.push_back(std::move(token));
            if (finished)
            {
                break;
            }
        }
        return tokens;
    }
}
