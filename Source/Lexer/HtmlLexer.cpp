#include "RoboHTMLCompiler/HtmlLexer.h"

#include <algorithm>
#include <cctype>

namespace RoboHTMLCompiler
{
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

        token.text = source.substr(textBegin, position - textBegin);
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
