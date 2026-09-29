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

                if (query == "querySelector" && !argument.empty() && argument[0] == '#')
                {
                    argument = argument.substr(1);
                }

                if (!argument.empty() &&
                    std::find(referencedElementIds.begin(), referencedElementIds.end(), argument) ==
                        referencedElementIds.end())
                {
                    referencedElementIds.push_back(argument);
                }

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

                if (std::find(referencedElementIds.begin(), referencedElementIds.end(), targetId) ==
                    referencedElementIds.end())
                {
                    referencedElementIds.push_back(targetId);
                }
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

        // Every referenced element must exist in the validated document.
        for (const std::string& elementId : referencedElementIds)
        {
            if (document.FindElementById(elementId) == nullptr)
            {
                SourceLocation location;
                location.file = document.sourceFile;

                diagnostics.Error(DiagnosticCode::UnresolvedScriptResource,
                                  location,
                                  "The behaviour module references element id '" + elementId +
                                      "', which does not exist in the document.",
                                  "The generated interaction table would contain a dangling binding.",
                                  "Add id=\"" + elementId + "\" in the markup, or correct the reference.",
                                  elementId);
            }
        }

        return diagnostics.ErrorCount() == errorsBefore;
    }
}
