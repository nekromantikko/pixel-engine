#pragma once
#include "data_types.h"
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
		DataType dataType;
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

	inline const u32 GetSubtypeCount() const {
		return subtypeNames.size();
	}
	inline const char* const* GetSubtypeNames() const {
		return subtypeNames.data();
	}
	inline u32 GetPropertyCount(u32 subtype) const {
		return subtypeProperties[subtype]->size();
	}
	inline const ActorEditorProperty& GetProperty(u32 subtype, u32 index) const {
		return subtypeProperties[subtype]->at(index);
	}
};