#include <cstdio>

// Constants from core_types.h
constexpr unsigned int VIEWPORT_WIDTH_TILES = 64;
constexpr unsigned int VIEWPORT_HEIGHT_TILES = 36;
constexpr unsigned int TILE_DIM_PIXELS = 8;
constexpr unsigned int MAX_SPRITES_PER_SCANLINE = 64;
constexpr unsigned int SCANLINE_COUNT = VIEWPORT_HEIGHT_TILES * TILE_DIM_PIXELS; // 288
constexpr unsigned int COMMAND_BUFFER_COUNT = 2;
constexpr unsigned int COLOR_COUNT = 0x80; // 128

int main() {
    printf("=== Pixel Engine Memory Usage Analysis ===\n\n");
    
    // Calculate major buffer/image sizes
    const unsigned int color_image_width = VIEWPORT_WIDTH_TILES * TILE_DIM_PIXELS;  // 512
    const unsigned int color_image_height = VIEWPORT_HEIGHT_TILES * TILE_DIM_PIXELS; // 288
    const unsigned int color_image_size = color_image_width * color_image_height * 4; // RGBA8
    
    printf("Color Images:\n");
    printf("  Dimensions: %u x %u pixels\n", color_image_width, color_image_height);
    printf("  Size per image: %.2f MB\n", color_image_size / (1024.0f * 1024.0f));
    printf("  Count: %u (COMMAND_BUFFER_COUNT)\n", COMMAND_BUFFER_COUNT);
    printf("  Total: %.2f MB\n", (color_image_size * COMMAND_BUFFER_COUNT) / (1024.0f * 1024.0f));
    printf("\n");
    
    const unsigned int evaluated_sprite_buffer_size = (MAX_SPRITES_PER_SCANLINE + 1) * SCANLINE_COUNT * sizeof(unsigned int);
    printf("Evaluated Sprite Index Buffers:\n");
    printf("  Max sprites per scanline: %u\n", MAX_SPRITES_PER_SCANLINE);
    printf("  Scanline count: %u\n", SCANLINE_COUNT);
    printf("  Size per buffer: %.2f MB\n", evaluated_sprite_buffer_size / (1024.0f * 1024.0f));
    printf("  Count: %u (COMMAND_BUFFER_COUNT)\n", COMMAND_BUFFER_COUNT);
    printf("  Total: %.2f MB\n", (evaluated_sprite_buffer_size * COMMAND_BUFFER_COUNT) / (1024.0f * 1024.0f));
    printf("\n");
    
    const unsigned int palette_size = COLOR_COUNT * sizeof(unsigned int);
    printf("Palette Image:\n");
    printf("  Color count: %u\n", COLOR_COUNT);
    printf("  Size: %.2f KB\n", palette_size / 1024.0f);
    printf("\n");
    
    // Potential CHR memory (from constants)
    const unsigned int chr_size_estimation = 16 * 16 * 64 * 8; // rough estimate
    printf("Estimated CHR Memory: %.2f KB\n", chr_size_estimation / 1024.0f);
    printf("\n");
    
    // Compute buffer size calculation (this needs to be found in the code)
    printf("Estimated Vulkan GPU Memory Usage:\n");
    const float total_gpu_memory = (color_image_size * COMMAND_BUFFER_COUNT + 
                                   evaluated_sprite_buffer_size * COMMAND_BUFFER_COUNT + 
                                   palette_size + chr_size_estimation) / (1024.0f * 1024.0f);
    printf("  Total estimated: %.2f MB\n", total_gpu_memory);
    printf("\n");
    
    // Per-image breakdown
    printf("Detailed Memory Breakdown:\n");
    printf("  Color images: %.2f MB\n", (color_image_size * COMMAND_BUFFER_COUNT) / (1024.0f * 1024.0f));
    printf("  Sprite buffers: %.2f MB\n", (evaluated_sprite_buffer_size * COMMAND_BUFFER_COUNT) / (1024.0f * 1024.0f));
    printf("  Palette: %.2f KB\n", palette_size / 1024.0f);
    printf("  CHR (est): %.2f KB\n", chr_size_estimation / 1024.0f);
    printf("\n");
    
    // Potential memory hogs
    printf("Potential Memory Usage Issues:\n");
    printf("1. GPU driver overhead (can be 50-200MB)\n");
    printf("2. Vulkan validation layers (if enabled - adds ~100MB)\n");
    printf("3. SDL2 audio/video buffers\n");
    printf("4. Asset data in memory\n");
    printf("5. Debug/logging overhead\n");
    printf("6. Multiple swapchain images (3x framebuffer size)\n");
    printf("\n");
    
    printf("Optimization Recommendations:\n");
    if (color_image_size * COMMAND_BUFFER_COUNT > 2 * 1024 * 1024) {
        printf("- Color images are large (%.1f MB total). Consider:\n", 
               (color_image_size * COMMAND_BUFFER_COUNT) / (1024.0f * 1024.0f));
        printf("  * Use R8G8B8 instead of RGBA8 (25%% reduction)\n");
        printf("  * Reduce viewport resolution\n");
        printf("  * Use single buffering\n");
    }
    
    if (evaluated_sprite_buffer_size > 256 * 1024) {
        printf("- Sprite index buffers: %.1f MB total. Consider:\n",
               (evaluated_sprite_buffer_size * COMMAND_BUFFER_COUNT) / (1024.0f * 1024.0f));
        printf("  * Reduce MAX_SPRITES_PER_SCANLINE from %u\n", MAX_SPRITES_PER_SCANLINE);
        printf("  * Use 16-bit indices instead of 32-bit\n");
    }
    
    return 0;
}