# Audio module library
add_library(audio STATIC
    audio.cpp
)

target_include_directories(audio PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(audio PUBLIC
    SDL2::SDL2
)

target_compile_features(audio PUBLIC cxx_std_20)