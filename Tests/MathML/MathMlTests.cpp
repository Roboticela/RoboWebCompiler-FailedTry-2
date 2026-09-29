#include "../TestFramework.h"
#include "../TestHelpers.h"

#include <string>
#include <vector>

using namespace RoboHTMLCompiler;
using namespace RoboTest;

namespace
{
    std::vector<std::string> AllMathMlElements()
    {
        std::vector<std::string> names;
        for (std::size_t index = 0; index < Generated::ElementCount; ++index)
        {
            if (Generated::ElementTable[index].namespaceId == Generated::NamespaceId::MathMl)
            {
                names.emplace_back(Generated::ElementTable[index].name);
            }
        }
        return names;
    }
}

ROBO_TEST(MathMl, EveryRegisteredElementParsesInTheMathMlNamespace)
{
    for (const std::string& name : AllMathMlElements())
    {
        if (name == "math")
        {
            continue;
        }

        const std::string snippet = "<math><" + name + ">x</" + name + "></math>";

        DiagnosticSink sink;
        std::unique_ptr<MarkupDocument> document = ParseHtml(WrapBody(snippet), sink);
        ROBO_CHECK(document != nullptr);
        if (document == nullptr)
        {
            continue;
        }

        bool found = false;
        document->ForEachElement([&](const ElementNode& element) {
            if (element.sourceTag == name && element.ns == MarkupNamespace::MathMl)
            {
                found = true;
            }
        });
        ROBO_CHECK(found);
    }
}

ROBO_TEST(MathMl, EveryRegisteredElementValidates)
{
    for (const std::string& name : AllMathMlElements())
    {
        if (name == "math")
        {
            continue;
        }

        const std::string snippet = "<math><" + name + ">x</" + name + "></math>";

        DiagnosticSink sink;
        const bool valid = ValidateBody(snippet, sink);
        if (!valid)
        {
            std::cout << "      mathml element <" << name << "> failed validation\n"
                      << FormatDiagnostics(sink) << "\n";
        }
        ROBO_CHECK(valid);
    }
}

ROBO_TEST(MathMl, UnknownElementIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<math><mbogus>x</mbogus></math>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::UnknownMathMlElement));
}

ROBO_TEST(MathMl, InvalidAttributeIsRejected)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<math bogus=\"1\"><mi>x</mi></math>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::InvalidMathMlAttribute));
}

ROBO_TEST(MathMl, AttributeScopedToTheWrongElementIsRejected)
{
    DiagnosticSink sink;
    // 'display' belongs to <math> only.
    ROBO_CHECK(!ValidateBody("<math><mi display=\"block\">x</mi></math>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::InvalidMathMlAttribute));

    DiagnosticSink second;
    // 'linethickness' belongs to <mfrac> only.
    ROBO_CHECK(!ValidateBody("<math><mi linethickness=\"2px\">x</mi></math>", second));
    ROBO_CHECK(HasError(second, DiagnosticCode::InvalidMathMlAttribute));
}

ROBO_TEST(MathMl, FullPresentationVocabularyIsAccepted)
{
    DiagnosticSink sink;
    ROBO_CHECK(ValidateBody(
        "<math display=\"block\" mathcolor=\"#111827\" mathbackground=\"#ffffff\" mathsize=\"1.2em\" "
        "displaystyle=\"true\" scriptlevel=\"0\">"
        "<mrow><mi mathvariant=\"bold\">x</mi><mo stretchy=\"true\" form=\"infix\" fence=\"false\" "
        "separator=\"false\" largeop=\"false\" movablelimits=\"false\" accent=\"false\">+</mo>"
        "<mn>2</mn></mrow>"
        "<mfrac linethickness=\"1px\" numalign=\"center\" denomalign=\"center\">"
        "<mn>1</mn><mn>2</mn></mfrac>"
        "<msup><mi>x</mi><mn>2</mn></msup>"
        "<msub><mi>x</mi><mn>1</mn></msub>"
        "<msubsup><mi>x</mi><mn>1</mn><mn>2</mn></msubsup>"
        "<munder><mi>x</mi><mo>&#8721;</mo></munder>"
        "<mover><mi>x</mi><mo>&#175;</mo></mover>"
        "<munderover><mo>&#8721;</mo><mn>0</mn><mn>1</mn></munderover>"
        "<mmultiscripts><mi>x</mi><mprescripts></mprescripts><none></none></mmultiscripts>"
        "<msqrt><mn>4</mn></msqrt>"
        "<mroot><mn>4</mn><mn>2</mn></mroot>"
        "<mspace width=\"1em\" height=\"1em\" depth=\"0em\"></mspace>"
        "<mpadded width=\"1em\" lspace=\"0\" voffset=\"0\"></mpadded>"
        "<mstyle mathvariant=\"italic\"></mstyle>"
        "<merror></merror>"
        "<mphantom></mphantom>"
        "<maction actiontype=\"toggle\" selection=\"1\"><mi>x</mi></maction>"
        "<mtext>label</mtext>"
        "<mtable columnalign=\"center\" rowalign=\"baseline\" columnspacing=\"1em\" rowspacing=\"1em\" "
        "columnlines=\"none\" rowlines=\"none\" frame=\"none\">"
        "<mtr><mtd rowspan=\"1\" columnspan=\"1\">a</mtd></mtr></mtable>"
        "<semantics><annotation encoding=\"application/x-tex\">x+2</annotation>"
        "<annotation-xml encoding=\"text/html\"><p>x+2</p></annotation-xml></semantics>"
        "</math>",
        sink));
}

ROBO_TEST(MathMl, MathMlIsNotLoweredIntoOrdinaryHtmlText)
{
    DiagnosticSink sink;
    std::unique_ptr<MarkupDocument> document =
        ParseHtml(WrapBody("<math><mfrac><mn>1</mn><mn>2</mn></mfrac></math>"), sink);
    ROBO_CHECK(document != nullptr);

    if (document == nullptr)
    {
        return;
    }

    bool fractionFound = false;
    document->ForEachElement([&](const ElementNode& element) {
        if (element.ns == MarkupNamespace::MathMl && element.sourceTag == "mfrac")
        {
            fractionFound = true;
        }
    });

    ROBO_CHECK(fractionFound);
}

ROBO_TEST(MathMl, InlineCssAndHandlersAreRejectedInsideMathMl)
{
    DiagnosticSink sink;
    ROBO_CHECK(!ValidateBody("<math><mi style=\"color: red\">x</mi></math>", sink));
    ROBO_CHECK(HasError(sink, DiagnosticCode::InlineCssAttribute));

    DiagnosticSink second;
    ROBO_CHECK(!ValidateBody("<math><mi onclick=\"f()\">x</mi></math>", second));
    ROBO_CHECK(HasError(second, DiagnosticCode::InlineEventHandlerAttribute));
}
