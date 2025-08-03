#ifdef USE_OPENGL

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL_opengl.h>
#include <stdlib.h>
#include <cstring>
#include "rendering.h"
#include "rendering_util.h"
#include "debug.h"
#include <cassert>
#include <vector>

#ifdef EDITOR
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#endif

static constexpr u32 COMMAND_BUFFER_COUNT = 2;

struct Quad {
	r32 x, y, w, h;
};
static constexpr Quad DEFAULT_QUAD = { 0, 0, 1, 1 };

struct ScanlineData {
	u32 spriteCount;
	u32 spriteIndices[MAX_SPRITES_PER_SCANLINE];

	s32 scrollX;
	s32 scrollY;
};

struct GLImage {
	u32 width, height;
	GLuint texture;
};

struct GLBuffer {
	u32 size;
	GLuint buffer;
};

struct RenderContext {
	SDL_GLContext glContext;

	// Compute program and shaders
	GLuint softwareComputeProgram;
	GLuint scanlineEvaluateComputeProgram;
	
	// Graphics programs and shaders
	GLuint quadVertexShader;
	GLuint rawFragmentShader;
	GLuint crtFragmentShader;
	GLuint rawProgram;
	GLuint crtProgram;

	// Vertex Array Object
	GLuint quadVAO;

	// General stuff
	GLuint paletteTexture;

	// Compute data
	u32 paletteTableOffset;
	u32 paletteTableSize;
	u32 chrOffset;
	u32 chrSize;
	u32 nametableOffset;
	u32 nametableSize;
	u32 oamOffset;
	u32 oamSize;
	u32 renderStateOffset;
	u32 renderStateSize;
	u32 computeBufferSize;

	void* renderData;
	GLBuffer computeBuffer[COMMAND_BUFFER_COUNT];
	GLBuffer scanlineBuffers[COMMAND_BUFFER_COUNT];

	GLImage colorImages[COMMAND_BUFFER_COUNT];

	u32 currentFrameIndex;

	// Settings
	RenderSettings settings;

#ifdef EDITOR
	// Editor stuff - minimal for now
#endif
};

static RenderContext* pContext = nullptr;

// Helper function to compile shaders
static GLuint CompileShader(GLenum type, const char* source) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	GLint compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLint logLength;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		char* log = (char*)malloc(logLength);
		glGetShaderInfoLog(shader, logLength, nullptr, log);
		DEBUG_ERROR("Shader compilation failed: %s\n", log);
		free(log);
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

// Helper function to link programs
static GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader) {
	GLuint program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	GLint linked;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		GLint logLength;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
		char* log = (char*)malloc(logLength);
		glGetProgramInfoLog(program, logLength, nullptr, log);
		DEBUG_ERROR("Program linking failed: %s\n", log);
		free(log);
		glDeleteProgram(program);
		return 0;
	}

	return program;
}

// Helper function to load shader from file
static char* LoadShaderSource(const char* filename) {
	FILE* file = fopen(filename, "r");
	if (!file) {
		DEBUG_ERROR("Failed to open shader file: %s\n", filename);
		return nullptr;
	}

	fseek(file, 0, SEEK_END);
	long length = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* source = (char*)malloc(length + 1);
	fread(source, 1, length, file);
	source[length] = '\0';

	fclose(file);
	return source;
}

void Rendering::CreateContext() {
	pContext = (RenderContext*)calloc(1, sizeof(RenderContext));
	
	// Set OpenGL attributes
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
}

