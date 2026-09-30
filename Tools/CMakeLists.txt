# ---------------------------------------------------------------------------
# RoboHTMLCompilerTools
#
#     Reference/HTML/  --SpecExtractor-->  CompilerKnowledge/Vocabulary/*.json
#     Registry/*.json  --RegistryCompiler-->  CompilerKnowledge/Include, CompilerKnowledge/Source
#
# These targets are engineering tooling: they never run as part of a normal
# application build, but they are what turns the standards reference into the
# compiler's structured knowledge.
# ---------------------------------------------------------------------------

add_library(RoboHTMLCompilerTooling STATIC "${CMAKE_CURRENT_SOURCE_DIR}/Json.cpp")

target_include_directories(RoboHTMLCompilerTooling PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(RoboHTMLCompilerTooling PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(RoboHTMLCompilerTooling PRIVATE /W4 /permissive-)
else()
    target_compile_options(RoboHTMLCompilerTooling PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(RoboHTMLCompilerTooling PROPERTIES OUTPUT_NAME "RoboHTMLCompilerTooling" FOLDER "RoboHTMLCompilerTools")

add_executable(SpecExtractor "${CMAKE_CURRENT_SOURCE_DIR}/SpecExtractor.cpp")
target_link_libraries(SpecExtractor PRIVATE RoboHTMLCompilerTooling)
set_target_properties(SpecExtractor PROPERTIES FOLDER "RoboHTMLCompilerTools")

add_executable(RegistryCompiler "${CMAKE_CURRENT_SOURCE_DIR}/RegistryCompiler.cpp")
target_link_libraries(RegistryCompiler PRIVATE RoboHTMLCompilerTooling)
set_target_properties(RegistryCompiler PROPERTIES FOLDER "RoboHTMLCompilerTools")

# The CSS reference is Bikeshed source rather than wattsi source, so it has its
# own extractor; it feeds the same Vocabulary/ directory and the same
# consumption discipline as the HTML pipeline above.
add_executable(CssSpecExtractor "${CMAKE_CURRENT_SOURCE_DIR}/CssSpecExtractor.cpp")
target_link_libraries(CssSpecExtractor PRIVATE RoboHTMLCompilerTooling)
set_target_properties(CssSpecExtractor PROPERTIES FOLDER "RoboHTMLCompilerTools")

if(MSVC)
    target_compile_options(CssSpecExtractor PRIVATE /W4 /permissive-)
else()
    target_compile_options(CssSpecExtractor PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# RoboRenderer harness, viewer and benchmarks
#
#     RoboRendererHarness   compile a document in process into the compiled UI
#                           data RoboRenderer consumes. Shared by the viewer,
#                           the benchmarks and the renderer tests, so all three
#                           exercise the real compiler pipeline.
#     RoboViewer            render a document to an image
#     RoboBench             measure the runtime and print a report
# ---------------------------------------------------------------------------

add_library(RoboRendererHarness STATIC "${CMAKE_CURRENT_SOURCE_DIR}/RendererHarness.cpp")

target_include_directories(RoboRendererHarness
    PUBLIC
        "${CMAKE_SOURCE_DIR}/Tools"
)

target_link_libraries(RoboRendererHarness PUBLIC RoboHTMLCompilerLib RoboRenderer)

target_compile_features(RoboRendererHarness PUBLIC cxx_std_17)

set_target_properties(RoboRendererHarness PROPERTIES OUTPUT_NAME "RoboRendererHarness" FOLDER "RoboRendererTools")

if(MSVC)
    target_compile_options(RoboRendererHarness PRIVATE /W4 /permissive-)
else()
    target_compile_options(RoboRendererHarness PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(RoboViewer "${CMAKE_CURRENT_SOURCE_DIR}/RoboViewer.cpp")
target_link_libraries(RoboViewer PRIVATE RoboRendererHarness)
set_target_properties(RoboViewer PROPERTIES FOLDER "RoboRendererTools")

add_executable(RoboBench "${CMAKE_CURRENT_SOURCE_DIR}/RoboBench.cpp")
target_link_libraries(RoboBench PRIVATE RoboRendererHarness)
set_target_properties(RoboBench PROPERTIES FOLDER "RoboRendererTools")

if(MSVC)
    target_compile_options(RoboViewer PRIVATE /W4 /permissive-)
    target_compile_options(RoboBench PRIVATE /W4 /permissive-)
else()
    target_compile_options(RoboViewer PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(RoboBench PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# RoboWatch - the live preview
#
#     RoboWatch          the two sources in a window, the frame beside them,
#                        rebuilt as they are typed in
#
# It is the same in process compile and software frame RoboViewer produces, so
# what it previews is what the compiler draws. The window itself is Win32, which
# keeps the tool as dependency free as the runtime it shows; the target is
# therefore Windows only, and the rest of the build does not notice it.
# ---------------------------------------------------------------------------

if(WIN32)
    add_executable(RoboWatch "${CMAKE_CURRENT_SOURCE_DIR}/RoboWatch.cpp")
    target_link_libraries(RoboWatch PRIVATE RoboRendererHarness user32 gdi32)
    set_target_properties(RoboWatch PROPERTIES FOLDER "RoboRendererTools")

    if(MSVC)
        target_compile_options(RoboWatch PRIVATE /W4 /permissive-)
    else()
        target_compile_options(RoboWatch PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# ---------------------------------------------------------------------------
# Regeneration pipeline
# ---------------------------------------------------------------------------

add_custom_target(RoboExtractReferences
    COMMAND SpecExtractor
            "${RoboReferenceDirectory}"
            "${RoboGeneratedRegistryDirectory}"
            "${CMAKE_SOURCE_DIR}/CompilerKnowledge/ImplementationCoverage.json"
    DEPENDS SpecExtractor
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Mining the standards reference into registry JSON"
    USES_TERMINAL
)

add_custom_target(RoboCompileRegistries
    COMMAND RegistryCompiler
            "${RoboRegistrySourceDirectory}"
            "${RoboGeneratedRegistryDirectory}"
            "${CMAKE_SOURCE_DIR}/CompilerKnowledge/Include/RoboHTMLCompiler"
            "${CMAKE_SOURCE_DIR}/CompilerKnowledge/Source"
            "${CMAKE_SOURCE_DIR}/CompilerKnowledge/ImplementationCoverage.json"
    DEPENDS RegistryCompiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating strongly typed registry tables"
    USES_TERMINAL
)

# The reference directory is an implementation checklist rather than a runtime
# dependency. The audit reports every element and attribute each reference
# document defines that the compiler does not account for yet;
# RoboConsumeReferences deletes the revision snapshots that are fully covered.
add_custom_target(RoboAuditReferences
    COMMAND SpecExtractor
            "${RoboReferenceDirectory}"
            "${RoboGeneratedRegistryDirectory}"
            "${CMAKE_SOURCE_DIR}/CompilerKnowledge/ImplementationCoverage.json"
            --sweep
            --hand-registry "${RoboRegistrySourceDirectory}"
            --audit-report "${CMAKE_SOURCE_DIR}/CompilerKnowledge/ReferenceAudit.json"
    DEPENDS SpecExtractor
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Auditing the standards reference against the compiled-in vocabulary"
    USES_TERMINAL
)

add_custom_target(RoboConsumeReferences
    COMMAND SpecExtractor
            "${RoboReferenceDirectory}"
            "${RoboGeneratedRegistryDirectory}"
            "${CMAKE_SOURCE_DIR}/CompilerKnowledge/ImplementationCoverage.json"
            --sweep
            --consume
            --hand-registry "${RoboRegistrySourceDirectory}"
            --audit-report "${CMAKE_SOURCE_DIR}/CompilerKnowledge/ReferenceAudit.json"
    DEPENDS SpecExtractor
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Consuming the fully covered reference documents"
    USES_TERMINAL
)

set(RoboCssGeneratedHeader "${CMAKE_SOURCE_DIR}/CompilerKnowledge/Include/RoboCSSCompiler/GeneratedCssVocabulary.h")
set(RoboCssAuditFile "${CMAKE_SOURCE_DIR}/CompilerKnowledge/CssReferenceAudit.json")

add_custom_target(RoboExtractCssReferences
    COMMAND CssSpecExtractor
            "${RoboCssReferenceDirectory}"
            "${RoboGeneratedRegistryDirectory}"
            --header "${RoboCssGeneratedHeader}"
            --audit "${RoboCssAuditFile}"
    DEPENDS CssSpecExtractor
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Mining the CSS reference into the generated vocabulary"
    USES_TERMINAL
)

# Retiring a module is how the reference shrinks: the modules named in
# -DRoboCssRetireModules are recorded as consumed and their directories removed.
add_custom_target(RoboRetireCssReferences
    COMMAND CssSpecExtractor
            "${RoboCssReferenceDirectory}"
            "${RoboGeneratedRegistryDirectory}"
            --header "${RoboCssGeneratedHeader}"
            --audit "${RoboCssAuditFile}"
            --retire "${RoboCssRetireModules}"
    DEPENDS CssSpecExtractor
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Retiring consumed CSS reference modules"
    USES_TERMINAL
)

add_custom_target(RoboGenerateRegistries
    DEPENDS RoboExtractReferences RoboExtractCssReferences RoboCompileRegistries
    COMMENT "Regenerating the complete RoboHTMLCompiler registry pipeline"
)

set_target_properties(RoboExtractReferences RoboCompileRegistries RoboGenerateRegistries
    RoboAuditReferences RoboConsumeReferences
    RoboExtractCssReferences RoboRetireCssReferences
    PROPERTIES FOLDER "RoboHTMLCompilerTools")
