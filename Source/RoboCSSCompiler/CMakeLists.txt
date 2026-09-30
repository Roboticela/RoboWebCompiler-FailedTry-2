# ---------------------------------------------------------------------------
# RoboCSSCompiler - the CSS frontend.
#
# CSS is a source language, so it is compiled here and nowhere else. The library
# depends on nothing in RoboHTMLCompiler or RoboRenderer: it takes stylesheets
# and a view of the element tree, and produces typed computed styles. The
# compiler links it; the runtime never does.
# ---------------------------------------------------------------------------

set(RoboCSSSources
    "${CMAKE_CURRENT_SOURCE_DIR}/CssCascade.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CssDiagnostics.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CssParser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CssProperty.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CssSelector.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CssSyntax.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CssValue.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CssValueParser.cpp"
)

add_library(RoboCSSCompilerLib STATIC ${RoboCSSSources})

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/CompilerKnowledge/Include/RoboCSSCompiler/GeneratedCssVocabulary.h")
    message(FATAL_ERROR
        "The generated CSS vocabulary is missing\n"
        "(${CMAKE_SOURCE_DIR}/CompilerKnowledge/Include/RoboCSSCompiler/GeneratedCssVocabulary.h).\n"
        "It is checked in next to the sources. Restore CompilerKnowledge/, or point\n"
        "-DRoboCssReferenceDirectory at the CSS Working Group drafts and run:\n"
        "    cmake --build <build> --target RoboExtractCssReferences")
endif()

target_include_directories(RoboCSSCompilerLib
    PUBLIC
        "${CMAKE_SOURCE_DIR}/Include"
        "${CMAKE_SOURCE_DIR}/CompilerKnowledge/Include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(RoboCSSCompilerLib PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(RoboCSSCompilerLib PRIVATE /W4 /permissive-)
    if(RoboWarningsAsErrors)
        target_compile_options(RoboCSSCompilerLib PRIVATE /WX)
    endif()
else()
    target_compile_options(RoboCSSCompilerLib PRIVATE -Wall -Wextra -Wpedantic)
    if(RoboWarningsAsErrors)
        target_compile_options(RoboCSSCompilerLib PRIVATE -Werror)
    endif()
endif()

set_target_properties(RoboCSSCompilerLib PROPERTIES
    OUTPUT_NAME "RoboCSSCompiler"
    FOLDER "RoboCSSCompiler"
)
