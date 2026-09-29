# ---------------------------------------------------------------------------
# RoboHTMLCompilerTests
#
# The suite is organised by frontend so that a failure points straight at the
# component that regressed.
# ---------------------------------------------------------------------------

set(RoboTestSources
    "${CMAKE_CURRENT_SOURCE_DIR}/TestMain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CSS/StyleCompilerTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/HTML/HtmlElementCoverageTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Integration/CompilationTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Integration/NativeModelTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/JavaScript/BehaviourModuleTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/MathML/MathMlTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Registry/RegistryTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/SVG/SvgTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Validation/AttributeValidationTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Validation/SeparationRulesTests.cpp"
)

add_executable(RoboHTMLCompilerTests ${RoboTestSources})

target_include_directories(RoboHTMLCompilerTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(RoboHTMLCompilerTests PRIVATE RoboHTMLCompilerLib)

target_compile_definitions(RoboHTMLCompilerTests PRIVATE
    ROBO_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

set_target_properties(RoboHTMLCompilerTests PROPERTIES FOLDER "RoboHTMLCompilerTests")

if(MSVC)
    target_compile_options(RoboHTMLCompilerTests PRIVATE /W4 /permissive-)
else()
    target_compile_options(RoboHTMLCompilerTests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME RoboHTMLCompilerTests COMMAND RoboHTMLCompilerTests)

# The example must be compiled and re-validated as part of the test run.
add_test(NAME RoboHTMLCompilerExample
    COMMAND RoboHTMLCompiler
            --name BasicApp
            --output "${CMAKE_BINARY_DIR}/example"
            "${CMAKE_SOURCE_DIR}/Examples/BasicApp"
)
set_tests_properties(RoboHTMLCompilerExample PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
