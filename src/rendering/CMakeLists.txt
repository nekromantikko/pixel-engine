# Rendering module library
add_library(rendering STATIC
    rendering_util.cpp
    rendering_vulkan.cpp
    shader.cpp
)

target_include_directories(rendering PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rendering PUBLIC
    assets  # For shared rendering types
    ${Vulkan_LIBRARY}
    SDL2::SDL2
)

target_compile_features(rendering PUBLIC cxx_std_20)