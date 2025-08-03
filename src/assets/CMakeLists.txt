# Assets module library (shared data and asset management)
add_library(assets STATIC
    shared/debug.cpp
    shared/random.cpp
    shared/asset_serialization.cpp
    shared/asset_manager.cpp
)

target_include_directories(assets PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(assets PUBLIC
    nlohmann_json
)

target_include_directories(assets PUBLIC
    ${glm_SOURCE_DIR}
)

target_compile_features(assets PUBLIC cxx_std_20)

target_compile_definitions(assets PUBLIC 
    ASSETS_NPAK_OUTPUT="${ASSETS_NPAK_OUTPUT}"
)