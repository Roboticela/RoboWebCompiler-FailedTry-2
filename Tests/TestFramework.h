// ---------------------------------------------------------------------------
// RoboHTMLCompiler - minimal test framework.
//
// A registration-based harness with no external dependencies, so the test
// suite builds with exactly the same toolchain as the compiler.
// ---------------------------------------------------------------------------
#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace RoboTest
{
    struct TestCase
    {
        std::string suite;
        std::string name;
        std::function<void()> body;
        bool skipped = false;
    };

    std::vector<TestCase>& AllTests();

    struct Registrar
    {
        Registrar(const char* suite, const char* name, std::function<void()> body);
    };

    /// Records a failed expectation for the currently running test.
    void Fail(const std::string& message, const char* file, int line);

    /// Records a skipped test.
    void Skip(const std::string& reason);

    extern int CheckCount;
    extern int FailureCount;
    extern int SkipCount;

    /// Runs every registered test. Returns the process exit code.
    int RunAll(const std::string& filter);

    inline std::string Describe(const std::string& value) { return "\"" + value + "\""; }
    inline std::string Describe(const char* value) { return std::string("\"") + value + "\""; }
    inline std::string Describe(bool value) { return value ? "true" : "false"; }

    template <typename T>
    std::string Describe(const T& value)
    {
        std::ostringstream output;
        output << value;
        return output.str();
    }
}

#define ROBO_TEST(SuiteName, TestName)                                                          \
    static void SuiteName##_##TestName##_Body();                                                \
    static const RoboTest::Registrar SuiteName##_##TestName##_Registrar(                        \
        #SuiteName, #TestName, SuiteName##_##TestName##_Body);                                  \
    static void SuiteName##_##TestName##_Body()

#define ROBO_CHECK(condition)                                                                   \
    do                                                                                          \
    {                                                                                           \
        ++RoboTest::CheckCount;                                                                 \
        if (!(condition))                                                                       \
        {                                                                                       \
            RoboTest::Fail("expected: " #condition, __FILE__, __LINE__);                        \
        }                                                                                       \
    } while (false)

#define ROBO_CHECK_EQ(left, right)                                                              \
    do                                                                                          \
    {                                                                                           \
        ++RoboTest::CheckCount;                                                                 \
        const auto roboLeftValue = (left);                                                      \
        const auto roboRightValue = (right);                                                    \
        if (!(roboLeftValue == roboRightValue))                                                 \
        {                                                                                       \
            RoboTest::Fail("expected " #left " == " #right ", found " +                         \
                               RoboTest::Describe(roboLeftValue) + " vs " +                     \
                               RoboTest::Describe(roboRightValue),                              \
                           __FILE__, __LINE__);                                                 \
        }                                                                                       \
    } while (false)

#define ROBO_CHECK_NE(left, right)                                                              \
    do                                                                                          \
    {                                                                                           \
        ++RoboTest::CheckCount;                                                                 \
        if ((left) == (right))                                                                  \
        {                                                                                       \
            RoboTest::Fail("expected " #left " != " #right, __FILE__, __LINE__);                \
        }                                                                                       \
    } while (false)

#define ROBO_SKIP(reason)                                                                       \
    do                                                                                          \
    {                                                                                           \
        RoboTest::Skip(reason);                                                                 \
        return;                                                                                 \
    } while (false)
