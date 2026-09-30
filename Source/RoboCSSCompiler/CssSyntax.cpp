#include "RoboCSSCompiler/CssSyntax.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace RoboCSSCompiler
{
    const char* ToString(CSSTokenType type)
    {
        switch (type)
        {
        case CSSTokenType::EndOfFile: return "EOF";
        case CSSTokenType::Ident: return "ident";
        case CSSTokenType::Function: return "function";
        case CSSTokenType::AtKeyword: return "at-keyword";
        case CSSTokenType::Hash: return "hash";
        case CSSTokenType::QuotedString: return "string";
        case CSSTokenType::BadString: return "bad-string";
        case CSSTokenType::Url: return "url";
        case CSSTokenType::BadUrl: return "bad-url";
        case CSSTokenType::Delim: return "delim";
        case CSSTokenType::Number: return "number";
        case CSSTokenType::Percentage: return "percentage";
        case CSSTokenType::Dimension: return "dimension";
        case CSSTokenType::Whitespace: return "whitespace";
        case CSSTokenType::Colon: return ":";
        case CSSTokenType::Semicolon: return ";";
        case CSSTokenType::Comma: return ",";
        case CSSTokenType::LeftSquare: return "[";
        case CSSTokenType::RightSquare: return "]";
        case CSSTokenType::LeftParen: return "(";
        case CSSTokenType::RightParen: return ")";
        case CSSTokenType::LeftCurly: return "{";
        case CSSTokenType::RightCurly: return "}";
        case CSSTokenType::Cdo: return "<!--";
        case CSSTokenType::Cdc: return "-->";
        }

        return "?";
    }

    bool CSSToken::IsIdent(const char* name) const
    {
        if (type != CSSTokenType::Ident || name == nullptr || text.size() != std::strlen(name))
        {
            return false;
        }

        for (std::size_t index = 0; index < text.size(); ++index)
        {
            const char left = static_cast<char>(std::tolower(static_cast<unsigned char>(text[index])));
            if (left != name[index])
            {
                return false;
            }
        }

        return true;
    }

    namespace
    {
        bool IsWhitespaceChar(char character)
        {
            return character == ' ' || character == '\t' || character == '\n';
        }

        bool IsDigit(char character)
        {
            return character >= '0' && character <= '9';
        }

        bool IsHexDigit(char character)
        {
            return IsDigit(character) || (character >= 'a' && character <= 'f') ||
                   (character >= 'A' && character <= 'F');
        }

        unsigned HexValue(char character)
        {
            if (IsDigit(character))
            {
                return static_cast<unsigned>(character - '0');
            }

            return static_cast<unsigned>((character >= 'a' ? character - 'a' : character - 'A') + 10);
        }

        bool IsIdentStartChar(char character)
        {
            return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                   character == '_' || static_cast<unsigned char>(character) >= 0x80;
        }

        /// Appends a code point as UTF-8. The surrogate range and out-of-range
        /// values are replaced, as the syntax specification requires.
        void AppendCodePoint(std::string& out, std::uint32_t codePoint)
        {
            if (codePoint == 0 || codePoint > 0x10FFFFu || (codePoint >= 0xD800u && codePoint <= 0xDFFFu))
            {
                out += "\xEF\xBF\xBD"; // U+FFFD
                return;
            }

            if (codePoint < 0x80u)
            {
                out.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint < 0x800u)
            {
                out.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
                out.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
            }
            else if (codePoint < 0x10000u)
            {
                out.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
                out.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
            }
            else
            {
                out.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
                out.push_back(static_cast<char>(0x80u | ((codePoint >> 12) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
                out.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
            }
        }
    }

    CSSTokenizer::CSSTokenizer(std::string text, std::string name, CSSDiagnosticSink* diagnosticSink)
        : source(std::move(text))
        , fileName(std::move(name))
        , diagnostics(diagnosticSink)
    {
        // Preprocessing: CRLF, CR and FF become LF, and NUL becomes U+FFFD, so
        // the tokenizer can treat every line ending as one character.
        std::string normalized;
        normalized.reserve(source.size());

        for (std::size_t index = 0; index < source.size(); ++index)
        {
            const char character = source[index];

            if (character == '\r')
            {
                if (index + 1 < source.size() && source[index + 1] == '\n')
                {
                    ++index;
                }
                normalized.push_back('\n');
            }
            else if (character == '\f')
            {
                normalized.push_back('\n');
            }
            else if (character == '\0')
            {
                normalized += "\xEF\xBF\xBD";
            }
            else
            {
                normalized.push_back(character);
            }
        }

        source = std::move(normalized);
    }

    char CSSTokenizer::Peek(std::size_t ahead) const
    {
        const std::size_t index = position + ahead;
        return index < source.size() ? source[index] : '\0';
    }

    char CSSTokenizer::Advance()
    {
        if (AtEnd())
        {
            return '\0';
        }

        const char character = source[position];
        ++position;

        if (character == '\n')
        {
            NewLine();
        }

        return character;
    }

    void CSSTokenizer::NewLine()
    {
        ++line;
        lineStart = static_cast<std::uint32_t>(position);
    }

    void CSSTokenizer::SetLocation(CSSSourceLocation& location) const
    {
        location.file = fileName;
        location.line = line;
        location.column = static_cast<std::uint32_t>(position) - lineStart + 1;
        location.offset = static_cast<std::uint32_t>(position);
    }

    bool CSSTokenizer::IsNonAscii(char character)
    {
        return static_cast<unsigned char>(character) >= 0x80;
    }

    bool CSSTokenizer::IsNameChar(char character)
    {
        return IsDigit(character) || IsIdentStartChar(character) || character == '-';
    }

    bool CSSTokenizer::WouldStartIdent(std::size_t offset) const
    {
        const char first = Peek(offset);
        const char second = Peek(offset + 1);

        if (first == '-')
        {
            if (IsIdentStartChar(second) || second == '-')
            {
                return true;
            }

            return second == '\\' && Peek(offset + 2) != '\n';
        }

        if (IsIdentStartChar(first))
        {
            return true;
        }

        return first == '\\' && second != '\n' && second != '\0';
    }

    bool CSSTokenizer::WouldStartNumber(std::size_t offset) const
    {
        const char first = Peek(offset);
        const char second = Peek(offset + 1);
        const char third = Peek(offset + 2);

        if (first == '+' || first == '-')
        {
            if (IsDigit(second))
            {
                return true;
            }

            return second == '.' && IsDigit(third);
        }

        if (first == '.')
        {
            return IsDigit(second);
        }

        return IsDigit(first);
    }

    std::uint32_t CSSTokenizer::ConsumeEscape()
    {
        // The backslash is already consumed.
        if (IsHexDigit(Peek()))
        {
            std::uint32_t value = 0;
            std::size_t digits = 0;

            while (digits < 6 && IsHexDigit(Peek()))
            {
                value = value * 16u + HexValue(Advance());
                ++digits;
            }

            // One whitespace character after the escape is part of it.
            if (IsWhitespaceChar(Peek()))
            {
                Advance();
            }

            return value;
        }

        if (AtEnd())
        {
            return 0xFFFDu;
        }

        return static_cast<std::uint32_t>(static_cast<unsigned char>(Advance()));
    }

    std::string CSSTokenizer::ConsumeName()
    {
        std::string name;

        while (!AtEnd())
        {
            const char character = Peek();

            if (IsNameChar(character))
            {
                name.push_back(Advance());
            }
            else if (character == '\\' && Peek(1) != '\n' && Peek(1) != '\0')
            {
                Advance();
                AppendCodePoint(name, ConsumeEscape());
            }
            else
            {
                break;
            }
        }

        return name;
    }

    CSSToken CSSTokenizer::ConsumeNumeric()
    {
        CSSToken token;
        SetLocation(token.location);

        const std::size_t start = position;
        bool isInteger = true;

        if (Peek() == '+' || Peek() == '-')
        {
            Advance();
        }

        while (IsDigit(Peek()))
        {
            Advance();
        }

        if (Peek() == '.' && IsDigit(Peek(1)))
        {
            isInteger = false;
            Advance();
            while (IsDigit(Peek()))
            {
                Advance();
            }
        }

        if ((Peek() == 'e' || Peek() == 'E') &&
            (IsDigit(Peek(1)) || ((Peek(1) == '+' || Peek(1) == '-') && IsDigit(Peek(2)))))
        {
            isInteger = false;
            Advance();
            if (Peek() == '+' || Peek() == '-')
            {
                Advance();
            }
            while (IsDigit(Peek()))
            {
                Advance();
            }
        }

        const std::string numberText = source.substr(start, position - start);
        token.number = std::strtod(numberText.c_str(), nullptr);
        token.isInteger = isInteger;

        if (Peek() == '%')
        {
            Advance();
            token.type = CSSTokenType::Percentage;
            token.text = numberText + "%";
        }
        else if (WouldStartIdent(0))
        {
            token.type = CSSTokenType::Dimension;
            token.unit = ConsumeName();
            std::transform(token.unit.begin(), token.unit.end(), token.unit.begin(), [](char character) {
                return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            });
            token.text = numberText;
        }
        else
        {
            token.type = CSSTokenType::Number;
            token.text = numberText;
        }

        token.raw = source.substr(start, position - start);
        return token;
    }

    CSSToken CSSTokenizer::ConsumeString(char quote)
    {
        CSSToken token;
        SetLocation(token.location);
        token.type = CSSTokenType::QuotedString;

        const std::size_t start = position;
        Advance(); // the opening quote

        while (true)
        {
            if (AtEnd())
            {
                if (diagnostics != nullptr)
                {
                    diagnostics->Warning(CSSDiagnosticCode::MalformedStylesheet,
                                         token.location,
                                         "Unterminated string at the end of the stylesheet.",
                                         "the closing quote is missing",
                                         "Close the string, or remove it.");
                }
                break;
            }

            const char character = Peek();

            if (character == quote)
            {
                Advance();
                break;
            }

            if (character == '\n')
            {
                // A newline inside a string is a parse error: the string ends.
                token.type = CSSTokenType::BadString;
                break;
            }

            if (character == '\\')
            {
                if (Peek(1) == '\n')
                {
                    Advance();
                    Advance();
                    continue;
                }

                Advance();
                AppendCodePoint(token.text, ConsumeEscape());
                continue;
            }

            token.text.push_back(Advance());
        }

        token.raw = source.substr(start, position - start);
        return token;
    }

    namespace
    {
        /// True for a character an unquoted url may not contain.
        bool IsForbiddenUrlChar(char character)
        {
            const unsigned char value = static_cast<unsigned char>(character);

            if (character == '"' || character == '\'' || character == '(' || character == '\\')
            {
                return true;
            }

            return value <= 0x1Fu || value == 0x7Fu;
        }
    }

    bool CSSTokenizer::ConsumeUrlBody(std::string& out)
    {
        while (IsWhitespaceChar(Peek()))
        {
            Advance();
        }

        while (!AtEnd())
        {
            const char character = Peek();

            if (character == ')')
            {
                Advance();
                return true;
            }

            if (IsWhitespaceChar(character))
            {
                while (IsWhitespaceChar(Peek()))
                {
                    Advance();
                }

                if (Peek() == ')')
                {
                    Advance();
                    return true;
                }

                // Anything else after whitespace is not a valid url: discard the
                // remnants of the function and fail.
                while (!AtEnd() && Peek() != ')')
                {
                    if (Peek() == '\\' && Peek(1) != '\n')
                    {
                        Advance();
                    }
                    Advance();
                }

                if (Peek() == ')')
                {
                    Advance();
                }

                return false;
            }

            if (IsForbiddenUrlChar(character))
            {
                if (character == '\\' && Peek(1) != '\n')
                {
                    Advance();
                    AppendCodePoint(out, ConsumeEscape());
                    continue;
                }

                while (!AtEnd() && Peek() != ')')
                {
                    if (Peek() == '\\' && Peek(1) != '\n')
                    {
                        Advance();
                    }
                    Advance();
                }

                if (Peek() == ')')
                {
                    Advance();
                }

                return false;
            }

            if (character == '\\')
            {
                if (Peek(1) == '\n')
                {
                    Advance();
                    Advance();
                    continue;
                }

                Advance();
                AppendCodePoint(out, ConsumeEscape());
                continue;
            }

            out.push_back(Advance());
        }

        return false;
    }

    CSSToken CSSTokenizer::ConsumeUrl(const CSSSourceLocation& location, const std::string& rawPrefix)
    {
        CSSToken token;
        token.location = location;

        const std::size_t start = location.offset;

        // A quoted body is a string in disguise.
        if (IsWhitespaceChar(Peek()) || Peek() == '"' || Peek() == '\'')
        {
            while (IsWhitespaceChar(Peek()))
            {
                Advance();
            }

            if (Peek() == '"' || Peek() == '\'')
            {
                const CSSToken stringToken = ConsumeString(Peek());
                while (IsWhitespaceChar(Peek()))
                {
                    Advance();
                }

                if (Peek() == ')')
                {
                    Advance();
                    token.type =
                        stringToken.type == CSSTokenType::BadString ? CSSTokenType::BadUrl : CSSTokenType::Url;
                    token.text = stringToken.text;
                }
                else
                {
                    token.type = CSSTokenType::BadUrl;
                }

                token.raw = source.substr(start, position - start);
                return token;
            }
        }

        std::string body;
        const bool ok = ConsumeUrlBody(body);
        token.type = ok ? CSSTokenType::Url : CSSTokenType::BadUrl;
        token.text = body;
        token.raw = source.substr(start, position - start);

        if (!ok && diagnostics != nullptr)
        {
            diagnostics->Warning(CSSDiagnosticCode::InvalidValue,
                                 location,
                                 "Malformed url() token.",
                                 "the url body is not a valid unquoted url",
                                 "Quote the url, or remove the characters that end it early.",
                                 rawPrefix);
        }

        return token;
    }

    void CSSTokenizer::ConsumeComment()
    {
        CSSSourceLocation location;
        SetLocation(location);

        while (!AtEnd())
        {
            if (Peek() == '*' && Peek(1) == '/')
            {
                Advance();
                Advance();
                return;
            }

            Advance();
        }

        if (diagnostics != nullptr)
        {
            diagnostics->Error(CSSDiagnosticCode::MalformedStylesheet,
                               location,
                               "Unterminated comment.",
                               "the stylesheet ends inside /* ... */",
                               "Close the comment with */.");
        }
    }

    std::vector<CSSToken> CSSTokenizer::Tokenize()
    {
        // A token averages a handful of characters, and a CSSToken is large
        // enough that growing the vector one doubling at a time copies tens of
        // megabytes on a big sheet. One guess up front costs nothing when it is
        // too small and saves the copies when it is close.
        std::vector<CSSToken> tokens;
        tokens.reserve(source.size() / 4);

        while (!AtEnd())
        {
            const char character = Peek();

            // --- whitespace --------------------------------------------------
            if (IsWhitespaceChar(character))
            {
                CSSToken token;
                SetLocation(token.location);
                const std::size_t start = position;

                token.type = CSSTokenType::Whitespace;
                while (IsWhitespaceChar(Peek()))
                {
                    Advance();
                }

                token.text = source.substr(start, position - start);
                token.raw = token.text;
                tokens.push_back(std::move(token));
                continue;
            }

            // --- comments ----------------------------------------------------
            if (character == '/' && Peek(1) == '*')
            {
                Advance();
                Advance();
                ConsumeComment();
                continue;
            }

            // --- strings -----------------------------------------------------
            if (character == '"' || character == '\'')
            {
                tokens.push_back(ConsumeString(character));
                continue;
            }

            // --- numbers (also covers `+`/`-`/`.` before digits) -------------
            if (WouldStartNumber(0))
            {
                tokens.push_back(ConsumeNumeric());
                continue;
            }

            CSSToken token;
            SetLocation(token.location);
            const std::size_t start = position;

            // --- `-->` -------------------------------------------------------
            if (character == '-' && Peek(1) == '-' && Peek(2) == '>')
            {
                Advance();
                Advance();
                Advance();
                token.type = CSSTokenType::Cdc;
                token.raw = "-->";
                tokens.push_back(std::move(token));
                continue;
            }

            // --- `<!--` ------------------------------------------------------
            if (character == '<' && Peek(1) == '!' && Peek(2) == '-' && Peek(3) == '-')
            {
                Advance();
                Advance();
                Advance();
                Advance();
                token.type = CSSTokenType::Cdo;
                token.raw = "<!--";
                tokens.push_back(std::move(token));
                continue;
            }

            // --- `@name` -----------------------------------------------------
            if (character == '@' && WouldStartIdent(1))
            {
                Advance();
                token.type = CSSTokenType::AtKeyword;
                token.text = ConsumeName();
                token.raw = source.substr(start, position - start);
                tokens.push_back(std::move(token));
                continue;
            }

            // --- `#name` -----------------------------------------------------
            if (character == '#')
            {
                if (IsNameChar(Peek(1)) || (Peek(1) == '\\' && Peek(2) != '\n'))
                {
                    Advance();
                    token.type = CSSTokenType::Hash;
                    token.text = ConsumeName();
                    token.hashIsId = WouldStartIdent(0);
                    token.raw = source.substr(start, position - start);
                    tokens.push_back(std::move(token));
                    continue;
                }

                token.type = CSSTokenType::Delim;
                token.text = std::string(1, Advance());
                token.raw = token.text;
                tokens.push_back(std::move(token));
                continue;
            }

            // --- `url(` ------------------------------------------------------
            if ((character == 'u' || character == 'U') && (Peek(1) == 'r' || Peek(1) == 'R') &&
                (Peek(2) == 'l' || Peek(2) == 'L') && Peek(3) == '(')
            {
                const std::string prefix = source.substr(start, 4);
                Advance();
                Advance();
                Advance();
                Advance();
                tokens.push_back(ConsumeUrl(token.location, prefix));
                continue;
            }

            // --- identifiers and functions -----------------------------------
            if (WouldStartIdent(0))
            {
                token.text = ConsumeName();
                token.type = CSSTokenType::Function;
                if (Peek() == '(')
                {
                    Advance();
                }
                else
                {
                    token.type = CSSTokenType::Ident;
                }

                token.raw = source.substr(start, position - start);
                tokens.push_back(std::move(token));
                continue;
            }

            // --- punctuation -------------------------------------------------
            switch (character)
            {
            case ':': Advance(); token.type = CSSTokenType::Colon; break;
            case ';': Advance(); token.type = CSSTokenType::Semicolon; break;
            case ',': Advance(); token.type = CSSTokenType::Comma; break;
            case '[': Advance(); token.type = CSSTokenType::LeftSquare; break;
            case ']': Advance(); token.type = CSSTokenType::RightSquare; break;
            case '(': Advance(); token.type = CSSTokenType::LeftParen; break;
            case ')': Advance(); token.type = CSSTokenType::RightParen; break;
            case '{': Advance(); token.type = CSSTokenType::LeftCurly; break;
            case '}': Advance(); token.type = CSSTokenType::RightCurly; break;
            default: Advance(); token.type = CSSTokenType::Delim; break;
            }

            token.text = source.substr(start, position - start);
            token.raw = token.text;
            tokens.push_back(std::move(token));
        }

        CSSToken end;
        SetLocation(end.location);
        end.type = CSSTokenType::EndOfFile;
        tokens.push_back(std::move(end));
        return tokens;
    }
}
