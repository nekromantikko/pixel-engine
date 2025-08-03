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

FetchContent_Declare(
		nlohmann_json
		GIT_REPOSITORY https://github.com/nlohmann/json.git
		GIT_TAG v3.12.0
	)
FetchContent_MakeAvailable(nlohmann_json)

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)

set(SHARED_INCLUDE_DIR src/shared)
set(SHARED_SOURCES 
	src/shared/debug.cpp
	src/shared/random.cpp
	src/shared/asset_serialization.cpp
	src/shared/asset_manager.cpp)

set(SOURCES 
		src/main.cpp
		src/collision.cpp
		src/game.cpp
		src/input.cpp
		src/rendering_util.cpp
		src/rendering_vulkan.cpp
		src/game_rendering.cpp
		src/tilemap.cpp
		src/actors.cpp
		src/audio.cpp
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
		src/spawner.cpp
		src/shader.cpp)

option(ENABLE_EDITOR "Enable editor functionality" ON)
option(USE_AVX "Use AVX implementation where applicable" ON)

set(ASSETS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(ASSETS_NPAK_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/assets.npak")

# Asset Packer Tool
set (ASSET_PACKER_SOURCES src/tools/asset_packer.cpp)

add_executable(asset_packer ${SHARED_SOURCES} ${ASSET_PACKER_SOURCES})
target_include_directories(asset_packer PRIVATE ${SHARED_INCLUDE_DIR} ${glm_SOURCE_DIR})
target_compile_features(asset_packer PUBLIC cxx_std_20)
target_compile_definitions(asset_packer PRIVATE EDITOR ASSETS_SRC_DIR="${ASSETS_SRC_DIR}")
target_link_libraries(asset_packer PRIVATE nlohmann_json)
target_compile_definitions(asset_packer PUBLIC EDITOR)

if (MSVC)
    target_compile_options(asset_packer PUBLIC /Zc:preprocessor)
endif()

IF(ENABLE_EDITOR)
	set(EDITOR_INCLUDE_DIRS src/ext/imgui-filebrowser)
	set(EDITOR_SOURCES src/editor.cpp src/editor_assets.cpp)

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
		${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
		
	# Add platform-specific backends
	IF(WIN32)
		list(APPEND IMGUI_SOURCES ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp)
	ENDIF()
ENDIF()

add_executable(${PROJECT_NAME} WIN32 ${SHARED_SOURCES} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${SHARED_INCLUDE_DIR} ${Vulkan_INCLUDE_DIRS} ${glm_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${Vulkan_LIBRARY} SDL2::SDL2 SDL2::SDL2main)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
target_compile_definitions(${PROJECT_NAME} PRIVATE ASSETS_NPAK_OUTPUT="${ASSETS_NPAK_OUTPUT}")

IF(ENABLE_EDITOR)
	target_sources(${PROJECT_NAME} PRIVATE ${IMGUI_SOURCES} ${EDITOR_SOURCES})
	target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${EDITOR_INCLUDE_DIRS})
	target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json)
	target_compile_definitions(${PROJECT_NAME} PRIVATE ASSETS_SRC_DIR="${ASSETS_SRC_DIR}")
ENDIF()

if (MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC /Zc:preprocessor)
endif()

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
file(MAKE_DIRECTORY ${SHADER_DEST})
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

# Generate assets.npak from source assets
add_custom_target(generate_assets ALL
	COMMAND asset_packer "${ASSETS_SRC_DIR}" "${ASSETS_NPAK_OUTPUT}"
	DEPENDS asset_packer
	COMMENT "Generating assets.npak from source assets"
)
add_dependencies(${PROJECT_NAME} generate_assets)

IF(WIN32)
	set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
	target_compile_definitions(${PROJECT_NAME} PUBLIC PLATFORM_WINDOWS)
ENDIF()

IF(UNIX AND NOT APPLE)
	set(USE_AVX OFF)
	target_compile_definitions(${PROJECT_NAME} PUBLIC PLATFORM_LINUX)
ENDIF()

IF(ENABLE_EDITOR)
	target_compile_definitions(${PROJECT_NAME} PUBLIC EDITOR)
ENDIF()

IF(USE_AVX)
	target_compile_definitions(${PROJECT_NAME} PUBLIC USE_AVX)
ENDIF()