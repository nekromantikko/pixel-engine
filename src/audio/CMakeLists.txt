# Audio module - SDL2 audio system
set(AUDIO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/audio.cpp
)

set(AUDIO_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/audio.h
)

# For now, we'll include these in the main target
# In the future, this could be its own library