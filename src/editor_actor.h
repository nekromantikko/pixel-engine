#pragma once
#include "imgui.h"
#include "asset_types.h"
#include <vector>
#include <cassert>
		

enum ActorEditorPropertyType {
	ACTOR_EDITOR_PROPERTY_SCALAR,
	ACTOR_EDITOR_PROPERTY_ASSET
};

struct ActorEditorProperty {
	const char* name;
	ActorEditorPropertyType type;
	union {
		ImGuiDataType_ dataType;
		AssetType assetType;
	};
	s32 components;
	u32 offset;
};

class ActorEditorData {
private:
	const std::vector<const char*> subtypeNames;
	const std::vector<std::vector<ActorEditorProperty>> subtypeProperties;
public:
	ActorEditorData(const std::vector<const char*>& names, const std::vector<std::vector<ActorEditorProperty>>& props) : subtypeNames(names), subtypeProperties(props) {
		assert(subtypeNames.size() == subtypeProperties.size());
	}

	const u32 GetSubtypeCount() const {
		return subtypeNames.size();
	}
	const char* const* GetSubtypeNames() const {
		return subtypeNames.data();
	}
	u32 GetPropertyCount(u32 subtype) const {
		return subtypeProperties[subtype].size();
	}
	const ActorEditorProperty& GetProperty(u32 subtype, u32 index) const {
		return subtypeProperties[subtype][index];
	}
};