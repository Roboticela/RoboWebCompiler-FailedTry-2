# ---------------------------------------------------------------------------
# RoboHTMLCompilerTests
#
# The suite is organised by frontend so that a failure points straight at the
# component that regressed.
# ---------------------------------------------------------------------------

set(RoboTestSources
    "${CMAKE_CURRENT_SOURCE_DIR}/TestMain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CSS/CssCascadeTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CSS/CssReferenceCoverageTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CSS/StyleCompilerTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RoboRenderer/BackendImageTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RoboRenderer/InputTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RoboRenderer/LayoutAndPaintTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RoboRenderer/RuntimeContractTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RoboRenderer/StyleAndTextTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RoboRenderer/TypographyTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RoboRenderer/VisualTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/HTML/HtmlElementCoverageTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Integration/CompilationTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Integration/CssPipelineTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Integration/NativeModelTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/JavaScript/BehaviourModuleTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/MathML/MathMlTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Registry/RegistryTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/SVG/SvgTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Validation/AttributeValidationTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Validation/SeparationRulesTests.cpp"
)

# ---------------------------------------------------------------------------
# The generated runtime target, built and compiled as part of the test run.
#
#     Examples/BasicApp --RoboHTMLCompiler--> <App>CompiledUi.{h,cpp}
#
# The generated source is compiled into the test executable, so the end to end
# test consumes exactly the data an application would: this is the only place
# where the generated C++ is checked by actually running it.
# ---------------------------------------------------------------------------
# The compiler writes each target into its own subdirectory of the output root.
set(RoboGeneratedUiDirectory "${CMAKE_BINARY_DIR}/generated-runtime")
set(RoboGeneratedUiSource "${RoboGeneratedUiDirectory}/GeneratedRuntime/BasicAppCompiledUi.cpp")

add_custom_command(
    OUTPUT "${RoboGeneratedUiSource}"
    COMMAND RoboHTMLCompiler
            --name BasicApp
            --target runtime
            --output "${RoboGeneratedUiDirectory}"
            "${CMAKE_SOURCE_DIR}/Examples/BasicApp"
    DEPENDS RoboHTMLCompiler
            "${CMAKE_SOURCE_DIR}/Examples/BasicApp/index.html"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Generating the compiled UI of BasicApp for the renderer tests"
    VERBATIM
)

add_custom_target(RoboGeneratedUi DEPENDS "${RoboGeneratedUiSource}")

set_source_files_properties("${RoboGeneratedUiSource}" PROPERTIES GENERATED TRUE)

add_executable(RoboHTMLCompilerTests ${RoboTestSources} "${RoboGeneratedUiSource}")

add_dependencies(RoboHTMLCompilerTests RoboGeneratedUi)

target_include_directories(RoboHTMLCompilerTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${RoboGeneratedUiDirectory}/GeneratedRuntime"
)

target_link_libraries(RoboHTMLCompilerTests PRIVATE RoboHTMLCompilerLib RoboRenderer RoboRendererHarness)

target_compile_definitions(RoboHTMLCompilerTests PRIVATE
    ROBO_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    ROBO_HAVE_GENERATED_UI=1
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

# The typography showcase exercises the text stack end to end and must keep
# compiling: it is the document that says what the renderer can draw.
add_test(NAME RoboHTMLCompilerTypographyExample
    COMMAND RoboHTMLCompiler
            --name TypographyShowcase
            --output "${CMAKE_BINARY_DIR}/example-typography"
            --all
            "${CMAKE_SOURCE_DIR}/Examples/TypographyShowcase"
)
set_tests_properties(RoboHTMLCompilerTypographyExample PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
