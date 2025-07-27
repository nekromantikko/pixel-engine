#pragma once

#ifdef EDITOR
#include "editor_actor.h"
#define ACTOR_SUBTYPE_PROPERTY_SCALAR(STRUCT_TYPE, FIELD, TYPE, COMPONENTS) \
	{ .name = #FIELD, .type = ACTOR_EDITOR_PROPERTY_SCALAR, .dataType = ImGuiDataType_##TYPE, .components = COMPONENTS, .offset = offsetof(STRUCT_TYPE, FIELD) }

#define ACTOR_SUBTYPE_PROPERTY_ASSET(STRUCT_TYPE, FIELD, ASSET_TYPE, COMPONENTS) \
	{ .name = #FIELD, .type = ACTOR_EDITOR_PROPERTY_ASSET, .assetType = ASSET_TYPE, .components = COMPONENTS, .offset = offsetof(STRUCT_TYPE, FIELD) }

#define ACTOR_SUBTYPE_PROPERTIES(STRUCT_TYPE, ...) \
	inline const std::vector<ActorEditorProperty>& Get##STRUCT_TYPE##EditorProperties() { \
		static const std::vector<ActorEditorProperty> properties = { __VA_ARGS__ }; \
		return properties; \
	} \

#define GET_SUBTYPE_PROPERTIES(STRUCT_TYPE) \
		Get##STRUCT_TYPE##EditorProperties()

#define DECLARE_ACTOR_EDITOR_DATA(ACTOR_TYPE) \
	namespace Editor { \
		extern const ActorEditorData ACTOR_TYPE##EditorData; \
	} \

#define DEFINE_ACTOR_EDITOR_DATA(ACTOR_TYPE, ...) \
	namespace Editor { \
		const ActorEditorData ACTOR_TYPE##EditorData = ActorEditorData({ __VA_ARGS__ }); \
	} \

#else
#define ACTOR_SUBTYPE_PROPERTY_SCALAR(STRUCT_TYPE, FIELD, TYPE, COMPONENTS)
#define ACTOR_SUBTYPE_PROPERTY_ASSET(STRUCT_TYPE, FIELD, ASSET_TYPE, COMPONENTS)
#define ACTOR_SUBTYPE_PROPERTIES(STRUCT_TYPE, ...)
#define GET_SUBTYPE_PROPERTIES(STRUCT_TYPE) \
		{}
#define DECLARE_ACTOR_EDITOR_DATA(ACTOR_TYPE)
#define DEFINE_ACTOR_EDITOR_DATA(ACTOR_TYPE, ...)
#endif