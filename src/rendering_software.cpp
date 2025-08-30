#include "rendering.h"

void Rendering::Init(SDL_Window* sdlWindow) {
    // This is just a stub to allow compilation when using the software renderer.
    // Actual implementation is in rendering_vulkan.cpp
    (void)sdlWindow;
}

void Rendering::Free() {
    // This is just a stub to allow compilation when using the software renderer.
}

void Rendering::BeginFrame() {
    // This is just a stub to allow compilation when using the software renderer.
}

void Rendering::BeginRenderPass() {
    // This is just a stub to allow compilation when using the software renderer.
}

void Rendering::EndFrame() {
    // This is just a stub to allow compilation when using the software renderer.
}

void Rendering::WaitForAllCommands() {
    // This is just a stub to allow compilation when using the software renderer.
}

void Rendering::ResizeSurface(u32 width, u32 height) {
    // This is just a stub to allow compilation when using the software renderer.
    (void)width;
    (void)height;
}

RenderSettings* Rendering::GetSettingsPtr() {
    // This is just a stub to allow compilation when using the software renderer.
    return nullptr;
}

Palette* Rendering::GetPalettePtr(u32 paletteIndex) {
    // This is just a stub to allow compilation when using the software renderer.
    (void)paletteIndex;
    return nullptr;
}

Sprite* Rendering::GetSpritesPtr(u32 offset) {
    // This is just a stub to allow compilation when using the software renderer.
    (void)offset;
    return nullptr;
}

ChrSheet* Rendering::GetChrPtr(u32 sheetIndex) {
    // This is just a stub to allow compilation when using the software renderer.
    (void)sheetIndex;
    return nullptr;
}

Nametable* Rendering::GetNametablePtr(u32 index) {
    // This is just a stub to allow compilation when using the software renderer.
    (void)index;
    return nullptr;
}

Scanline* Rendering::GetScanlinePtr(u32 offset) {
    // This is just a stub to allow compilation when using the software renderer.
    (void)offset;
    return nullptr;
}

#pragma region Editor
#ifdef EDITOR
void Rendering::InitEditor(SDL_Window* sdlWindow) {
    // This is just a stub to allow compilation when using the software renderer.
    (void)sdlWindow;
}

void Rendering::BeginEditorFrame() {
    // This is just a stub to allow compilation when using the software renderer.
}

void Rendering::ShutdownEditor() {
    // This is just a stub to allow compilation when using the software renderer.
}

size_t Rendering::GetEditorBufferSize() {
    // This is just a stub to allow compilation when using the software renderer.
    return 0;
}

bool Rendering::InitEditorBuffer(EditorRenderBuffer* pBuffer, size_t size, const void* data) {
    return false;
}
bool Rendering::UpdateEditorBuffer(const EditorRenderBuffer* pBuffer, const void* data) {
    return false;
}
void Rendering::FreeEditorBuffer(EditorRenderBuffer* pBuffer) {
    // This is just a stub to allow compilation when using the software renderer.
}

size_t Rendering::GetEditorTextureSize() {
    return 0;
}
bool Rendering::InitEditorTexture(EditorRenderTexture* pTexture, u32 width, u32 height, u32 usage, u32 chrSheetOffset, u32 chrPaletteOffset, const EditorRenderBuffer* pChrBuffer, const EditorRenderBuffer* pPaletteBuffer) {
    return false;
}
bool Rendering::UpdateEditorTexture(const EditorRenderTexture* pTexture, const EditorRenderBuffer* pChrBuffer, const EditorRenderBuffer* pPaletteBuffer) {
    return false;
}
void* Rendering::GetEditorTextureData(const EditorRenderTexture* pTexture) {
    return nullptr;
}
void Rendering::FreeEditorTexture(EditorRenderTexture* pTexture) {
    // This is just a stub to allow compilation when using the software renderer.
}

// Software
void Rendering::RenderEditorTexture(const EditorRenderTexture* pTexture) {
    // This is just a stub to allow compilation when using the software renderer.
}

// Render pass
void Rendering::RenderEditor() {
    // This is just a stub to allow compilation when using the software renderer.
}
#endif
#pragma endregion
