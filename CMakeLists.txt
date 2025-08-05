cmake_minimum_required(VERSION 3.22.1)
set(PROJECT_NAME pixelengine)
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if(MSVC)
set(COMPILER_FLAGS
	"/arch:AVX2" # Enable AVX2 instructions
	"/Zc:preprocessor" # Enable C++20 preprocessor features
	"/permissive-" # Disable MSVC's non-standard C++ extensions
	"/W4" # Set warning level to 4
	"/wd4100" # Disable warning C4100: unreferenced formal parameter
	"/wd4101" # Disable warning C4101: unreferenced local variable
	"/wd4189" # Disable warning C4189: local variable is initialized but not referenced
	"/wd4201" # Disable warning C4201: nonstandard extension used: nameless struct/union
	"/wd4324" # Disable warning C4324: structure was padded due to alignment specifier
	"/wd4505" # Disable warning C4505: unreferenced local function has been removed
)
else()
set(COMPILER_FLAGS
	"-mavx2"
	"-Wall"
	"-Wextra"
	"-Wno-unused-parameter"
	"-Wno-unused-variable"
	"-Wno-unused-but-set-variable"
	"-Wno-gnu-anonymous-struct"
	"-Wno-padded"
	"-Wno-unused-function"
)
endif()

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
find_package(slang QUIET)

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
		src/random.cpp
		src/debug.cpp
		src/asset_archive.cpp
		src/asset_manager.cpp)

option(ENABLE_EDITOR "Enable editor functionality" ON)

set(ASSETS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(ASSETS_NPAK_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/assets.npak")

# Asset Packer Tool (only if slang is available)
if(slang_FOUND)
	set (ASSET_PACKER_SOURCES 
		src/tools/asset_packer.cpp 
		src/asset_archive.cpp 
		src/asset_serialization.cpp
		src/shader_compiler.cpp)

	add_executable(asset_packer ${ASSET_PACKER_SOURCES})
	target_include_directories(asset_packer PRIVATE ${glm_SOURCE_DIR})
	target_link_libraries(asset_packer PRIVATE nlohmann_json slang::slang)
	target_compile_definitions(asset_packer PRIVATE EDITOR)
	target_compile_options(asset_packer PRIVATE ${COMPILER_FLAGS})

	if(MSVC)
		target_compile_definitions(asset_packer PRIVATE _CRT_SECURE_NO_WARNINGS)
	endif()
else()
	message(STATUS "Slang not found, asset_packer will not be built")
endif()

IF(ENABLE_EDITOR)
	set(EDITOR_INCLUDE_DIRS src/ext/imgui-filebrowser)
	set(EDITOR_SOURCES 
		src/editor.cpp 
		src/editor_assets.cpp
		src/asset_serialization.cpp
		src/shader_compiler.cpp)

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
target_compile_definitions(${PROJECT_NAME} PRIVATE ASSETS_NPAK_OUTPUT="${ASSETS_NPAK_OUTPUT}")
target_compile_options(${PROJECT_NAME} PRIVATE ${COMPILER_FLAGS})

IF(ENABLE_EDITOR)
	target_sources(${PROJECT_NAME} PRIVATE ${IMGUI_SOURCES} ${EDITOR_SOURCES})
	target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${EDITOR_INCLUDE_DIRS})
	target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json)
	if(slang_FOUND)
		target_link_libraries(${PROJECT_NAME} PRIVATE slang::slang)
	endif()
	target_compile_definitions(${PROJECT_NAME} PRIVATE ASSETS_SRC_DIR="${ASSETS_SRC_DIR}")
ENDIF()

if(MSVC)
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Generate assets.npak from source assets (only if asset_packer was built)
if(slang_FOUND)
	add_custom_target(generate_assets ALL
		COMMAND asset_packer "${ASSETS_SRC_DIR}" "${ASSETS_NPAK_OUTPUT}"
		DEPENDS asset_packer
		COMMENT "Generating assets.npak from source assets"
	)
	add_dependencies(${PROJECT_NAME} generate_assets)
else()
	message(STATUS "Skipping asset packing - slang not available")
endif()

IF(WIN32)
	set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
	target_compile_definitions(${PROJECT_NAME} PUBLIC PLATFORM_WINDOWS)
ENDIF()

IF(UNIX AND NOT APPLE)
	target_compile_definitions(${PROJECT_NAME} PUBLIC PLATFORM_LINUX)
ENDIF()

IF(ENABLE_EDITOR)
	target_compile_definitions(${PROJECT_NAME} PUBLIC EDITOR)
ENDIF()