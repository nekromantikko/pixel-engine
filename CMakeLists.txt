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

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)

set(SOURCES 
		src/main.cpp
		src/collision.cpp
		src/game.cpp
		src/input.cpp
		src/rendering_util.cpp
		src/rendering_vulkan.cpp
		src/game_rendering.cpp
		src/tiles.cpp
		src/actors.cpp
		src/audio.cpp
		src/random.cpp
		src/coroutines.cpp
		src/dialog.cpp
		src/game_input.cpp
		src/game_state.cpp
		src/game_ui.cpp
		src/player.cpp
		src/enemy.cpp
		src/bullet.cpp
		src/pickup.cpp
		src/effect.cpp
		src/interactable.cpp
		src/actor_types.cpp
		src/spawner.cpp
		src/debug.cpp
		src/room.cpp
		src/overworld.cpp
		src/shader.cpp
		src/asset_manager.cpp)

set(HEADERS 
		src/rendering.h
		src/game.h
		src/input.h
		src/memory_pool.h
		src/metasprite.h
		src/rendering_util.h
		src/typedef.h
		src/game_rendering.h
		src/collision.h
		src/tiles.h
		src/actors.h)

option(ENABLE_EDITOR "Enable editor functionality" ON)

IF(ENABLE_EDITOR)
	set(EDITOR_INCLUDE_DIRS src/ext/imgui-filebrowser)
	set(EDITOR_HEADERS src/editor.h)
	set(EDITOR_SOURCES src/editor.cpp src/editor_serialization.cpp src/editor_asset_watcher.cpp)

	FetchContent_Declare(
        	imgui
		GIT_REPOSITORY https://github.com/ocornut/imgui.git
        	GIT_TAG v1.91.8
	)
	FetchContent_MakeAvailable(imgui)

	set(IMGUI_SOURCES 
		${imgui_SOURCE_DIR}/imgui.h
		${imgui_SOURCE_DIR}/imgui.cpp
		${imgui_SOURCE_DIR}/imconfig.h
		${imgui_SOURCE_DIR}/imgui_demo.cpp
		${imgui_SOURCE_DIR}/imgui_draw.cpp
		${imgui_SOURCE_DIR}/imgui_internal.h
		${imgui_SOURCE_DIR}/imstb_rectpack.h
		${imgui_SOURCE_DIR}/imstb_textedit.h
		${imgui_SOURCE_DIR}/imstb_truetype.h
		${imgui_SOURCE_DIR}/imgui_tables.cpp
		${imgui_SOURCE_DIR}/imgui_widgets.cpp
		${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
		${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
		${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)

	FetchContent_Declare(
		nlohmann_json
		GIT_REPOSITORY https://github.com/nlohmann/json.git
		GIT_TAG v3.12.0
	)
	FetchContent_MakeAvailable(nlohmann_json)
ENDIF()

add_executable(${PROJECT_NAME} WIN32 ${HEADERS} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS} ${glm_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${Vulkan_LIBRARY} SDL2::SDL2)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

IF(ENABLE_EDITOR)
	target_sources(${PROJECT_NAME} PRIVATE ${IMGUI_SOURCES} ${EDITOR_HEADERS} ${EDITOR_SOURCES})
	target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${nlohmann_json_INCLUDE_DIRS} ${EDITOR_INCLUDE_DIRS})
	target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json)
ENDIF()

# Assets
set(ASSETS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

target_compile_definitions(${PROJECT_NAME} PRIVATE ASSETS_SRC_DIR="${ASSETS_SRC_DIR}")

# Compile shaders
set (GLSL_SHADERS
        src/shaders/debug_blit_chr.comp
        src/shaders/debug_blit_pal.comp
	src/shaders/scanline_evaluate.comp
	src/shaders/software.comp
	src/shaders/quad.vert
	src/shaders/textured_crt.frag
	src/shaders/textured_raw.frag)

set(SHADER_DEST ${CMAKE_CURRENT_BINARY_DIR}/shaders)
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

IF(ENABLE_EDITOR)
	target_compile_definitions(${PROJECT_NAME} PUBLIC EDITOR)
ENDIF()