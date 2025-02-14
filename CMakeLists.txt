cmake_minimum_required(VERSION 3.22.1)
set(PROJECT_NAME pixelengine)
project(${PROJECT_NAME})

# For FetchContent_Declare() and FetchContent_MakeAvailable()
include(FetchContent)

FetchContent_Declare(
        glm
        URL_HASH MD5=7d235d4813a2e7b1e10cc711b8e25213
        URL https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip
)
FetchContent_MakeAvailable(glm)

set(IMGUI_PATH $ENV{IMGUI_PATH})
set(IMGUI_SOURCES 
		${IMGUI_PATH}/imgui.h
		${IMGUI_PATH}/imgui.cpp
		${IMGUI_PATH}/imconfig.h
		${IMGUI_PATH}/imgui_demo.cpp
		${IMGUI_PATH}/imgui_draw.cpp
		${IMGUI_PATH}/imgui_internal.h
		${IMGUI_PATH}/imstb_rectpack.h
		${IMGUI_PATH}/imstb_textedit.h
		${IMGUI_PATH}/imstb_truetype.h
		${IMGUI_PATH}/imgui_tables.cpp
		${IMGUI_PATH}/imgui_widgets.cpp
		${IMGUI_PATH}/backends/imgui_impl_sdl2.cpp
		${IMGUI_PATH}/backends/imgui_impl_win32.cpp
		${IMGUI_PATH}/backends/imgui_impl_vulkan.cpp)

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)

set(SOURCES 
		src/main.cpp
		src/collision.cpp
		src/game.cpp
		src/input.cpp
		src/level.cpp
		src/metasprite.cpp
		src/rendering_util.cpp
		src/rendering_vulkan.cpp
		src/system.cpp
		src/viewport.cpp
		src/tiles.cpp
		src/actors.cpp
		src/audio.cpp)

set(EDITOR_SOURCES
		src/editor.cpp)

set(HEADERS 
		src/rendering.h
		src/game.h
		src/input.h
		src/level.h
		src/memory_pool.h
		src/metasprite.h
		src/rendering_util.h
		src/system.h
		src/typedef.h
		src/viewport.h
		src/collision.h
		src/tiles.h
		src/actors.h)

set(EDITOR_HEADERS
		src/editor.h)

add_executable(${PROJECT_NAME} WIN32 ${HEADERS} ${SOURCES} ${IMGUI_SOURCES} ${EDITOR_HEADERS} ${EDITOR_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_PATH} ${IMGUI_PATH}/backends ${SDL2_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS} ${glm_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${Vulkan_LIBRARY} SDL2::SDL2)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

# Compile shaders
set (GLSL_SHADERS
        src/shaders/debug_blit_chr.comp
        src/shaders/debug_blit_pal.comp
		src/shaders/scanline_evaluate.comp
		src/shaders/software.comp
		src/shaders/quad.vert
		src/shaders/textured_crt.frag
		src/shaders/textured_raw.frag)

set(SHADER_DEST ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)
include(${CMAKE_CURRENT_SOURCE_DIR}/glsl_shader.cmake)
set_source_files_properties(src/shaders/debug_blit_chr.comp PROPERTIES ShaderType "comp")
set_source_files_properties(src/shaders/debug_blit_pal.comp PROPERTIES ShaderType "comp")
set_source_files_properties(src/shaders/scanline_evaluate.comp PROPERTIES ShaderType "comp")
set_source_files_properties(src/shaders/software.comp PROPERTIES ShaderType "comp")
set_source_files_properties(src/shaders/quad.vert PROPERTIES ShaderType "vert")
set_source_files_properties(src/shaders/textured_crt.frag PROPERTIES ShaderType "frag")
set_source_files_properties(src/shaders/textured_raw.frag PROPERTIES ShaderType "frag")

foreach(FILE ${GLSL_SHADERS})
    get_filename_component(FILE_WE ${FILE} NAME_WE)
    get_source_file_property(shadertype ${FILE} ShaderType)
    glsl_spv_shader(
            INPUT "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
            OUTPUT "${SHADER_DEST}/${FILE_WE}.spv"
            STAGE ${shadertype}
            ENTRY_POINT main
            TARGET_ENV vulkan1.0
    )
    # Make our project depend on these files
    target_sources(${PROJECT_NAME} PRIVATE "${SHADER_DEST}/${FILE_WE}.spv")
endforeach()

# Copy assets to build directory
set(ASSETS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(ASSETS_DST_DIR "${CMAKE_CURRENT_BINARY_DIR}/assets")

# Ensure the assets directory exists in the build folder
file(MAKE_DIRECTORY ${ASSETS_DST_DIR})

# Custom target that copies assets on every build
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_SRC_DIR} ${ASSETS_DST_DIR}
    COMMENT "Copying assets to build directory"
)
add_dependencies(${PROJECT_NAME} copy_assets)

IF(WIN32)
	target_compile_definitions(${PROJECT_NAME} PUBLIC PLATFORM_WINDOWS)
ENDIF()

target_compile_definitions(${PROJECT_NAME} PUBLIC EDITOR)