# Rendering module - Vulkan rendering system
set(RENDERING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/rendering_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rendering_vulkan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shader.cpp
)

set(RENDERING_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/rendering.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rendering_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/shader.h
)

# For now, we'll include these in the main target
# In the future, this could be its own library