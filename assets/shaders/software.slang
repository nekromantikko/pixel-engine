static let g_nametableSizePixels = uint2(NAMETABLE_DIM_PIXELS, NAMETABLE_DIM_PIXELS);

struct Sprite {
    int16_t y;
    int16_t x;
    uint16_t tileId : 10;
    uint16_t palette : 3;
    uint16_t priority : 1;
    uint16_t flipHorizontal : 1;
    uint16_t flipVertical : 1;
};

struct Scanline {
    int2 scroll;
};

struct ChrTile {
    uint64_t p0;
	uint64_t p1;
	uint64_t p2;
};

struct BgTile {
    uint16_t tileId : 10;
    uint16_t palette : 3;
    uint16_t unused : 1;
    uint16_t flipHorizontal : 1;
    uint16_t flipVertical : 1;
};

[[vk::binding(0, 0)]]
RWTexture2D<float4> outputTexture : register(u0);
[[vk::binding(1, 0)]]
Texture1D<float4> paletteTexture : register(t1);
[[vk::binding(2, 0)]]
StructuredBuffer<ChrTile> chrBuffer : register(t2);
[[vk::binding(3, 0)]]
StructuredBuffer<uint8_t> paletteBuffer : register(t3);
[[vk::binding(4, 0)]]
StructuredBuffer<BgTile> nametableBuffer : register(t4);
[[vk::binding(5, 0)]]
StructuredBuffer<Sprite> spriteBuffer : register(t5);
[[vk::binding(6, 0)]]
StructuredBuffer<Scanline> scanlineBuffer : register(t6);
[[vk::binding(7, 0)]]
RWByteAddressBuffer evaluatedScanlineSpriteIndicesBuffer : register(u7);

uint8_t ReadChrTile(uint chrPage, uint tileIndex, uint offset, bool flipX, bool flipY) {
    if (flipX) offset ^= 7;
    if (flipY) offset ^= 56;

    uint tileOffset = CHR_SIZE_TILES * chrPage + tileIndex;
    ChrTile tile = chrBuffer.Load(tileOffset);
    uint8_t colorIndex = uint8_t((tile.p0 >> offset) & 1);
    colorIndex |= uint8_t(((tile.p1 >> offset) & 1) << 1);
    colorIndex |= uint8_t(((tile.p2 >> offset) & 1) << 2);

    return colorIndex;
}

uint2 Scroll(in Scanline scanline, uint2 screenPos, out uint nametableIndex) {
    uint2 scrollPos = screenPos;
    scrollPos += scanline.scroll;
    uint2 nametableIndex2D = scrollPos / g_nametableSizePixels;
    nametableIndex = (nametableIndex2D.x + nametableIndex2D.y) % 2;
    return scrollPos % g_nametableSizePixels;
}

uint2 RenderBackground(uint2 scrollPos, uint nametableIndex) {
    uint2 tilePos = scrollPos / 8;                   // Position of tile
    uint tileIndex = tilePos.x + tilePos.y * 64;    // Should be 12 bits (yyyyyyxxxxxx)
    uint2 pixelPos = scrollPos % 8;                  // Position of pixel within tile
    uint pixelIndex = pixelPos.x + pixelPos.y * 8;   // Should be 6 bits (yyyxxx)

    uint tileOffset = NAMETABLE_SIZE_TILES * nametableIndex + tileIndex;
    BgTile bgTile = nametableBuffer.Load(tileOffset);

    uint chrPage = (bgTile.tileId >> 8) & 3;
    uint chrTileIndex = bgTile.tileId & 0xFF;
    uint8_t colorIndex = ReadChrTile(chrPage, chrTileIndex, pixelIndex, bgTile.flipHorizontal != 0, bgTile.flipVertical != 0);

    if (colorIndex == 0) {
        return int2(0, 0);
    }

    return uint2(colorIndex, bgTile.palette);
}

void ProcessSprite(uint spriteIndex, uint2 screenPos, inout uint minSpriteIndex, inout uint2 background) {
    if (minSpriteIndex < spriteIndex) {
        return; // Already found a sprite with a lower index
    }

    Sprite sprite = spriteBuffer.Load(spriteIndex);
    // Early return if sprite is not touching this pixel
    if (screenPos.x < sprite.x || screenPos.x >= sprite.x + 8) {
		return;
	}

    uint2 offset2D = screenPos - int2(sprite.x, sprite.y);
    uint offset = offset2D.x + offset2D.y * 8;

    uint chrPage = (sprite.tileId >> 8) & 3;
    uint tileIndex = sprite.tileId & 0xFF;
    uint8_t colorIndex = ReadChrTile(chrPage + 4, tileIndex, offset, sprite.flipHorizontal != 0, sprite.flipVertical != 0);

    if (colorIndex == 0) {
		return; // Transparent pixel
	}

    // This is the highest priority sprite drawn so far
    minSpriteIndex = spriteIndex;

    // Draw sprites on top of bg if priority is 0 or if the bg pixel is not opaque
    if (sprite.priority == 0 || background.x == 0) {
        background.x = colorIndex;
        background.y = sprite.palette + 8;
    }
}

uint GetScanlineEvaluatedSpriteCountOffset(uint scanlineIndex) {
    return (MAX_SPRITES_PER_SCANLINE + 1) * scanlineIndex * sizeof(uint);
}

