# Core module - Basic engine systems
set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/input.cpp
)

set(CORE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/input.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fixed_hash_map.h
    ${CMAKE_CURRENT_SOURCE_DIR}/nes_timing.h
)

# For now, we'll include these in the main target
# In the future, this could be its own library