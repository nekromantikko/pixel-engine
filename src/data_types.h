#pragma once
#include "typedef.h"

enum DataType: u32 {
	DATA_TYPE_S8,
	DATA_TYPE_U8,
	DATA_TYPE_S16,
	DATA_TYPE_U16,
	DATA_TYPE_S32,
	DATA_TYPE_U32,
	DATA_TYPE_S64,
	DATA_TYPE_U64,
	DATA_TYPE_R32,
	DATA_TYPE_R64,
	DATA_TYPE_BOOL,

	DATA_TYPE_COUNT
};

struct DataTypeInfo {
	const char* name;
	size_t size;
};

inline const DataTypeInfo* GetDataTypeInfo(DataType type) {
	static const DataTypeInfo dataTypeInfos[DATA_TYPE_COUNT] = {
		{"S8", 1}, {"U8", 1}, {"S16", 2}, {"U16", 2},
		{"S32", 4}, {"U32", 4}, {"S64", 8}, {"U64", 8},
		{"R32", 4}, {"R64", 8}, {"BOOL", 1}
	};

	if (type >= DATA_TYPE_COUNT) {
		return nullptr; // Invalid type
	}

	return &dataTypeInfos[type];
}
