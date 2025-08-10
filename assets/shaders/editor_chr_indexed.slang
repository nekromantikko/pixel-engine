[[vk::binding(0, 0)]]
RWTexture2D<uint> outputTexture : register(u0);
[[vk::binding(2, 0)]]
StructuredBuffer<ChrTile> chrBuffer : register(t2);

struct ChrTile {
    uint64_t p0;
	uint64_t p1;
	uint64_t p2;
};

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

[shader("compute")] [numthreads(8, 8, 1)]
void EditorBlitChrIndexed(uint2 dti: SV_DispatchThreadID, uint2 gi: SV_GroupID, uint2 gti: SV_GroupThreadID) {
    uint sheetIndex = gi.y / 16;
    uint2 tileCoord = gi % 16;
    uint tileIndex = tileCoord.x + tileCoord.y * 16;
    uint pixelOffset = gti.x + gti.y * 8;
    uint8_t colorIndex = ReadChrTile(sheetIndex, tileIndex, pixelOffset, false, false);
    
    // Output indexed color value (0-7) instead of final RGB
    outputTexture[dti] = colorIndex;
}