#pragma once
#include "imgui.h"
#include "asset_types.h"
#include <vector>

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
	typedef std::pair<const char*, const std::vector<ActorEditorProperty>&> SubtypePropertyPair;

	std::vector<const char*> subtypeNames;
	std::vector<const std::vector<ActorEditorProperty>*> subtypeProperties;
public:
	ActorEditorData(std::vector<SubtypePropertyPair>&& subtypePropertyPairs)
		: subtypeNames(subtypePropertyPairs.size()), subtypeProperties(subtypePropertyPairs.size()) {

		for (size_t i = 0; i < subtypePropertyPairs.size(); ++i) {
			subtypeNames[i] = subtypePropertyPairs[i].first;
			subtypeProperties[i] = &subtypePropertyPairs[i].second;
		}
	}

	const u32 GetSubtypeCount() const {
		return subtypeNames.size();
	}
	const char* const* GetSubtypeNames() const {
		return subtypeNames.data();
	}
	u32 GetPropertyCount(u32 subtype) const {
		return subtypeProperties[subtype]->size();
	}
	const ActorEditorProperty& GetProperty(u32 subtype, u32 index) const {
		return subtypeProperties[subtype]->at(index);
	}
};