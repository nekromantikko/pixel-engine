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
#include "core_types.h"
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

// Forward declarations
static void LoadShaders();

// Helper function to pad buffer size for alignment  
static constexpr u32 PadBufferSize(u32 originalSize, const u32 minAlignment) {
	return (originalSize + minAlignment - 1) & ~(minAlignment - 1);
}

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
	// Calculate buffer layout - same as Vulkan implementation
	const u32 minOffsetAlignment = 256; // OpenGL standard buffer alignment
	
	pContext->paletteTableOffset = 0;
	pContext->paletteTableSize = PadBufferSize(PALETTE_COUNT * sizeof(Palette), minOffsetAlignment);
	pContext->chrOffset = pContext->paletteTableOffset + pContext->paletteTableSize;
	pContext->chrSize = PadBufferSize(CHR_MEMORY_SIZE, minOffsetAlignment);
	pContext->nametableOffset = pContext->chrOffset + pContext->chrSize;
	pContext->nametableSize = PadBufferSize(sizeof(Nametable) * NAMETABLE_COUNT, minOffsetAlignment);
	pContext->oamOffset = pContext->nametableOffset + pContext->nametableSize;
	pContext->oamSize = PadBufferSize(MAX_SPRITE_COUNT * sizeof(Sprite), minOffsetAlignment);
	pContext->renderStateOffset = pContext->oamOffset + pContext->oamSize;
	pContext->renderStateSize = PadBufferSize(sizeof(Scanline) * SCANLINE_COUNT, minOffsetAlignment);
	pContext->computeBufferSize = pContext->paletteTableSize + pContext->chrSize + pContext->nametableSize + pContext->oamSize + pContext->renderStateSize;

	pContext->renderData = calloc(1, pContext->computeBufferSize);

	// Create OpenGL buffers
	for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		glGenBuffers(1, &pContext->computeBuffer[i].buffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, pContext->computeBuffer[i].buffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, pContext->computeBufferSize, nullptr, GL_DYNAMIC_DRAW);
		pContext->computeBuffer[i].size = pContext->computeBufferSize;

		glGenBuffers(1, &pContext->scanlineBuffers[i].buffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, pContext->scanlineBuffers[i].buffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ScanlineData) * SCANLINE_COUNT, nullptr, GL_DYNAMIC_DRAW);
		pContext->scanlineBuffers[i].size = sizeof(ScanlineData) * SCANLINE_COUNT;
	}

	// Create color images
	for (u32 i = 0; i < COMMAND_BUFFER_COUNT; i++) {
		glGenTextures(1, &pContext->colorImages[i].texture);
		glBindTexture(GL_TEXTURE_2D, pContext->colorImages[i].texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 288, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		pContext->colorImages[i].width = 512;
		pContext->colorImages[i].height = 288;
	}

	// Create palette texture
	glGenTextures(1, &pContext->paletteTexture);
	glBindTexture(GL_TEXTURE_1D, pContext->paletteTexture);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

	// Load and compile shaders
	LoadShaders();

	// Create VAO for fullscreen quad
	glGenVertexArrays(1, &pContext->quadVAO);

	pContext->currentFrameIndex = 0;
	pContext->settings = DEFAULT_RENDER_SETTINGS;

	DEBUG_LOG("OpenGL rendering backend initialized\n");
}

static void LoadShaders() {
	// Load and compile compute shader
	char* softwareSource = LoadShaderSource("src/shaders/opengl/software.comp");
	if (softwareSource) {
		GLuint computeShader = CompileShader(GL_COMPUTE_SHADER, softwareSource);
		if (computeShader) {
			pContext->softwareComputeProgram = glCreateProgram();
			glAttachShader(pContext->softwareComputeProgram, computeShader);
			glLinkProgram(pContext->softwareComputeProgram);

			GLint linked;
			glGetProgramiv(pContext->softwareComputeProgram, GL_LINK_STATUS, &linked);
			if (!linked) {
				GLint logLength;
				glGetProgramiv(pContext->softwareComputeProgram, GL_INFO_LOG_LENGTH, &logLength);
				char* log = (char*)malloc(logLength);
				glGetProgramInfoLog(pContext->softwareComputeProgram, logLength, nullptr, log);
				DEBUG_ERROR("Compute program linking failed: %s\n", log);
				free(log);
			}
			glDeleteShader(computeShader);
		}
		free(softwareSource);
	}

	// Load vertex and fragment shaders
	char* vertexSource = LoadShaderSource("src/shaders/opengl/quad.vert");
	char* rawFragmentSource = LoadShaderSource("src/shaders/opengl/textured_raw.frag");
	char* crtFragmentSource = LoadShaderSource("src/shaders/opengl/textured_crt.frag");

	if (vertexSource && rawFragmentSource) {
		pContext->quadVertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
		pContext->rawFragmentShader = CompileShader(GL_FRAGMENT_SHADER, rawFragmentSource);
		
		if (pContext->quadVertexShader && pContext->rawFragmentShader) {
			pContext->rawProgram = LinkProgram(pContext->quadVertexShader, pContext->rawFragmentShader);
		}
	}

	if (vertexSource && crtFragmentSource) {
		if (!pContext->quadVertexShader) {
			pContext->quadVertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
		}
		pContext->crtFragmentShader = CompileShader(GL_FRAGMENT_SHADER, crtFragmentSource);
		
		if (pContext->quadVertexShader && pContext->crtFragmentShader) {
			pContext->crtProgram = LinkProgram(pContext->quadVertexShader, pContext->crtFragmentShader);
		}
	}

	if (vertexSource) free(vertexSource);
	if (rawFragmentSource) free(rawFragmentSource);
	if (crtFragmentSource) free(crtFragmentSource);
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
	u32 currentFrame = pContext->currentFrameIndex;
	
	// Update compute buffer with render data
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, pContext->computeBuffer[currentFrame].buffer);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, pContext->computeBufferSize, pContext->renderData);

	// Bind compute shader and buffers
	glUseProgram(pContext->softwareComputeProgram);
	
	// Bind output image
	glBindImageTexture(0, pContext->colorImages[currentFrame].texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
	
	// Bind palette texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, pContext->paletteTexture);
	glUniform1i(glGetUniformLocation(pContext->softwareComputeProgram, "palette"), 1);
	
	// Bind shader storage buffers
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, pContext->computeBuffer[currentFrame].buffer);
	glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 3, pContext->computeBuffer[currentFrame].buffer, 
					  pContext->paletteTableOffset, pContext->paletteTableSize);
	glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 4, pContext->computeBuffer[currentFrame].buffer,
					  pContext->nametableOffset, pContext->nametableSize);
	glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 5, pContext->computeBuffer[currentFrame].buffer,
					  pContext->oamOffset, pContext->oamSize);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, pContext->scanlineBuffers[currentFrame].buffer);

	// Dispatch compute shader
	glDispatchCompute(16, 9, 1); // 512/32 = 16, 288/32 = 9
	
	// Wait for compute to finish
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// Render fullscreen quad
	GLuint program = pContext->settings.useCRTFilter ? pContext->crtProgram : pContext->rawProgram;
	glUseProgram(program);
	
	glBindVertexArray(pContext->quadVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, pContext->colorImages[currentFrame].texture);
	glUniform1i(glGetUniformLocation(program, "colorSampler"), 0);
	
	// Set quad uniform (fullscreen)
	glUniform4f(glGetUniformLocation(program, "quad"), 0.0f, 0.0f, 2.0f, 2.0f);
	
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
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