void Rendering::CreateSurface(SDL_Window* sdlWindow) {
	pContext->glContext = SDL_GL_CreateContext(sdlWindow);
	if (!pContext->glContext) {
		DEBUG_ERROR("Failed to create OpenGL context: %s\n", SDL_GetError());
		return;
	}

	SDL_GL_MakeCurrent(sdlWindow, pContext->glContext);
	
	// Enable VSync
	SDL_GL_SetSwapInterval(1);

	DEBUG_LOG("OpenGL Version: %s\n", glGetString(GL_VERSION));
	DEBUG_LOG("OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
}

void Rendering::Init() {
	// TODO: Implement shader loading and buffer creation
	// This is a complex function that needs careful implementation
	DEBUG_LOG("OpenGL rendering backend initialized\n");
}

void Rendering::Free() {
	// TODO: Cleanup OpenGL resources
}

void Rendering::DestroyContext() {
	if (pContext) {
		if (pContext->glContext) {
			SDL_GL_DeleteContext(pContext->glContext);
		}
		free(pContext);
		pContext = nullptr;
	}
}

void Rendering::BeginFrame() {
	pContext->currentFrameIndex = (pContext->currentFrameIndex + 1) % COMMAND_BUFFER_COUNT;
}

void Rendering::BeginRenderPass() {
	glClear(GL_COLOR_BUFFER_BIT);
}

void Rendering::EndFrame() {
	// TODO: Present the frame
}

void Rendering::WaitForAllCommands() {
	glFinish();
}

void Rendering::ResizeSurface(u32 width, u32 height) {
	glViewport(0, 0, width, height);
}

// Data access functions
RenderSettings* Rendering::GetSettingsPtr() {
	return &pContext->settings;
}

Palette* Rendering::GetPalettePtr(u32 paletteIndex) {
	if (!pContext->renderData) return nullptr;
	return (Palette*)((u8*)pContext->renderData + pContext->paletteTableOffset) + paletteIndex;
}

Sprite* Rendering::GetSpritesPtr(u32 offset) {
	if (!pContext->renderData) return nullptr;
	return (Sprite*)((u8*)pContext->renderData + pContext->oamOffset) + offset;
}

ChrSheet* Rendering::GetChrPtr(u32 sheetIndex) {
	if (!pContext->renderData) return nullptr;
	return (ChrSheet*)((u8*)pContext->renderData + pContext->chrOffset) + sheetIndex;
}

Nametable* Rendering::GetNametablePtr(u32 index) {
	if (!pContext->renderData) return nullptr;
	return (Nametable*)((u8*)pContext->renderData + pContext->nametableOffset) + index;
}

Scanline* Rendering::GetScanlinePtr(u32 offset) {
	if (!pContext->renderData) return nullptr;
	return (Scanline*)((u8*)pContext->renderData + pContext->renderStateOffset) + offset;
}

#ifdef EDITOR
void Rendering::InitEditor(SDL_Window* pWindow) {
	// TODO: Initialize ImGui with OpenGL backend
}

void Rendering::BeginEditorFrame() {
	// TODO: Begin ImGui frame
}

void Rendering::ShutdownEditor() {
	// TODO: Shutdown ImGui
}

EditorRenderBuffer* Rendering::CreateEditorBuffer(u32 size, const void* data) {
	// TODO: Implement editor buffer creation
	return nullptr;
}

bool Rendering::UpdateEditorBuffer(const EditorRenderBuffer* pBuffer, const void* data) {
	// TODO: Implement editor buffer update
	return false;
}

void Rendering::FreeEditorBuffer(EditorRenderBuffer* pBuffer) {
	// TODO: Implement editor buffer cleanup
}

EditorRenderTexture* Rendering::CreateEditorTexture(u32 width, u32 height, u32 usage, const EditorRenderBuffer* pChrBuffer, const EditorRenderBuffer* pPaletteBuffer) {
	// TODO: Implement editor texture creation
	return nullptr;
}

bool Rendering::UpdateEditorTexture(const EditorRenderTexture* pTexture, const EditorRenderBuffer* pChrBuffer, const EditorRenderBuffer* pPaletteBuffer) {
	// TODO: Implement editor texture update
	return false;
}

void* Rendering::GetEditorTextureData(const EditorRenderTexture* pTexture) {
	// TODO: Implement editor texture data access
	return nullptr;
}

void Rendering::FreeEditorTexture(EditorRenderTexture* pTexture) {
	// TODO: Implement editor texture cleanup
}

void Rendering::RenderEditorTexture(const EditorRenderTexture* pTexture) {
	// TODO: Implement editor texture rendering
}

void Rendering::RenderEditor() {
	// TODO: Implement editor rendering
}
#endif

#endif // USE_OPENGL