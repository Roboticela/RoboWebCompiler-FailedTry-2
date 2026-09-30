#include "RoboHTMLCompiler/JavaScriptModule.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace RoboHTMLCompiler
{
    namespace
    {
        int LineOf(const std::string& text, std::size_t offset)
        {
            int line = 1;
            for (std::size_t index = 0; index < offset && index < text.size(); ++index)
            {
                if (text[index] == '\n')
                {
                    ++line;
                }
            }
            return line;
        }

        /// One-based character number in the line that contains `offset`.
        int ColumnOf(const std::string& text, std::size_t offset)
        {
            std::size_t lineStart = 0;

            for (std::size_t index = 0; index < offset && index < text.size(); ++index)
            {
                if (text[index] == '\n')
                {
                    lineStart = index + 1;
                }
            }

            return static_cast<int>(offset - lineStart) + 1;
        }

        /// The text of one line, without its terminator, so a report can show
        /// the code the reader has to change.
        std::string LineTextOf(const std::string& text, int line)
        {
            int current = 1;
            std::size_t start = 0;

            for (;;)
            {
                if (current == line || start >= text.size())
                {
                    break;
                }

                const std::size_t newline = text.find('\n', start);
                if (newline == std::string::npos)
                {
                    start = text.size();
                    break;
                }

                start = newline + 1;
                ++current;
            }

            if (start >= text.size())
            {
                return std::string();
            }

            std::size_t end = text.find('\n', start);
            if (end == std::string::npos)
            {
                end = text.size();
            }

            std::string result = text.substr(start, end - start);
            while (!result.empty() && (result.back() == '\r' || result.back() == ' ' || result.back() == '\t'))
            {
                result.pop_back();
            }

            std::size_t firstNonSpace = 0;
            while (firstNonSpace < result.size() && (result[firstNonSpace] == ' ' || result[firstNonSpace] == '\t'))
            {
                ++firstNonSpace;
            }

            if (firstNonSpace > 0)
            {
                result.erase(0, firstNonSpace);
            }

            return result;
        }

        bool IsIdentifierCharacter(char character)
        {
            return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_' ||
                   character == '$';
        }

        /// Reads the identifier that ends immediately before the offset.
        std::string IdentifierBefore(const std::string& text, std::size_t& cursor)
        {
            while (cursor > 0 && std::isspace(static_cast<unsigned char>(text[cursor - 1])) != 0)
            {
                --cursor;
            }

            const std::size_t end = cursor;
            while (cursor > 0 && IsIdentifierCharacter(text[cursor - 1]))
            {
                --cursor;
            }

            return text.substr(cursor, end - cursor);
        }

        /// Recovers `name` from `const name = document.getElementById("x")`.
        /// Returns false when the query result is not assigned to a variable.
        bool FindAssignedVariable(const std::string& source, std::size_t callStart, std::string& out)
        {
            std::size_t cursor = callStart;
            std::string receiver = IdentifierBefore(source, cursor);

            // Consume a member access such as `document.` before the call.
            if (cursor > 0 && source[cursor - 1] == '.')
            {
                --cursor;
                receiver = IdentifierBefore(source, cursor);
            }
            else
            {
                receiver.clear();
            }

            if (!receiver.empty() && receiver != "document" && receiver != "window")
            {
                // The query result belongs to an element handle, not a document
                // lookup, so there is no document id to record here.
                out.clear();
                return false;
            }

            while (cursor > 0 && std::isspace(static_cast<unsigned char>(source[cursor - 1])) != 0)
            {
                --cursor;
            }

            if (cursor == 0 || source[cursor - 1] != '=')
            {
                out.clear();
                return false;
            }

            --cursor;
            const std::string variable = IdentifierBefore(source, cursor);
            if (variable.empty() || variable == "const" || variable == "let" || variable == "var")
            {
                out.clear();
                return false;
            }

            out = variable;
            return true;
        }

        /// Reads a simple quoted string literal starting at the opening quote.
        bool ReadStringLiteral(const std::string& text, std::size_t quotePosition, std::string& out)
        {
            if (quotePosition >= text.size() ||
                (text[quotePosition] != '"' && text[quotePosition] != '\'' && text[quotePosition] != '`'))
            {
                return false;
            }

            const char quote = text[quotePosition];
            std::size_t position = quotePosition + 1;
            out.clear();

            while (position < text.size() && text[position] != quote)
            {
                if (text[position] == '\\' && position + 1 < text.size())
                {
                    out.push_back(text[position + 1]);
                    position += 2;
                    continue;
                }

                out.push_back(text[position]);
                ++position;
            }

            return position < text.size();
        }
    }

    JavaScriptModuleCompiler::JavaScriptModuleCompiler(DiagnosticSink& diagnosticSink)
        : diagnostics(diagnosticSink)
    {
    }

    void JavaScriptModuleCompiler::AddScriptResource(const ScriptResource& resource)
    {
        scriptResources.push_back(resource);
    }

    void JavaScriptModuleCompiler::AddModule(const std::string& fileName, const std::string& source)
    {
        ScanModule(fileName, source);
    }

    void JavaScriptModuleCompiler::RecordQuery(const std::string& targetId,
                                              const std::string& fileName,
                                              const std::string& source,
                                              std::size_t offset)
    {
        if (targetId.empty())
        {
            return;
        }

        // The same id is often used twice - declared once, listened to once -
        // and one position is enough to fix it, so the first one is kept.
        for (const QueryBinding& existing : elementQueries)
        {
            if (existing.targetId == targetId)
            {
                return;
            }
        }

        QueryBinding query;
        query.targetId = targetId;
        query.sourceFile = fileName;
        query.line = LineOf(source, offset);
        query.column = ColumnOf(source, offset);
        query.sourceLine = LineTextOf(source, query.line);
        elementQueries.push_back(std::move(query));
    }

    void JavaScriptModuleCompiler::ScanModule(const std::string& fileName, const std::string& source)
    {
        // Variable -> element id, discovered from
        //     const name = document.getElementById("id");
        //     const name = document.querySelector("#id");
        std::map<std::string, std::string> variables;

        const std::string queries[] = { "getElementById", "querySelector" };

        for (const std::string& query : queries)
        {
            std::size_t position = 0;
            while ((position = source.find(query, position)) != std::string::npos)
            {
                std::size_t cursor = position + query.size();

                // Skip to the argument list.
                while (cursor < source.size() && std::isspace(static_cast<unsigned char>(source[cursor])) != 0)
                {
                    ++cursor;
                }

                if (cursor >= source.size() || source[cursor] != '(')
                {
                    position = cursor;
                    continue;
                }

                ++cursor;
                while (cursor < source.size() && std::isspace(static_cast<unsigned char>(source[cursor])) != 0)
                {
                    ++cursor;
                }

                std::string argument;
                if (!ReadStringLiteral(source, cursor, argument))
                {
                    position = cursor;
                    continue;
                }

                // The position reported is the first character of the id itself,
                // inside its quotes, past a leading '#' when the query uses one.
                std::size_t idOffset = cursor + 1;

                if (query == "querySelector" && !argument.empty() && argument[0] == '#')
                {
                    argument = argument.substr(1);
                    ++idOffset;
                }
                else if (query == "querySelector")
                {
                    // `querySelector(".row")` is not an element id, and pretending
                    // it is would report a missing element named `.row`. The
                    // compiler follows id lookups; anything else is named here, at
                    // the line that uses it.
                    const SourceLocation location{ fileName, LineOf(source, cursor), ColumnOf(source, cursor),
                                                   cursor };

                    diagnostics.Error(
                        DiagnosticCode::MalformedBehaviorScript,
                        location,
                        "The script looks up an element with \"" + argument + "\", and only element id "
                        "lookups are compiled.",
                        "A compiled application finds its elements by id, so the interaction table can name "
                        "one element per lookup.",
                        "Give the element an id and look it up with document.getElementById(\"the-id\") or "
                        "document.querySelector(\"#the-id\").",
                        argument);

                    position = cursor;
                    continue;
                }

                RecordQuery(argument, fileName, source, idOffset);

                std::string variableName;
                if (FindAssignedVariable(source, position, variableName))
                {
                    variables[variableName] = argument;
                }

                position = cursor;
            }
        }

        // Event bindings: <variable>.addEventListener("event", handler) or
        // document.getElementById("id").addEventListener("event", handler).
        std::size_t position = 0;
        const std::string listener = "addEventListener";

        while ((position = source.find(listener, position)) != std::string::npos)
        {
            std::size_t cursor = position + listener.size();
            while (cursor < source.size() && std::isspace(static_cast<unsigned char>(source[cursor])) != 0)
            {
                ++cursor;
            }

            if (cursor >= source.size() || source[cursor] != '(')
            {
                position = cursor;
                continue;
            }

            ++cursor;
            while (cursor < source.size() && std::isspace(static_cast<unsigned char>(source[cursor])) != 0)
            {
                ++cursor;
            }

            std::string eventName;
            if (!ReadStringLiteral(source, cursor, eventName))
            {
                position = cursor;
                continue;
            }

            // Handler is the second argument.
            std::size_t argumentCursor = cursor;
            int depth = 0;
            while (argumentCursor < source.size())
            {
                if (source[argumentCursor] == ',')
                {
                    break;
                }
                if (source[argumentCursor] == '(')
                {
                    ++depth;
                }
                else if (source[argumentCursor] == ')')
                {
                    --depth;
                }
                if (depth < 0)
                {
                    break;
                }
                ++argumentCursor;
            }

            std::string handler;
            if (argumentCursor < source.size())
            {
                std::size_t handlerBegin = argumentCursor + 1;
                while (handlerBegin < source.size() &&
                       std::isspace(static_cast<unsigned char>(source[handlerBegin])) != 0)
                {
                    ++handlerBegin;
                }

                std::size_t handlerEnd = handlerBegin;
                while (handlerEnd < source.size() && IsIdentifierCharacter(source[handlerEnd]))
                {
                    ++handlerEnd;
                }

                handler = source.substr(handlerBegin, handlerEnd - handlerBegin);
            }

            // Determine the receiver of the call.
            std::string receiver;
            std::size_t receiverEnd = position;
            while (receiverEnd > 0 && std::isspace(static_cast<unsigned char>(source[receiverEnd - 1])) != 0)
            {
                --receiverEnd;
            }

            if (receiverEnd > 0 && source[receiverEnd - 1] == '.')
            {
                std::size_t receiverBegin = receiverEnd - 1;
                while (receiverBegin > 0 && IsIdentifierCharacter(source[receiverBegin - 1]))
                {
                    --receiverBegin;
                }
                receiver = source.substr(receiverBegin, receiverEnd - 1 - receiverBegin);
            }

            std::string targetId;
            const auto variable = variables.find(receiver);
            if (variable != variables.end())
            {
                targetId = variable->second;
            }
            else if (receiver == "document" || receiver.empty())
            {
                // Global listeners carry no element identity.
                targetId.clear();
            }
            else
            {
                targetId = receiver;
            }

            if (!targetId.empty())
            {
                EventBinding binding;
                binding.targetId = targetId;
                binding.event = eventName;
                binding.handler = handler.empty() ? "<inline>" : handler;
                binding.sourceFile = fileName;
                binding.line = LineOf(source, position);
                eventBindings.push_back(std::move(binding));

                RecordQuery(targetId, fileName, source, position);
            }

            position = cursor;
        }

        // Detect behaviour source that embeds markup or styling: that would mix
        // the three languages in the wrong direction.
        static const char* const forbiddenPatterns[] = { "innerHTML", "style.cssText", "document.write" };
        for (const char* pattern : forbiddenPatterns)
        {
            std::size_t found = source.find(pattern);
            while (found != std::string::npos)
            {
                diagnostics.Error(DiagnosticCode::MalformedBehaviorScript,
                                  SourceLocation{ fileName, LineOf(source, found), 1, found },
                                  std::string("'") + pattern +
                                      "' would build structure or appearance from behaviour.",
                                  "RoboHTMLCompiler keeps HTML, CSS and JavaScript in separate languages: markup and "
                                  "styles must come from the compiled model, not from strings.",
                                  "Express the change through the compiled model instead.",
                                  pattern);
                found = source.find(pattern, found + 1);
            }
        }
    }

    bool JavaScriptModuleCompiler::LinkAgainstDocument(const MarkupDocument& document)
    {
        const std::size_t errorsBefore = diagnostics.ErrorCount();

        // Every element a behaviour module looks up must exist in the page it is
        // compiled for.
        for (const QueryBinding& query : elementQueries)
        {
            if (document.FindElementById(query.targetId) != nullptr)
            {
                continue;
            }

            SourceLocation location;
            // The report points into the script, at the line that asks for the
            // missing element: that is the line the reader has to change.
            location.file = query.sourceFile.empty() ? document.sourceFile : query.sourceFile;
            location.line = query.line > 0 ? query.line : 1;
            location.column = query.column > 0 ? query.column : 1;
            location.offset = 0;

            diagnostics.Error(DiagnosticCode::UnresolvedScriptResource,
                              location,
                              "The script asks for element id \"" + query.targetId +
                                  "\", but this page has no element with that id.",
                              std::string(),
                              "Give the element id=\"" + query.targetId +
                                  "\" in the HTML file, or correct the id in the script so the two match exactly.",
                              query.sourceLine);
        }

        return diagnostics.ErrorCount() == errorsBefore;
    }
}
