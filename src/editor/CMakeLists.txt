# Editor static library
set(EDITOR_SOURCES 
	editor.cpp 
	editor_assets.cpp)

set(EDITOR_INCLUDE_DIRS ext/imgui-filebrowser)

add_library(pixelengine_editor STATIC ${EDITOR_SOURCES})

# Include directories for editor
target_include_directories(pixelengine_editor PRIVATE 
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${CMAKE_CURRENT_SOURCE_DIR}/${EDITOR_INCLUDE_DIRS}
	${imgui_SOURCE_DIR} 
	${imgui_SOURCE_DIR}/backends
	${glm_SOURCE_DIR}
	${Vulkan_INCLUDE_DIRS})

# Link required libraries
target_link_libraries(pixelengine_editor PRIVATE 
	nlohmann_json 
	slang::slang
	${Vulkan_LIBRARY} 
	SDL2::SDL2)

# Compile definitions
target_compile_definitions(pixelengine_editor PRIVATE 
	EDITOR
	ASSETS_SRC_DIR="${ASSETS_SRC_DIR}")

# Platform-specific definitions
if(WIN32)
	target_compile_definitions(pixelengine_editor PRIVATE PLATFORM_WINDOWS _CRT_SECURE_NO_WARNINGS)
endif()

if(UNIX AND NOT APPLE)
	target_compile_definitions(pixelengine_editor PRIVATE PLATFORM_LINUX)
endif()

# Compile options
target_compile_options(pixelengine_editor PRIVATE ${COMPILER_FLAGS})