uint GetScanlineEvaluatedSpriteCount(uint scanlineIndex) {
    uint byteOffset = GetScanlineEvaluatedSpriteCountOffset(scanlineIndex);
    uint spriteCount = evaluatedScanlineSpriteIndicesBuffer.Load(byteOffset);
    return min(spriteCount, MAX_SPRITES_PER_SCANLINE);
}

uint GetScanlineEvaluatedSpriteIndexOffset(uint scanlineIndex, uint spriteIndex) {
    uint scanlineOffset = GetScanlineEvaluatedSpriteCountOffset(scanlineIndex);
    return scanlineOffset + sizeof(uint) + spriteIndex * sizeof(uint);
}

uint GetScanlineEvaluatedSpriteIndex(uint scanlineIndex, uint spriteIndex) {
    uint byteOffset = GetScanlineEvaluatedSpriteIndexOffset(scanlineIndex, spriteIndex);
	return evaluatedScanlineSpriteIndicesBuffer.Load(byteOffset);
}

void RenderSprites(uint2 screenPos, inout uint2 background) {
    uint scanlineIndex = screenPos.y;
    uint spriteCount = GetScanlineEvaluatedSpriteCount(scanlineIndex);

    uint minSpriteIndex = ~0u;

    for (uint i = 0; i < spriteCount; i++) {
        uint spriteIndex = GetScanlineEvaluatedSpriteIndex(scanlineIndex, i);
        ProcessSprite(spriteIndex, screenPos, minSpriteIndex, background);
    }
}

float3 RenderDebugGrid(uint2 scrollPos, float3 background) {
    if (scrollPos.x % 16 == 0 || scrollPos.y % 16 == 0) {
        return lerp(background, float3(0.0, 0.0, 1.0), 0.5);
    }
    else if (scrollPos.x % 8 == 0 || scrollPos.y % 8 == 0) {
        return lerp(background, float3(0.0, 0.0, 1.0), 0.25);
    }
    return background;
}

float3 ReadPaletteColor(uint paletteIndex, uint colorIndex) {
    uint paletteOffset = paletteIndex * PALETTE_COLOR_COUNT + colorIndex;
    uint8_t colorIndexValue = paletteBuffer.Load(paletteOffset);
    return paletteTexture.Load(int2(colorIndexValue, 0)).rgb;
}

[shader("compute")] [numthreads(32, 32, 1)]
void SoftwareRenderMain(uint2 screenPos: SV_DispatchThreadID) {
    Scanline scanline = scanlineBuffer.Load(screenPos.y);
    uint nametableIndex;
    uint2 scrollPos = Scroll(scanline, screenPos, nametableIndex);

    uint2 result = RenderBackground(scrollPos, nametableIndex);
    RenderSprites(screenPos, result);

    float3 finalColor = ReadPaletteColor(result.y, result.x);
    outputTexture[screenPos] = float4(finalColor, 1.0f);
}

[shader("compute")] [numthreads(8, 1, 1)]
void EditorBlitPalette(uint2 dti: SV_DispatchThreadID, uint2 gi: SV_GroupID, uint2 gti: SV_GroupThreadID) {
    uint palette = gi.x;
    uint paletteColorIndex = gti.x;
    palette *= sign(paletteColorIndex % 8); // BG is always color 0
    float3 finalColor = ReadPaletteColor(palette, paletteColorIndex);
    outputTexture[dti] = float4(finalColor, 1.0f);
}

[shader("compute")] [numthreads(8, 8, 1)]
void EditorBlitChr(uint2 dti: SV_DispatchThreadID, uint2 gi: SV_GroupID, uint2 gti: SV_GroupThreadID) {
    uint sheetIndex = gi.y / 16;
    uint setIndex = sheetIndex / 4;
    uint2 tileCoord = gi % 16;
    uint tileIndex = tileCoord.x + tileCoord.y * 16;
    uint palette = gi.x / 16 + setIndex * 8;
    uint pixelOffset = gti.x + gti.y * 8;
    uint8_t colorIndex = ReadChrTile(sheetIndex, tileIndex, pixelOffset, false, false);
    float3 finalColor = ReadPaletteColor(palette, colorIndex);
    float alpha = colorIndex == 0 ? 0.0f : 1.0f;
    outputTexture[dti] = float4(finalColor, alpha);
}

[shader("compute")] [numthreads(MAX_SPRITES_PER_SCANLINE, 1, 1)]
void ScanlineEvaluate(uint2 dti: SV_DispatchThreadID, uint2 gi: SV_GroupID) {
    uint scanlineIndex = gi.y;
    uint globalSpriteIndex = dti.x;

    Sprite sprite = spriteBuffer.Load(globalSpriteIndex);

    if (scanlineIndex >= sprite.y && scanlineIndex < sprite.y + 8) {
        uint spriteCountOffset = GetScanlineEvaluatedSpriteCountOffset(scanlineIndex);
        uint scanlineSpriteIndex;
        evaluatedScanlineSpriteIndicesBuffer.InterlockedAdd(spriteCountOffset, 1, scanlineSpriteIndex);
        if (scanlineSpriteIndex < MAX_SPRITES_PER_SCANLINE) {
            uint spriteIndexOffset = GetScanlineEvaluatedSpriteIndexOffset(scanlineIndex, scanlineSpriteIndex);
            evaluatedScanlineSpriteIndicesBuffer.Store(spriteIndexOffset, globalSpriteIndex);
		}
	}
}