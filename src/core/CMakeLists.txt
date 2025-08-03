# Core module library
add_library(core STATIC
    input.cpp
)

target_include_directories(core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(core PUBLIC
    SDL2::SDL2
)

target_compile_features(core PUBLIC cxx_std_20)