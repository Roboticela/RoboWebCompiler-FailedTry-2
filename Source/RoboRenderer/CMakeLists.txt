# ---------------------------------------------------------------------------
# RoboRenderer - the native runtime and rendering system.
#
# RoboRenderer consumes the compiled UI data that RoboHTMLCompiler emits. It
# depends on the compiler for exactly one thing, the generated ElementKind enum
# (CompilerKnowledge/Include), which is the native category vocabulary both
# sides share. It never links the compiler and it never parses HTML or CSS.
# ---------------------------------------------------------------------------

set(RoboRendererSources
    "${CMAKE_CURRENT_SOURCE_DIR}/CompiledUi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Unicode.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Generated/UnicodeData.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CompiledUiValidation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Style.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Text.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/FontFile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CffOutline.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/FontTables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/SvgGlyph.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/SvgImage.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/FontVariations.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenTypeLayout.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/FontStack.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Bidi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StrokeFont.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RuntimeState.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/LayoutEngine.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PaintEngine.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Image.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ResourceManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/SoftwareBackend.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Input.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Renderer.cpp"
)

add_library(RoboRenderer STATIC ${RoboRendererSources})

target_include_directories(RoboRenderer
    PUBLIC
        "${CMAKE_SOURCE_DIR}/Include"
        "${CMAKE_SOURCE_DIR}/CompilerKnowledge/Include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(RoboRenderer PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(RoboRenderer PRIVATE /W4 /permissive-)
    if(RoboWarningsAsErrors)
        target_compile_options(RoboRenderer PRIVATE /WX)
    endif()
else()
    target_compile_options(RoboRenderer PRIVATE -Wall -Wextra -Wpedantic)
    if(RoboWarningsAsErrors)
        target_compile_options(RoboRenderer PRIVATE -Werror)
    endif()
endif()

# Debug build row: element boxes, identifiers, layout boxes, hit-test regions,
# clipping and dirty regions, paint commands. A release build compiles the debug
# renderer out and removes its per-frame bookkeeping with it.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(RoboRenderer PUBLIC ROBO_RENDERER_DEBUG=1)
else()
    target_compile_definitions(RoboRenderer PUBLIC ROBO_RENDERER_DEBUG=0)
endif()

set_target_properties(RoboRenderer PROPERTIES FOLDER "RoboRenderer")
