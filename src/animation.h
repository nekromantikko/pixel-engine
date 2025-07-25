#pragma once
#include "typedef.h"
#include "asset_types.h"

constexpr u32 ANIMATION_MAX_FRAME_COUNT = 64;

struct AnimationFrame {
	MetaspriteHandle metaspriteId;
};

struct Animation {
	u8 frameLength;
	s16 loopPoint;
	u16 frameCount;
	AnimationFrame frames[ANIMATION_MAX_FRAME_COUNT];
};

#ifdef EDITOR
#include <nlohmann/json.hpp>

static void from_json(const nlohmann::json& j, AnimationFrame& frame) {
	j.at("metasprite_id").get_to(frame.metaspriteId.id);
}

static void to_json(nlohmann::json& j, const AnimationFrame& frame) {
	j["metasprite_id"] = frame.metaspriteId.id;
}

static void from_json(const nlohmann::json& j, Animation& anim) {
	j.at("frame_length").get_to(anim.frameLength);
	j.at("loop_point").get_to(anim.loopPoint);
	j.at("frame_count").get_to(anim.frameCount);
	for (u32 i = 0; i < anim.frameCount; ++i) {
		anim.frames[i] = j.at("frames").at(i).get<AnimationFrame>();
	}
}

static void to_json(nlohmann::json& j, const Animation& anim) {
	j["frame_length"] = anim.frameLength;
	j["loop_point"] = anim.loopPoint;
	j["frame_count"] = anim.frameCount;
	for (u32 i = 0; i < anim.frameCount; ++i) {
		j["frames"].push_back(anim.frames[i]);
	}
}

#endif