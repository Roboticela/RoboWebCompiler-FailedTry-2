cmake_minimum_required(VERSION 3.20)

project(RoboHTMLCompiler
    VERSION 0.1.0
    DESCRIPTION "RoboHTMLCompiler - strict HTML/SVG/MathML to native C++ data model compiler"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global language and output configuration
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(RoboConfiguration Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${RoboConfiguration}" RoboConfigurationUpper)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${RoboConfigurationUpper} "${CMAKE_BINARY_DIR}/bin")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${RoboConfigurationUpper} "${CMAKE_BINARY_DIR}/lib")
    endforeach()
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(RoboBuildTests "Build the RoboHTMLCompilerTests target" ON)
option(RoboBuildTools "Build the RoboHTMLCompilerTools targets" ON)
option(RoboWarningsAsErrors "Treat compiler warnings as errors" OFF)

# ---------------------------------------------------------------------------
# Diagnostics: optional toolchain extraction / registry generation
# ---------------------------------------------------------------------------

# Directory holding the hand-authored registries plus the JSON produced by the
# SpecExtractor tool out of the standards reference.
set(RoboRegistrySourceDirectory "${CMAKE_SOURCE_DIR}/Registry" CACHE PATH "Hand-authored registry JSON directory")
set(RoboGeneratedRegistryDirectory "${CMAKE_SOURCE_DIR}/CompilerKnowledge/Vocabulary" CACHE PATH "Extracted registry JSON directory")

# The standards reference is an implementation checklist that has been fully
# consumed; the directory is optional. When it is absent the extraction stage
# reports that and leaves CompilerKnowledge/Vocabulary/ authoritative.
set(RoboReferenceDirectory "${CMAKE_SOURCE_DIR}/Reference/HTML" CACHE PATH "Standards reference directory (optional)")

# The CSS reference is the CSS Working Group's drafts. It is mined by
# CssSpecExtractor into CompilerKnowledge/Vocabulary/CssVocabulary.json and the
# generated header, and retired module by module as the compiler accounts for it.
set(RoboCssReferenceDirectory "${CMAKE_SOURCE_DIR}/Reference/CSS" CACHE PATH "CSS standards reference directory (optional)")
set(RoboCssRetireModules "" CACHE STRING "CSS reference modules to retire (comma separated)")

# Default target name for the compiler executable.
set(RoboCompilerExecutableName "RoboHTMLCompiler")

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------

add_subdirectory(Source)

if(RoboBuildTools)
    add_subdirectory(Tools)
endif()

if(RoboBuildTests)
    enable_testing()
    add_subdirectory(Tests)
endif()

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

message(STATUS "")
message(STATUS "RoboHTMLCompiler ${PROJECT_VERSION}")
message(STATUS "  build type ......... ${CMAKE_BUILD_TYPE}")
message(STATUS "  reference directory  ${RoboReferenceDirectory}")
message(STATUS "  generated output ... ${CMAKE_BINARY_DIR}/GeneratedHTML")
message(STATUS "  tests .............. ${RoboBuildTests}")
message(STATUS "  tools .............. ${RoboBuildTools}")
message(STATUS "")
