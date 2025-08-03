#pragma once
/*
 * Rendering Module Public API
 * 
 * Provides Vulkan-based NES-style rendering functionality.
 * Include this header to access rendering systems.
 */

// Public rendering API - this is the main interface
#include "rendering.h"

// Utilities that may be needed by game code
#include "rendering_util.h"

// Note: rendering_vulkan.cpp and shader internals are private implementation details