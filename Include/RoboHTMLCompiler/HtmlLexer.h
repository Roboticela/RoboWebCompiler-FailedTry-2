// ---------------------------------------------------------------------------
// RoboHTMLCompiler - markup lexer.
//
// The lexer is namespace agnostic: it produces tags, attribute tokens, text,
// comments and doctypes. Everything that depends on the source vocabulary
// (void elements, raw text elements, custom element rules) is decided by the
// parsers and validators.
// ---------------------------------------------------------------------------
#pragma once

#include "RoboHTMLCompiler/Ast.h"
#include "RoboHTMLCompiler/Diagnostics.h"

#include <string>
#include <string_view>
#include <vector>

namespace RoboHTMLCompiler
{
    enum class TokenKind
    {
        StartTag,
        EndTag,
        Text,
        Comment,
        Doctype,
        EndOfFile
    };

    struct Token
    {
        TokenKind kind = TokenKind::EndOfFile;
        std::string name;                                    // tag name, lowercased
        std::string originalName;                            // tag name as written
        std::string text;                                    // text, comment or doctype payload
        std::vector<AttributeNode> attributes;
        SourceLocation location;
        bool selfClosing = false;
    };

    class HtmlLexer
    {
    public:
        HtmlLexer(std::string source, std::string fileName, DiagnosticSink& diagnostics);

        /// Reads the next token, or an EndOfFile token once exhausted.
        Token Next();

        /// Tokenises the whole input.
        std::vector<Token> Tokenize();

        /// Rewinds to the start of the input.
        void Reset();

        /// Switches the lexer into raw-text mode until the matching end tag of
        /// the supplied element name. Used for <script> and <style>, whose
        /// contents are not markup and must never be tokenised as tags.
        void EnterRawTextMode(const std::string& tagName);

        /// Leaves raw-text mode early.
        void ExitRawTextMode() { rawTextTag.clear(); }

        const std::string& GetSource() const { return source; }

    private:
        bool IsAtEnd() const { return position >= source.size(); }
        char Peek(std::size_t lookahead = 0) const;
        bool MatchAhead(std::string_view text) const;
        void Advance(std::size_t count = 1);
        SourceLocation LocationAt(std::size_t offset) const;
        SourceLocation CurrentLocation() const { return LocationAt(position); }

        bool TryReadComment(Token& token);
        bool TryReadDoctype(Token& token);
        bool TryReadEndTag(Token& token);
        bool TryReadStartTag(Token& token);
        void ReadAttribute(Token& token);
        void ReadUntilTagEnd();

        static bool IsTagNameCharacter(char character);
        static bool IsAttributeNameCharacter(char character);

        std::string source;
        std::string fileName;
        DiagnosticSink& diagnostics;
        std::size_t position = 0;
        std::string rawTextTag;
        std::vector<std::size_t> lineStarts;
    };
}
