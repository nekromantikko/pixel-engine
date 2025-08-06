#pragma once
#include "asset_archive.h"
#include <cstdarg>
#include <filesystem>

struct SDL_Window;
union SDL_Event;

namespace Editor {
	void CreateContext(AssetArchive *pAssetArchive);
	void Init(SDL_Window* pWindow);
	void Free();
	void DestroyContext();

	void ConsoleLog(const char* fmt, va_list args);
	void ClearLog();

	void ProcessEvent(const SDL_Event* event);
	void Update(r64 dt);
	void SetupFrame();
	void Render();

	namespace Assets {
		bool LoadSourceAssetsFromDirectory(const std::filesystem::path& directory, AssetArchive* pArchive);
		void InitializeAsset(AssetType type, void* pData);
		u32 GetAssetSize(AssetType type, const void* pData);
	}
}