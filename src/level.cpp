#include "level.h"
#include <stdio.h>

void LoadLevel(Level* pLevel, const char* fname)
{
    FILE* pFile;
    fopen_s(&pFile, fname, "rb");

    if (pFile == NULL) {
        ERROR("Failed to load level file\n");
    }

    const char signature[4]{};
    fread((void*)signature, sizeof(u8), 4, pFile);
    u32 screenCount;
    fread(&screenCount, sizeof(u32), 1, pFile);
    Screen* screens = (Screen*)calloc(screenCount, sizeof(Screen));
    fread((void*)screens, sizeof(Screen), screenCount, pFile);

    pLevel->name = fname;
    pLevel->screenCount = screenCount;
    pLevel->screens = screens;

    fclose(pFile);
}

void SaveLevel(Level* pLevel, const char* fname)
{
    FILE* pFile;
    fopen_s(&pFile, fname, "wb");

    if (pFile == NULL) {
        ERROR("Failed to write level file\n");
    }

    const char signature[4] = "LEV";
    fwrite(signature, sizeof(u8), 4, pFile);
    fwrite(&pLevel->screenCount, sizeof(u32), 1, pFile);
    fwrite(pLevel->screens, sizeof(Screen), pLevel->screenCount, pFile);

    fclose(pFile);
}