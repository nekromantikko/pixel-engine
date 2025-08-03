#pragma once

#ifdef EDITOR
#include "editor_actor.h"
#define ACTOR_SUBTYPE_PROPERTY_SCALAR(STRUCT_TYPE, FIELD, TYPE, COMPONENTS) \
	{ .name = #FIELD, .type = ACTOR_EDITOR_PROPERTY_SCALAR, .dataType = DATA_TYPE_##TYPE, .components = COMPONENTS, .offset = offsetof(STRUCT_TYPE, FIELD) }

#define ACTOR_SUBTYPE_PROPERTY_ASSET(STRUCT_TYPE, FIELD, ASSET_TYPE, COMPONENTS) \
	{ .name = #FIELD, .type = ACTOR_EDITOR_PROPERTY_ASSET, .assetType = ASSET_TYPE, .components = COMPONENTS, .offset = offsetof(STRUCT_TYPE, FIELD) }

#define ACTOR_SUBTYPE_PROPERTIES(STRUCT_TYPE, ...) \
	inline static const std::vector<ActorEditorProperty>& Get##STRUCT_TYPE##EditorProperties() { \
		static const std::vector<ActorEditorProperty> properties = { __VA_ARGS__ }; \
		return properties; \
	} \

#define GET_SUBTYPE_PROPERTIES(STRUCT_TYPE) \
		Get##STRUCT_TYPE##EditorProperties()

#define ACTOR_EDITOR_DATA(ACTOR_TYPE, ...) \
	inline static const ActorEditorData Get##ACTOR_TYPE##EditorData() { \
		static const ActorEditorData editorData = ActorEditorData({ __VA_ARGS__ }); \
		return editorData; \
	} \

#else
#define ACTOR_SUBTYPE_PROPERTY_SCALAR(STRUCT_TYPE, FIELD, TYPE, COMPONENTS)
#define ACTOR_SUBTYPE_PROPERTY_ASSET(STRUCT_TYPE, FIELD, ASSET_TYPE, COMPONENTS)
#define ACTOR_SUBTYPE_PROPERTIES(STRUCT_TYPE, ...)
#define GET_SUBTYPE_PROPERTIES(STRUCT_TYPE) \
		{}
#define ACTOR_EDITOR_DATA(ACTOR_TYPE, ...)
#endif