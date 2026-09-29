# ---------------------------------------------------------------------------
# RoboHTMLCompilerTools
#
#     Reference/HTML/  --SpecExtractor-->  Generated/Registry/*.json
#     Registry/*.json  --RegistryCompiler-->  Generated/Include, Generated/Source
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

# ---------------------------------------------------------------------------
# Regeneration pipeline
# ---------------------------------------------------------------------------

add_custom_target(RoboExtractReferences
    COMMAND SpecExtractor
            "${RoboReferenceDirectory}"
            "${RoboGeneratedRegistryDirectory}"
            "${CMAKE_SOURCE_DIR}/Generated/ImplementationCoverage.json"
    DEPENDS SpecExtractor
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Mining the standards reference into registry JSON"
    USES_TERMINAL
)

add_custom_target(RoboCompileRegistries
    COMMAND RegistryCompiler
            "${RoboRegistrySourceDirectory}"
            "${RoboGeneratedRegistryDirectory}"
            "${CMAKE_SOURCE_DIR}/Generated/Include/RoboHTMLCompiler"
            "${CMAKE_SOURCE_DIR}/Generated/Source"
            "${CMAKE_SOURCE_DIR}/Generated/ImplementationCoverage.json"
    DEPENDS RegistryCompiler
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating strongly typed registry tables"
    USES_TERMINAL
)

add_custom_target(RoboGenerateRegistries
    DEPENDS RoboExtractReferences RoboCompileRegistries
    COMMENT "Regenerating the complete RoboHTMLCompiler registry pipeline"
)

set_target_properties(RoboExtractReferences RoboCompileRegistries RoboGenerateRegistries
    PROPERTIES FOLDER "RoboHTMLCompilerTools")
