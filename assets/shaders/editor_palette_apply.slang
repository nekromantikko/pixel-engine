struct VSOutput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
}

struct PushConstants {
    uint paletteIndex;
};

[[vk_push_constant]] PushConstants pushConstants;

[[vk::binding(0)]]
Texture2D<uint> indexedTexture : register(t0);
[[vk::binding(1)]]
Texture1D<float4> paletteTexture : register(t1);
[[vk::binding(2)]]
StructuredBuffer<uint8_t> paletteBuffer : register(t2);

[shader("vertex")]
VSOutput Vertex(uint vertexID: SV_VertexID) {
    VSOutput output;
    
    // Hardcoded full-screen quad vertices
    float2 positions[4] = {
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
        float2(-1.0,  1.0),
        float2( 1.0,  1.0)
    };
    
    float2 uv[4] = {
        float2(0.0, 1.0),
        float2(1.0, 1.0),
        float2(0.0, 0.0),
        float2(1.0, 0.0)
    };

    output.position = float4(positions[vertexID], 0.0, 1.0);
    output.texCoord = uv[vertexID];

    return output;
}

[shader("pixel")]
float4 PaletteApply(VSOutput input) : SV_Target0 {
    // Sample the indexed texture to get color index
    uint colorIndex = indexedTexture.Sample(input.texCoord).r;
    
    // Skip transparent pixels
    if (colorIndex == 0) {
        discard;
    }
    
    // Get the final color from palette
    uint paletteOffset = pushConstants.paletteIndex * PALETTE_COLOR_COUNT + colorIndex;
    uint8_t colorIndexValue = paletteBuffer.Load(paletteOffset);
    float3 finalColor = paletteTexture.Load(int2(colorIndexValue, 0)).rgb;
    
    return float4(finalColor, 1.0);
}