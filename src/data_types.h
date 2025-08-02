#pragma once
#include "typedef.h"

// Custom data type enumeration to replace ImGuiDataType
enum DataType {
    DATA_TYPE_S8,
    DATA_TYPE_U8,
    DATA_TYPE_S16,
    DATA_TYPE_U16,
    DATA_TYPE_S32,
    DATA_TYPE_U32,
    DATA_TYPE_S64,
    DATA_TYPE_U64,
    DATA_TYPE_FLOAT,
    DATA_TYPE_DOUBLE,
    DATA_TYPE_BOOL,
    DATA_TYPE_STRING,
    DATA_TYPE_COUNT
};

// Type information structure to replace ImGuiDataTypeInfo
struct DataTypeInfo {
    u32 size;
    const char* name;
};

// Get type information for a given DataType
inline const DataTypeInfo* GetDataTypeInfo(DataType dataType) {
    static const DataTypeInfo typeInfos[DATA_TYPE_COUNT] = {
        { sizeof(s8),  "s8" },      // DATA_TYPE_S8
        { sizeof(u8),  "u8" },      // DATA_TYPE_U8
        { sizeof(s16), "s16" },     // DATA_TYPE_S16
        { sizeof(u16), "u16" },     // DATA_TYPE_U16
        { sizeof(s32), "s32" },     // DATA_TYPE_S32
        { sizeof(u32), "u32" },     // DATA_TYPE_U32
        { sizeof(s64), "s64" },     // DATA_TYPE_S64
        { sizeof(u64), "u64" },     // DATA_TYPE_U64
        { sizeof(r32), "r32" },     // DATA_TYPE_FLOAT
        { sizeof(r64), "r64" },     // DATA_TYPE_DOUBLE
        { sizeof(bool), "bool" },   // DATA_TYPE_BOOL
        { 0, "string" }             // DATA_TYPE_STRING (not supported for fixed-size data)
    };
    
    if (dataType >= 0 && dataType < DATA_TYPE_COUNT) {
        return &typeInfos[dataType];
    }
    return nullptr;
}

#ifdef EDITOR
// Mapping functions between custom DataType and ImGuiDataType for editor use
#include <imgui.h>

inline ImGuiDataType DataTypeToImGui(DataType dataType) {
    switch (dataType) {
        case DATA_TYPE_S8:     return ImGuiDataType_S8;
        case DATA_TYPE_U8:     return ImGuiDataType_U8;
        case DATA_TYPE_S16:    return ImGuiDataType_S16;
        case DATA_TYPE_U16:    return ImGuiDataType_U16;
        case DATA_TYPE_S32:    return ImGuiDataType_S32;
        case DATA_TYPE_U32:    return ImGuiDataType_U32;
        case DATA_TYPE_S64:    return ImGuiDataType_S64;
        case DATA_TYPE_U64:    return ImGuiDataType_U64;
        case DATA_TYPE_FLOAT:  return ImGuiDataType_Float;
        case DATA_TYPE_DOUBLE: return ImGuiDataType_Double;
        case DATA_TYPE_BOOL:   return ImGuiDataType_COUNT; // Special case - bool handled differently in ImGui
        case DATA_TYPE_STRING: return ImGuiDataType_COUNT; // Not supported
        default:               return ImGuiDataType_COUNT;
    }
}

inline DataType ImGuiToDataType(ImGuiDataType imguiType) {
    switch (imguiType) {
        case ImGuiDataType_S8:     return DATA_TYPE_S8;
        case ImGuiDataType_U8:     return DATA_TYPE_U8;
        case ImGuiDataType_S16:    return DATA_TYPE_S16;
        case ImGuiDataType_U16:    return DATA_TYPE_U16;
        case ImGuiDataType_S32:    return DATA_TYPE_S32;
        case ImGuiDataType_U32:    return DATA_TYPE_U32;
        case ImGuiDataType_S64:    return DATA_TYPE_S64;
        case ImGuiDataType_U64:    return DATA_TYPE_U64;
        case ImGuiDataType_Float:  return DATA_TYPE_FLOAT;
        case ImGuiDataType_Double: return DATA_TYPE_DOUBLE;
        default:                   return DATA_TYPE_COUNT;
    }
}
#endif