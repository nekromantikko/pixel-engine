# Game module library - all gameplay logic
add_library(game STATIC
    collision.cpp
    game.cpp
    game_rendering.cpp
    tilemap.cpp
    actors.cpp
    coroutines.cpp
    dialog.cpp
    game_input.cpp
    game_state.cpp
    game_ui.cpp
    player.cpp
    enemy.cpp
    bullet.cpp
    pickup.cpp
    effect.cpp
    interactable.cpp
    spawner.cpp
)

target_include_directories(game PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(game PUBLIC
    core
    rendering  
    audio
    assets
)

target_compile_features(game PUBLIC cxx_std_20)