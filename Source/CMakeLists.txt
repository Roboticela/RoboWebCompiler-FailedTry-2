# ---------------------------------------------------------------------------
# RoboHTMLCompiler core library.
#
# The library owns the whole compiler frontend. The generated registry tables
# under Generated/ are compiled straight in, so the compiler never parses JSON,
# HTML or CSS at run time.
# ---------------------------------------------------------------------------

set(RoboCoreSources
    "${CMAKE_CURRENT_SOURCE_DIR}/AST/Ast.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CodeGeneration/NativeTargetGenerator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CodeGeneration/WebTargetGenerator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Compiler/Compiler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CSS/StyleResolver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CSS/TailwindUtilities.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Diagnostics/Diagnostics.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/IR/NativeElement.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/JavaScript/JavaScriptModule.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Lexer/HtmlLexer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Native/NativeTreeBuilder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Parser/Parsers.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Registry/Registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Validation/Validation.cpp"
    "${CMAKE_SOURCE_DIR}/Generated/Source/GeneratedRegistries.cpp"
)

set(RoboCoreGeneratedHeader "${CMAKE_SOURCE_DIR}/Generated/Include/RoboHTMLCompiler/GeneratedEnums.h")

if(NOT EXISTS "${RoboCoreGeneratedHeader}")
    message(FATAL_ERROR
        "The generated registries are missing (${RoboCoreGeneratedHeader}).\n"
        "Run 'cmake --build <build> --target RoboGenerateRegistries' once, or run:\n"
        "    RoboHTMLCompilerTools: SpecExtractor + RegistryCompiler")
endif()

add_library(RoboHTMLCompilerLib STATIC ${RoboCoreSources})

target_include_directories(RoboHTMLCompilerLib
    PUBLIC
        "${CMAKE_SOURCE_DIR}/Include"
        "${CMAKE_SOURCE_DIR}/Generated/Include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(RoboHTMLCompilerLib PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(RoboHTMLCompilerLib PRIVATE /W4 /permissive-)
    if(RoboWarningsAsErrors)
        target_compile_options(RoboHTMLCompilerLib PRIVATE /WX)
    endif()
else()
    target_compile_options(RoboHTMLCompilerLib PRIVATE -Wall -Wextra -Wpedantic)
    if(RoboWarningsAsErrors)
        target_compile_options(RoboHTMLCompilerLib PRIVATE -Werror)
    endif()
endif()

set_target_properties(RoboHTMLCompilerLib PROPERTIES
    OUTPUT_NAME "RoboHTMLCompiler"
    FOLDER "RoboHTMLCompiler"
)

# ---------------------------------------------------------------------------
# The compiler executable
# ---------------------------------------------------------------------------

add_executable(RoboHTMLCompiler "${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp")

target_link_libraries(RoboHTMLCompiler PRIVATE RoboHTMLCompilerLib)

set_target_properties(RoboHTMLCompiler PROPERTIES
    FOLDER "RoboHTMLCompiler"
)

# Convenience target: compile the bundled example application into the build
# tree so that build/GeneratedHTML/index.html always exists for inspection.
add_custom_target(RoboCompileExample
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/GeneratedHTML"
    COMMAND RoboHTMLCompiler
            --name BasicApp
            --output "${CMAKE_BINARY_DIR}"
            "${CMAKE_SOURCE_DIR}/Examples/BasicApp"
    DEPENDS RoboHTMLCompiler
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Compiling Examples/BasicApp"
    USES_TERMINAL
)

set_target_properties(RoboCompileExample PROPERTIES FOLDER "RoboHTMLCompiler")
