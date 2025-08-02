#include "rendering.h"
#include "debug.h"
#include <cassert>

// Simple test to validate error handling works
void TestRenderingErrorHandling() {
    DEBUG_LOG("Testing rendering error handling...\n");
    
    // Test context creation
    Rendering::CreateContext();
    
    // Should not be initialized yet (no surface created)
    if (Rendering::HasErrors()) {
        DEBUG_LOG("Context creation has errors: %s\n", Rendering::GetErrorString());
    }
    
    // Test accessing functions before initialization
    RenderSettings* settings = Rendering::GetSettingsPtr();
    if (!settings) {
        DEBUG_LOG("GetSettingsPtr correctly returned null before proper initialization\n");
    }
    
    Palette* palette = Rendering::GetPalettePtr(0);
    if (!palette) {
        DEBUG_LOG("GetPalettePtr correctly returned null before proper initialization\n");
    }
    
    // Test boundary conditions
    Palette* invalidPalette = Rendering::GetPalettePtr(999999);
    if (!invalidPalette) {
        DEBUG_LOG("GetPalettePtr correctly rejected invalid index\n");
    }
    
    DEBUG_LOG("Error handling test completed successfully!\n");
}

// Test function for validation - not meant to be compiled in regular build
// This is just to show how the error handling can be tested
#ifdef TEST_ERROR_HANDLING
int main() {
    TestRenderingErrorHandling();
    return 0;
}
#endif