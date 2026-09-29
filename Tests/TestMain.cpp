#include "TestFramework.h"

#include <iostream>
#include <string>
#include <vector>

namespace RoboTest
{
    int CheckCount = 0;
    int FailureCount = 0;
    int SkipCount = 0;

    namespace
    {
        std::string currentTest;
        bool currentTestFailed = false;
        bool currentTestSkipped = false;
    }

    std::vector<TestCase>& AllTests()
    {
        static std::vector<TestCase> tests;
        return tests;
    }

    Registrar::Registrar(const char* suite, const char* name, std::function<void()> body)
    {
        TestCase test;
        test.suite = suite;
        test.name = name;
        test.body = std::move(body);
        AllTests().push_back(std::move(test));
    }

    void Fail(const std::string& message, const char* file, int line)
    {
        ++FailureCount;
        currentTestFailed = true;

        const char* fileName = file;
        const char* cursor = file;
        while (*cursor != '\0')
        {
            if (*cursor == '/' || *cursor == '\\')
            {
                fileName = cursor + 1;
            }
            ++cursor;
        }

        std::cout << "    FAIL " << currentTest << "\n"
                  << "         " << fileName << ":" << line << ": " << message << "\n";
    }

    void Skip(const std::string& reason)
    {
        ++SkipCount;
        currentTestSkipped = true;
        std::cout << "    SKIP " << currentTest << ": " << reason << "\n";
    }

    int RunAll(const std::string& filter)
    {
        int passed = 0;
        int failed = 0;

        for (TestCase& test : AllTests())
        {
            currentTest = test.suite + "." + test.name;

            if (!filter.empty() && currentTest.find(filter) == std::string::npos)
            {
                continue;
            }

            currentTestFailed = false;
            currentTestSkipped = false;

            const int failuresBefore = FailureCount;

            try
            {
                test.body();
            }
            catch (const std::exception& error)
            {
                Fail(std::string("unhandled exception: ") + error.what(), __FILE__, __LINE__);
            }
            catch (...)
            {
                Fail("unhandled exception", __FILE__, __LINE__);
            }

            if (FailureCount > failuresBefore)
            {
                ++failed;
            }
            else
            {
                ++passed;
                if (!currentTestSkipped)
                {
                    std::cout << "    ok   " << currentTest << "\n";
                }
            }
        }

        std::cout << "\n"
                  << passed << " passed, " << failed << " failed, " << SkipCount << " skipped ("
                  << CheckCount << " checks)\n";

        return failed == 0 ? 0 : 1;
    }
}

int main(int argumentCount, char** argumentValues)
{
    const std::string filter = (argumentCount > 1) ? argumentValues[1] : std::string();

    std::cout << "RoboHTMLCompilerTests\n";
    std::cout << "=====================\n";

    return RoboTest::RunAll(filter);
}
