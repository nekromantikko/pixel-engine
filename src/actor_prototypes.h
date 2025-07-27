#pragma once
#include "typedef.h"
#include "collision.h"
#include "actor_types.h"
#include "asset_types.h"

static constexpr u32 ACTOR_PROTOTYPE_MAX_ANIMATION_COUNT = 64;

struct ActorPrototype {
	TActorType type;
	TActorSubtype subtype;

	AABB hitbox;

	u32 animCount;
	AnimationHandle animations[ACTOR_PROTOTYPE_MAX_ANIMATION_COUNT];

	ActorPrototypeData data;
};

#ifdef EDITOR
#include <nlohmann/json.hpp>
#include <imgui_internal.h>
#include <sstream>

inline void to_json(nlohmann::json& j, const AABB& aabb) {
	j["min_x"] = aabb.min.x;
	j["min_y"] = aabb.min.y;
	j["max_x"] = aabb.max.x;
	j["max_y"] = aabb.max.y;
}

inline void from_json(const nlohmann::json& j, AABB& aabb) {
	aabb.min.x = j.at("min_x").get<r32>();
	aabb.min.y = j.at("min_y").get<r32>();
	aabb.max.x = j.at("max_x").get<r32>();
	aabb.max.y = j.at("max_y").get<r32>();
}

inline void SerializeScalarActorProperty(nlohmann::json& j_properties, const ActorEditorProperty& prop, void* pData) {
	if (prop.components == 0) {
		j_properties[prop.name] = nullptr;
		return;
	}

	const ImGuiDataTypeInfo* pTypeInfo = ImGui::DataTypeGetInfo(prop.dataType);
	std::stringstream ss;
	char buf[64];
	ss << "[";
	for (s32 i = 0; i < prop.components; i++) {
		ImGui::DataTypeFormatString(buf, IM_ARRAYSIZE(buf), prop.dataType, pData, pTypeInfo->PrintFmt);
		ss << buf;
		if (i < prop.components - 1) {
			ss << ", ";
		}
		pData = (void*)((char*)pData + pTypeInfo->Size);
	}
	ss << "]";
	nlohmann::json arrayJson = nlohmann::json::parse(ss.str());
	if (prop.components == 1) {
		j_properties[prop.name] = arrayJson[0];
	}
	else {
		j_properties[prop.name] = arrayJson;
	}
}

inline void to_json(nlohmann::json& j, const ActorPrototype& prototype) {
	const ActorEditorData& editorData = Editor::actorEditorData[prototype.type];

	j["type"] = (ActorType)prototype.type;
	j["subtype"] = editorData.GetSubtypeNames()[prototype.subtype];
	j["hitbox"] = prototype.hitbox;
	j["animation_ids"] = nlohmann::json::array();
	for (u32 i = 0; i < prototype.animCount; ++i) {
		j["animation_ids"].push_back(prototype.animations[i].id);
	}
	u32 propertyCount = editorData.GetPropertyCount(prototype.subtype);
	j["properties"] = nlohmann::json::object();
	for (u32 i = 0; i < propertyCount; ++i) {
		const ActorEditorProperty& prop = editorData.GetProperty(prototype.subtype, i);
		void* propertyData = (u8*)&prototype.data + prop.offset;

		switch (prop.type) {
		case ACTOR_EDITOR_PROPERTY_SCALAR: {
			SerializeScalarActorProperty(j["properties"], prop, propertyData);
			break;
		}
		case ACTOR_EDITOR_PROPERTY_ASSET: {
			u64& id = *(u64*)propertyData;
			j["properties"][prop.name] = id;
			break;
		}
		default:
			break;
		}
	}
}

inline void from_json(const nlohmann::json& j, ActorPrototype& prototype) {
	// TODO
}

#endif